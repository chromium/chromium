// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_requests.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/time_util.h"
#include "google_apis/drive/request_util.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace google_apis::drive {
namespace {

// Format of one request in batch uploading request.
const char kBatchUploadRequestFormat[] =
    "%s %s HTTP/1.1\n"
    "Host: %s\n"
    "X-Goog-Upload-Protocol: multipart\n"
    "Content-Type: %s\n"
    "\n";

// Request header for specifying batch upload.
const char kBatchUploadHeader[] = "X-Goog-Upload-Protocol: batch";

// Content type of HTTP request.
const char kHttpContentType[] = "application/http";

// Break line in HTTP message.
const char kHttpBr[] = "\r\n";

// Mime type of multipart mixed.
const char kMultipartMixedMimeTypePrefix[] = "multipart/mixed; boundary=";

// Parses the JSON value to FileResource instance and runs |callback| on the
// UI thread once parsing is done.
// This is customized version of ParseJsonAndRun defined above to adapt the
// remaining response type.
void ParseFileResourceWithUploadRangeAndRun(
    UploadRangeCallback callback,
    const UploadRangeResponse& response,
    std::unique_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  std::unique_ptr<FileResource> file_resource;
  if (value) {
    file_resource = FileResource::CreateFrom(*value);
    if (!file_resource) {
      std::move(callback).Run(
          UploadRangeResponse(PARSE_ERROR, response.start_position_received,
                              response.end_position_received),
          std::unique_ptr<FileResource>());
      return;
    }
  }

  std::move(callback).Run(response, std::move(file_resource));
}

// Attaches |properties| to the |request_body| if |properties| is not empty.
void AttachProperties(const Properties& properties,
                      base::Value::Dict& request_body) {
  if (properties.empty())
    return;

  base::Value::List properties_value;
  for (const auto& property : properties) {
    base::Value::Dict property_value;
    std::string visibility_as_string;
    switch (property.visibility()) {
      case Property::VISIBILITY_PRIVATE:
        visibility_as_string = "PRIVATE";
        break;
      case Property::VISIBILITY_PUBLIC:
        visibility_as_string = "PUBLIC";
        break;
    }
    property_value.Set("visibility", visibility_as_string);
    property_value.Set("key", property.key());
    property_value.Set("value", property.value());
    properties_value.Append(std::move(property_value));
  }
  request_body.Set("properties", base::Value(std::move(properties_value)));
}

// Creates metadata JSON string for multipart uploading.
// All the values are optional. If the value is empty or null, the value does
// not appear in the metadata.
std::string CreateMultipartUploadMetadataJson(
    const std::string& title,
    const std::string& parent_resource_id,
    std::optional<std::string_view> converted_mime_type,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const Properties& properties) {
  base::Value::Dict root;
  if (!title.empty())
    root.Set("title", title);

  // Fill parent link.
  if (!parent_resource_id.empty()) {
    base::Value::List parents;
    parents.Append(google_apis::util::CreateParentValue(parent_resource_id));
    root.Set("parents", base::Value(std::move(parents)));
  }

  if (converted_mime_type.has_value()) {
    root.Set("mimeType", *converted_mime_type);
  }

  if (!modified_date.is_null()) {
    root.Set("modifiedDate",
             google_apis::util::FormatTimeAsString(modified_date));
  }

  if (!last_viewed_by_me_date.is_null()) {
    root.Set("lastViewedByMeDate",
             google_apis::util::FormatTimeAsString(last_viewed_by_me_date));
  }

  AttachProperties(properties, root);
  std::string json_string;
  base::JSONWriter::Write(root, &json_string);
  return json_string;
}

}  // namespace

MultipartHttpResponse::MultipartHttpResponse() = default;

MultipartHttpResponse::~MultipartHttpResponse() = default;

// The |response| must be multipart/mixed format that contains child HTTP
// response of drive batch request.
// https://www.ietf.org/rfc/rfc2046.txt
//
// It looks like:
// --Boundary
// Content-type: application/http
//
// HTTP/1.1 200 OK
// Header of child response
//
// Body of child response
// --Boundary
// Content-type: application/http
//
// HTTP/1.1 404 Not Found
// Header of child response
//
// Body of child response
// --Boundary--
bool ParseMultipartResponse(const std::string& content_type,
                            const std::string& response,
                            std::vector<MultipartHttpResponse>* parts) {
  if (response.empty())
    return false;

  std::string_view content_type_piece(content_type);
  if (!base::StartsWith(content_type_piece, kMultipartMixedMimeTypePrefix)) {
    return false;
  }
  content_type_piece.remove_prefix(
      std::string_view(kMultipartMixedMimeTypePrefix).size());

  if (content_type_piece.empty())
    return false;
  if (content_type_piece[0] == '"') {
    if (content_type_piece.size() <= 2 || content_type_piece.back() != '"')
      return false;

    content_type_piece =
        content_type_piece.substr(1, content_type_piece.size() - 2);
  }

  std::string boundary(content_type_piece);
  const std::string header = "--" + boundary;
  const std::string terminator = "--" + boundary + "--";

  std::vector<std::string_view> lines = base::SplitStringPieceUsingSubstr(
      response, kHttpBr, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  enum {
    STATE_START,
    STATE_PART_HEADER,
    STATE_PART_HTTP_STATUS_LINE,
    STATE_PART_HTTP_HEADER,
    STATE_PART_HTTP_BODY
  } state = STATE_START;

  const std::string kHttpStatusPrefix = "HTTP/1.1 ";
  std::vector<MultipartHttpResponse> responses;
  ApiErrorCode code = PARSE_ERROR;
  std::string body;
  for (const auto& line : lines) {
    if (state == STATE_PART_HEADER && line.empty()) {
      state = STATE_PART_HTTP_STATUS_LINE;
      continue;
    }

    if (state == STATE_PART_HTTP_STATUS_LINE) {
      if (base::StartsWith(line, kHttpStatusPrefix)) {
        int int_code;
        base::StringToInt(
            line.substr(std::string_view(kHttpStatusPrefix).size()), &int_code);
        if (int_code > 0)
          code = static_cast<ApiErrorCode>(int_code);
        else
          code = PARSE_ERROR;
      } else {
        code = PARSE_ERROR;
      }
      state = STATE_PART_HTTP_HEADER;
      continue;
    }

    if (state == STATE_PART_HTTP_HEADER && line.empty()) {
      state = STATE_PART_HTTP_BODY;
      body.clear();
      continue;
    }
    const std::string_view chopped_line =
        base::TrimString(line, " \t", base::TRIM_TRAILING);
    const bool is_new_part = chopped_line == header;
    const bool was_last_part = chopped_line == terminator;
    if (is_new_part || was_last_part) {
      switch (state) {
        case STATE_START:
          break;
        case STATE_PART_HEADER:
        case STATE_PART_HTTP_STATUS_LINE:
          responses.emplace_back();
          responses.back().code = PARSE_ERROR;
          break;
        case STATE_PART_HTTP_HEADER:
          responses.emplace_back();
          responses.back().code = code;
          break;
        case STATE_PART_HTTP_BODY:
          // Drop the last kHttpBr.
          if (!body.empty())
            body.resize(body.size() - 2);
          responses.emplace_back();
          responses.back().code = code;
          responses.back().body.swap(body);
          break;
      }
      if (is_new_part)
        state = STATE_PART_HEADER;
      if (was_last_part)
        break;
    } else if (state == STATE_PART_HTTP_BODY) {
      base::StrAppend(&body, {line, kHttpBr});
    }
  }

  parts->swap(responses);
  return true;
}

Property::Property() : visibility_(VISIBILITY_PRIVATE) {}

Property::~Property() = default;

//============================ DriveApiPartialFieldRequest ====================

DriveApiPartialFieldRequest::DriveApiPartialFieldRequest(RequestSender* sender)
    : DriveUrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()) {
}

DriveApiPartialFieldRequest::~DriveApiPartialFieldRequest() = default;

GURL DriveApiPartialFieldRequest::GetURL() const {
  GURL url = GetURLInternal();
  if (!fields_.empty())
    url = net::AppendOrReplaceQueryParameter(url, "fields", fields_);
  return url;
}

//=============================== FilesGetRequest =============================

FilesGetRequest::FilesGetRequest(RequestSender* sender,
                                 const DriveApiUrlGenerator& url_generator,
                                 FileResourceCallback callback)
    : DriveApiDataRequest<FileResource>(sender, std::move(callback)),
      url_generator_(url_generator) {}

FilesGetRequest::~FilesGetRequest() = default;

GURL FilesGetRequest::GetURLInternal() const {
  return url_generator_.GetFilesGetUrl(file_id_, embed_origin_);
}

//============================ FilesInsertRequest ============================

FilesInsertRequest::FilesInsertRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    FileResourceCallback callback)
    : DriveApiDataRequest<FileResource>(sender, std::move(callback)),
      url_generator_(url_generator),
      visibility_(FILE_VISIBILITY_DEFAULT) {}

FilesInsertRequest::~FilesInsertRequest() = default;

HttpRequestMethod FilesInsertRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool FilesInsertRequest::GetContentData(std::string* upload_content_type,
                                        std::string* upload_content) {
  *upload_content_type = util::kContentTypeApplicationJson;

  base::Value::Dict root;

  if (!last_viewed_by_me_date_.is_null()) {
    root.Set("lastViewedByMeDate",
             util::FormatTimeAsString(last_viewed_by_me_date_));
  }

  if (!mime_type_.empty())
    root.Set("mimeType", mime_type_);

  if (!modified_date_.is_null())
    root.Set("modifiedDate", util::FormatTimeAsString(modified_date_));

  if (!parents_.empty()) {
    base::Value::List parents_value;
    for (const std::string& parent_id : parents_) {
      base::Value::Dict parent;
      parent.Set("id", parent_id);
      parents_value.Append(std::move(parent));
    }
    root.Set("parents", base::Value(std::move(parents_value)));
  }

  if (!title_.empty())
    root.Set("title", title_);

  AttachProperties(properties_, root);
  base::JSONWriter::Write(root, upload_content);

  DVLOG(1) << "FilesInsert data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

GURL FilesInsertRequest::GetURLInternal() const {
  return url_generator_.GetFilesInsertUrl(
      visibility_ == FILE_VISIBILITY_PRIVATE ? "PRIVATE" : "");
}

//============================== FilesPatchRequest ============================

FilesPatchRequest::FilesPatchRequest(RequestSender* sender,
                                     const DriveApiUrlGenerator& url_generator,
                                     FileResourceCallback callback)
    : DriveApiDataRequest<FileResource>(sender, std::move(callback)),
      url_generator_(url_generator),
      set_modified_date_(false),
      update_viewed_date_(true) {}

FilesPatchRequest::~FilesPatchRequest() = default;

HttpRequestMethod FilesPatchRequest::GetRequestType() const {
  return HttpRequestMethod::kPatch;
}

std::vector<std::string> FilesPatchRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

GURL FilesPatchRequest::GetURLInternal() const {
  return url_generator_.GetFilesPatchUrl(file_id_, set_modified_date_,
                                         update_viewed_date_);
}

bool FilesPatchRequest::GetContentData(std::string* upload_content_type,
                                       std::string* upload_content) {
  if (title_.empty() && modified_date_.is_null() &&
      last_viewed_by_me_date_.is_null() && parents_.empty())
    return false;

  *upload_content_type = util::kContentTypeApplicationJson;

  base::Value::Dict root;
  if (!title_.empty())
    root.Set("title", title_);

  if (!modified_date_.is_null())
    root.Set("modifiedDate", util::FormatTimeAsString(modified_date_));

  if (!last_viewed_by_me_date_.is_null()) {
    root.Set("lastViewedByMeDate",
             util::FormatTimeAsString(last_viewed_by_me_date_));
  }

  if (!parents_.empty()) {
    base::Value::List parents_value;
    for (const std::string& parent_id : parents_) {
      base::Value::Dict parent;
      parent.Set("id", parent_id);
      parents_value.Append(std::move(parent));
    }
    root.Set("parents", base::Value(std::move(parents_value)));
  }

  AttachProperties(properties_, root);
  base::JSONWriter::Write(root, upload_content);

  DVLOG(1) << "FilesPatch data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//============================= FilesCopyRequest ==============================

FilesCopyRequest::FilesCopyRequest(RequestSender* sender,
                                   const DriveApiUrlGenerator& url_generator,
                                   FileResourceCallback callback)
    : DriveApiDataRequest<FileResource>(sender, std::move(callback)),
      url_generator_(url_generator),
      visibility_(FILE_VISIBILITY_DEFAULT) {}

FilesCopyRequest::~FilesCopyRequest() = default;

HttpRequestMethod FilesCopyRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

GURL FilesCopyRequest::GetURLInternal() const {
  return url_generator_.GetFilesCopyUrl(
      file_id_, visibility_ == FILE_VISIBILITY_PRIVATE ? "PRIVATE" : "");
}

bool FilesCopyRequest::GetContentData(std::string* upload_content_type,
                                      std::string* upload_content) {
  if (parents_.empty() && title_.empty())
    return false;

  *upload_content_type = util::kContentTypeApplicationJson;

  base::Value::Dict root;

  if (!modified_date_.is_null())
    root.Set("modifiedDate", util::FormatTimeAsString(modified_date_));

  if (!parents_.empty()) {
    base::Value::List parents_value;

    for (const std::string& parent_id : parents_) {
      base::Value::Dict parent;
      parent.Set("id", parent_id);
      parents_value.Append(std::move(parent));
    }
    root.Set("parents", base::Value(std::move(parents_value)));
  }

  if (!title_.empty())
    root.Set("title", title_);

  base::JSONWriter::Write(root, upload_content);
  DVLOG(1) << "FilesCopy data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//========================= TeamDriveListRequest =============================

TeamDriveListRequest::TeamDriveListRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    TeamDriveListCallback callback)
    : DriveApiDataRequest<TeamDriveList>(sender, std::move(callback)),
      url_generator_(url_generator),
      max_results_(30) {}

TeamDriveListRequest::~TeamDriveListRequest() = default;

GURL TeamDriveListRequest::GetURLInternal() const {
  return url_generator_.GetTeamDriveListUrl(max_results_, page_token_);
}

//========================= StartPageTokenRequest =============================

StartPageTokenRequest::StartPageTokenRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    StartPageTokenCallback callback)
    : DriveApiDataRequest<StartPageToken>(sender, std::move(callback)),
      url_generator_(url_generator) {}

StartPageTokenRequest::~StartPageTokenRequest() = default;

GURL StartPageTokenRequest::GetURLInternal() const {
  return url_generator_.GetStartPageTokenUrl(team_drive_id_);
}

//============================= FilesListRequest =============================

FilesListRequest::FilesListRequest(RequestSender* sender,
                                   const DriveApiUrlGenerator& url_generator,
                                   FileListCallback callback)
    : DriveApiDataRequest<FileList>(sender, std::move(callback)),
      url_generator_(url_generator),
      max_results_(100),
      corpora_(FilesListCorpora::DEFAULT) {}

FilesListRequest::~FilesListRequest() = default;

GURL FilesListRequest::GetURLInternal() const {
  return url_generator_.GetFilesListUrl(max_results_, page_token_, corpora_,
                                        team_drive_id_, q_);
}

//======================== FilesListNextPageRequest =========================

FilesListNextPageRequest::FilesListNextPageRequest(RequestSender* sender,
                                                   FileListCallback callback)
    : DriveApiDataRequest<FileList>(sender, std::move(callback)) {}

FilesListNextPageRequest::~FilesListNextPageRequest() = default;

GURL FilesListNextPageRequest::GetURLInternal() const {
  return next_link_;
}

//============================ FilesDeleteRequest =============================

FilesDeleteRequest::FilesDeleteRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    EntryActionCallback callback)
    : EntryActionRequest(sender, std::move(callback)),
      url_generator_(url_generator) {}

FilesDeleteRequest::~FilesDeleteRequest() = default;

HttpRequestMethod FilesDeleteRequest::GetRequestType() const {
  return HttpRequestMethod::kDelete;
}

GURL FilesDeleteRequest::GetURL() const {
  return url_generator_.GetFilesDeleteUrl(file_id_);
}

std::vector<std::string> FilesDeleteRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers(
      EntryActionRequest::GetExtraRequestHeaders());
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

//============================ FilesTrashRequest =============================

FilesTrashRequest::FilesTrashRequest(RequestSender* sender,
                                     const DriveApiUrlGenerator& url_generator,
                                     FileResourceCallback callback)
    : DriveApiDataRequest<FileResource>(sender, std::move(callback)),
      url_generator_(url_generator) {}

FilesTrashRequest::~FilesTrashRequest() = default;

HttpRequestMethod FilesTrashRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

GURL FilesTrashRequest::GetURLInternal() const {
  return url_generator_.GetFilesTrashUrl(file_id_);
}

//============================== AboutGetRequest =============================

AboutGetRequest::AboutGetRequest(RequestSender* sender,
                                 const DriveApiUrlGenerator& url_generator,
                                 AboutResourceCallback callback)
    : DriveApiDataRequest<AboutResource>(sender, std::move(callback)),
      url_generator_(url_generator) {}

AboutGetRequest::~AboutGetRequest() = default;

GURL AboutGetRequest::GetURLInternal() const {
  return url_generator_.GetAboutGetUrl();
}

//============================ ChangesListRequest ===========================

ChangesListRequest::ChangesListRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    ChangeListCallback callback)
    : DriveApiDataRequest<ChangeList>(sender, std::move(callback)),
      url_generator_(url_generator),
      include_deleted_(true),
      max_results_(100),
      start_change_id_(0) {}

ChangesListRequest::~ChangesListRequest() = default;

GURL ChangesListRequest::GetURLInternal() const {
  return url_generator_.GetChangesListUrl(include_deleted_, max_results_,
                                          page_token_, start_change_id_,
                                          team_drive_id_);
}

//======================== ChangesListNextPageRequest =========================

ChangesListNextPageRequest::ChangesListNextPageRequest(
    RequestSender* sender,
    ChangeListCallback callback)
    : DriveApiDataRequest<ChangeList>(sender, std::move(callback)) {}

ChangesListNextPageRequest::~ChangesListNextPageRequest() = default;

GURL ChangesListNextPageRequest::GetURLInternal() const {
  return next_link_;
}

//========================== ChildrenInsertRequest ============================

ChildrenInsertRequest::ChildrenInsertRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    EntryActionCallback callback)
    : EntryActionRequest(sender, std::move(callback)),
      url_generator_(url_generator) {}

ChildrenInsertRequest::~ChildrenInsertRequest() = default;

HttpRequestMethod ChildrenInsertRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

GURL ChildrenInsertRequest::GetURL() const {
  return url_generator_.GetChildrenInsertUrl(folder_id_);
}

bool ChildrenInsertRequest::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = util::kContentTypeApplicationJson;

  base::Value::Dict root;
  root.Set("id", id_);

  base::JSONWriter::Write(root, upload_content);
  DVLOG(1) << "InsertResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//========================== ChildrenDeleteRequest ============================

ChildrenDeleteRequest::ChildrenDeleteRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    EntryActionCallback callback)
    : EntryActionRequest(sender, std::move(callback)),
      url_generator_(url_generator) {}

ChildrenDeleteRequest::~ChildrenDeleteRequest() = default;

HttpRequestMethod ChildrenDeleteRequest::GetRequestType() const {
  return HttpRequestMethod::kDelete;
}

GURL ChildrenDeleteRequest::GetURL() const {
  return url_generator_.GetChildrenDeleteUrl(child_id_, folder_id_);
}

//======================= InitiateUploadNewFileRequest =======================

InitiateUploadNewFileRequest::InitiateUploadNewFileRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    InitiateUploadCallback callback)
    : InitiateUploadRequestBase(sender,
                                std::move(callback),
                                content_type,
                                content_length),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      title_(title) {}

InitiateUploadNewFileRequest::~InitiateUploadNewFileRequest() = default;

GURL InitiateUploadNewFileRequest::GetURL() const {
  return url_generator_.GetInitiateUploadNewFileUrl(!modified_date_.is_null());
}

HttpRequestMethod InitiateUploadNewFileRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool InitiateUploadNewFileRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = util::kContentTypeApplicationJson;

  base::Value::Dict root;
  root.Set("title", title_);

  // Fill parent link.
  base::Value::List parents;
  parents.Append(util::CreateParentValue(parent_resource_id_));
  root.Set("parents", base::Value(std::move(parents)));

  if (!modified_date_.is_null())
    root.Set("modifiedDate", util::FormatTimeAsString(modified_date_));

  if (!last_viewed_by_me_date_.is_null()) {
    root.Set("lastViewedByMeDate",
             util::FormatTimeAsString(last_viewed_by_me_date_));
  }

  AttachProperties(properties_, root);
  base::JSONWriter::Write(root, upload_content);

  DVLOG(1) << "InitiateUploadNewFile data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//===================== InitiateUploadExistingFileRequest ====================

InitiateUploadExistingFileRequest::InitiateUploadExistingFileRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const std::string& etag,
    InitiateUploadCallback callback)
    : InitiateUploadRequestBase(sender,
                                std::move(callback),
                                content_type,
                                content_length),
      url_generator_(url_generator),
      resource_id_(resource_id),
      etag_(etag) {}

InitiateUploadExistingFileRequest::~InitiateUploadExistingFileRequest() =
    default;

GURL InitiateUploadExistingFileRequest::GetURL() const {
  return url_generator_.GetInitiateUploadExistingFileUrl(
      resource_id_, !modified_date_.is_null());
}

HttpRequestMethod InitiateUploadExistingFileRequest::GetRequestType() const {
  return HttpRequestMethod::kPut;
}

std::vector<std::string>
InitiateUploadExistingFileRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers(
      InitiateUploadRequestBase::GetExtraRequestHeaders());
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

bool InitiateUploadExistingFileRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  base::Value::Dict root;
  if (!parent_resource_id_.empty()) {
    base::Value::List parents;
    parents.Append(util::CreateParentValue(parent_resource_id_));
    root.Set("parents", base::Value(std::move(parents)));
  }

  if (!title_.empty())
    root.Set("title", title_);

  if (!modified_date_.is_null())
    root.Set("modifiedDate", util::FormatTimeAsString(modified_date_));

  if (!last_viewed_by_me_date_.is_null()) {
    root.Set("lastViewedByMeDate",
             util::FormatTimeAsString(last_viewed_by_me_date_));
  }

  AttachProperties(properties_, root);
  if (root.empty())
    return false;

  *upload_content_type = util::kContentTypeApplicationJson;
  base::JSONWriter::Write(root, upload_content);
  DVLOG(1) << "InitiateUploadExistingFile data: " << *upload_content_type
           << ", [" << *upload_content << "]";
  return true;
}

//============================ ResumeUploadRequest ===========================

ResumeUploadRequest::ResumeUploadRequest(RequestSender* sender,
                                         const GURL& upload_location,
                                         int64_t start_position,
                                         int64_t end_position,
                                         int64_t content_length,
                                         const std::string& content_type,
                                         const base::FilePath& local_file_path,
                                         UploadRangeCallback callback,
                                         ProgressCallback progress_callback)
    : ResumeUploadRequestBase(sender,
                              upload_location,
                              start_position,
                              end_position,
                              content_length,
                              content_type,
                              local_file_path,
                              progress_callback),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

ResumeUploadRequest::~ResumeUploadRequest() = default;

void ResumeUploadRequest::OnRangeRequestComplete(
    const UploadRangeResponse& response,
    std::unique_ptr<base::Value> value) {
  DCHECK(CalledOnValidThread());
  ParseFileResourceWithUploadRangeAndRun(std::move(callback_), response,
                                         std::move(value));
}

//========================== GetUploadStatusRequest ==========================

GetUploadStatusRequest::GetUploadStatusRequest(RequestSender* sender,
                                               const GURL& upload_url,
                                               int64_t content_length,
                                               UploadRangeCallback callback)
    : GetUploadStatusRequestBase(sender, upload_url, content_length),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

GetUploadStatusRequest::~GetUploadStatusRequest() = default;

void GetUploadStatusRequest::OnRangeRequestComplete(
    const UploadRangeResponse& response,
    std::unique_ptr<base::Value> value) {
  DCHECK(CalledOnValidThread());
  ParseFileResourceWithUploadRangeAndRun(std::move(callback_), response,
                                         std::move(value));
}

//======================= MultipartUploadNewFileDelegate =======================

MultipartUploadNewFileDelegate::MultipartUploadNewFileDelegate(
    base::SequencedTaskRunner* task_runner,
    const std::string& title,
    const std::string& parent_resource_id,
    const std::string& content_type,
    std::optional<std::string_view> converted_mime_type,
    int64_t content_length,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const base::FilePath& local_file_path,
    const Properties& properties,
    const DriveApiUrlGenerator& url_generator,
    FileResourceCallback callback,
    ProgressCallback progress_callback)
    : MultipartUploadRequestBase(
          task_runner,
          CreateMultipartUploadMetadataJson(title,
                                            parent_resource_id,
                                            converted_mime_type,
                                            modified_date,
                                            last_viewed_by_me_date,
                                            properties),
          content_type,
          content_length,
          local_file_path,
          std::move(callback),
          progress_callback),
      has_modified_date_(!modified_date.is_null()),
      convert_(converted_mime_type.has_value()),
      url_generator_(url_generator) {}

MultipartUploadNewFileDelegate::~MultipartUploadNewFileDelegate() = default;

GURL MultipartUploadNewFileDelegate::GetURL() const {
  return url_generator_.GetMultipartUploadNewFileUrl(has_modified_date_,
                                                     convert_);
}

HttpRequestMethod MultipartUploadNewFileDelegate::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

//====================== MultipartUploadExistingFileDelegate ===================

MultipartUploadExistingFileDelegate::MultipartUploadExistingFileDelegate(
    base::SequencedTaskRunner* task_runner,
    const std::string& title,
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& content_type,
    int64_t content_length,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const base::FilePath& local_file_path,
    const std::string& etag,
    const Properties& properties,
    const DriveApiUrlGenerator& url_generator,
    FileResourceCallback callback,
    ProgressCallback progress_callback)
    : MultipartUploadRequestBase(task_runner,
                                 CreateMultipartUploadMetadataJson(
                                     title,
                                     parent_resource_id,
                                     /*converted_mime_type=*/std::nullopt,
                                     modified_date,
                                     last_viewed_by_me_date,
                                     properties),
                                 content_type,
                                 content_length,
                                 local_file_path,
                                 std::move(callback),
                                 progress_callback),
      resource_id_(resource_id),
      etag_(etag),
      has_modified_date_(!modified_date.is_null()),
      url_generator_(url_generator) {}

MultipartUploadExistingFileDelegate::~MultipartUploadExistingFileDelegate() =
    default;

std::vector<std::string>
MultipartUploadExistingFileDelegate::GetExtraRequestHeaders() const {
  std::vector<std::string> headers(
      MultipartUploadRequestBase::GetExtraRequestHeaders());
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

GURL MultipartUploadExistingFileDelegate::GetURL() const {
  return url_generator_.GetMultipartUploadExistingFileUrl(resource_id_,
                                                          has_modified_date_);
}

HttpRequestMethod MultipartUploadExistingFileDelegate::GetRequestType() const {
  return HttpRequestMethod::kPut;
}

//========================== DownloadFileRequest ==========================

DownloadFileRequest::DownloadFileRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const base::FilePath& output_file_path,
    DownloadActionCallback download_action_callback,
    const GetContentCallback& get_content_callback,
    ProgressCallback progress_callback)
    : DownloadFileRequestBase(
          sender,
          std::move(download_action_callback),
          get_content_callback,
          progress_callback,
          url_generator.GenerateDownloadFileUrl(resource_id),
          output_file_path) {}

DownloadFileRequest::~DownloadFileRequest() = default;

//========================== PermissionsInsertRequest ==========================

PermissionsInsertRequest::PermissionsInsertRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    EntryActionCallback callback)
    : EntryActionRequest(sender, std::move(callback)),
      url_generator_(url_generator),
      type_(PERMISSION_TYPE_USER),
      role_(PERMISSION_ROLE_READER) {}

PermissionsInsertRequest::~PermissionsInsertRequest() = default;

GURL PermissionsInsertRequest::GetURL() const {
  return url_generator_.GetPermissionsInsertUrl(id_);
}

HttpRequestMethod PermissionsInsertRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool PermissionsInsertRequest::GetContentData(std::string* upload_content_type,
                                              std::string* upload_content) {
  *upload_content_type = util::kContentTypeApplicationJson;

  base::Value::Dict root;
  switch (type_) {
    case PERMISSION_TYPE_ANYONE:
      root.Set("type", "anyone");
      break;
    case PERMISSION_TYPE_DOMAIN:
      root.Set("type", "domain");
      break;
    case PERMISSION_TYPE_GROUP:
      root.Set("type", "group");
      break;
    case PERMISSION_TYPE_USER:
      root.Set("type", "user");
      break;
  }
  switch (role_) {
    case PERMISSION_ROLE_OWNER:
      root.Set("role", "owner");
      break;
    case PERMISSION_ROLE_READER:
      root.Set("role", "reader");
      break;
    case PERMISSION_ROLE_WRITER:
      root.Set("role", "writer");
      break;
    case PERMISSION_ROLE_COMMENTER:
      root.Set("role", "reader");
      {
        base::Value::List list;
        list.Append("commenter");
        root.Set("additionalRoles", std::move(list));
      }
      break;
  }
  root.Set("value", value_);
  base::JSONWriter::Write(root, upload_content);
  return true;
}

//======================= SingleBatchableDelegateRequest =======================

SingleBatchableDelegateRequest::SingleBatchableDelegateRequest(
    RequestSender* sender,
    std::unique_ptr<BatchableDelegate> delegate)
    : DriveUrlFetchRequestBase(
          sender,
          base::BindRepeating(
              &SingleBatchableDelegateRequest::OnUploadProgress,
              // Safe to not retain as the SimpleURLLoader is owned by our base
              // class and cannot outlive this instance.
              base::Unretained(this)),
          ProgressCallback()),
      delegate_(std::move(delegate)) {}

SingleBatchableDelegateRequest::~SingleBatchableDelegateRequest() = default;

GURL SingleBatchableDelegateRequest::GetURL() const {
  return delegate_->GetURL();
}

HttpRequestMethod SingleBatchableDelegateRequest::GetRequestType() const {
  return delegate_->GetRequestType();
}

std::vector<std::string>
SingleBatchableDelegateRequest::GetExtraRequestHeaders() const {
  return delegate_->GetExtraRequestHeaders();
}

void SingleBatchableDelegateRequest::Prepare(PrepareCallback callback) {
  delegate_->Prepare(std::move(callback));
}

bool SingleBatchableDelegateRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  return delegate_->GetContentData(upload_content_type, upload_content);
}

void SingleBatchableDelegateRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  delegate_->NotifyResult(
      GetErrorCode(), response_body,
      base::BindOnce(
          &SingleBatchableDelegateRequest::OnProcessURLFetchResultsComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

void SingleBatchableDelegateRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode code) {
  delegate_->NotifyError(code);
}

void SingleBatchableDelegateRequest::OnUploadProgress(int64_t current,
                                                      int64_t total) {
  delegate_->NotifyUploadProgress(current, total);
}

//========================== BatchUploadRequest ==========================

BatchUploadChildEntry::BatchUploadChildEntry(BatchableDelegate* request)
    : request(request), prepared(false), data_offset(0), data_size(0) {}

BatchUploadChildEntry::~BatchUploadChildEntry() = default;

BatchUploadRequest::BatchUploadRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator)
    : DriveUrlFetchRequestBase(
          sender,
          // Safe to not retain as the SimpleURLLoader is owned by our base
          // class and cannot outlive this instance.
          base::BindRepeating(&BatchUploadRequest::OnUploadProgress,
                              base::Unretained(this)),
          ProgressCallback()),
      sender_(sender),
      url_generator_(url_generator),
      committed_(false),
      last_progress_value_(0) {}

BatchUploadRequest::~BatchUploadRequest() = default;

void BatchUploadRequest::SetBoundaryForTesting(const std::string& boundary) {
  boundary_ = boundary;
}

void BatchUploadRequest::AddRequest(BatchableDelegate* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  DCHECK(GetChildEntry(request) == child_requests_.end());
  DCHECK(!committed_);
  child_requests_.push_back(std::make_unique<BatchUploadChildEntry>(request));
  request->Prepare(base::BindOnce(&BatchUploadRequest::OnChildRequestPrepared,
                                  weak_ptr_factory_.GetWeakPtr(), request));
}

void BatchUploadRequest::OnChildRequestPrepared(RequestID request_id,
                                                ApiErrorCode result) {
  DCHECK(CalledOnValidThread());
  auto const child = GetChildEntry(request_id);
  CHECK(child != child_requests_.end(), base::NotFatalUntil::M130);
  if (IsSuccessfulDriveApiErrorCode(result)) {
    (*child)->prepared = true;
  } else {
    (*child)->request->NotifyError(result);
    child_requests_.erase(child);
  }
  MayCompletePrepare();
}

void BatchUploadRequest::Commit() {
  DCHECK(CalledOnValidThread());
  DCHECK(!committed_);
  if (child_requests_.empty()) {
    Cancel();
  } else {
    committed_ = true;
    MayCompletePrepare();
  }
}

void BatchUploadRequest::Prepare(PrepareCallback callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(callback);
  prepare_callback_ = std::move(callback);
  MayCompletePrepare();
}

void BatchUploadRequest::Cancel() {
  child_requests_.clear();
  UrlFetchRequestBase::Cancel();
}

// Obtains corresponding child entry of |request_id|. Returns NULL if the
// entry is not found.
std::vector<std::unique_ptr<BatchUploadChildEntry>>::iterator
BatchUploadRequest::GetChildEntry(RequestID request_id) {
  for (auto it = child_requests_.begin(); it != child_requests_.end(); ++it) {
    if ((*it)->request.get() == request_id)
      return it;
  }
  return child_requests_.end();
}

void BatchUploadRequest::MayCompletePrepare() {
  if (!committed_ || prepare_callback_.is_null())
    return;
  for (const auto& child : child_requests_) {
    if (!child->prepared)
      return;
  }

  // Build multipart body here.
  std::vector<ContentTypeAndData> parts;
  for (const auto& child : child_requests_) {
    std::string type;
    std::string data;
    const bool result = child->request->GetContentData(&type, &data);
    // Upload request must have content data.
    DCHECK(result);

    const GURL url = child->request->GetURL();
    HttpRequestMethod method = child->request->GetRequestType();
    const std::string header = base::StringPrintf(
        kBatchUploadRequestFormat, HttpRequestMethodToString(method).c_str(),
        url.path().c_str(), url_generator_.GetBatchUploadUrl().host().c_str(),
        type.c_str());

    child->data_offset = header.size();
    child->data_size = data.size();

    parts.push_back(ContentTypeAndData({kHttpContentType, header + data}));
  }

  std::vector<uint64_t> part_data_offset;
  GenerateMultipartBody(MultipartType::kMixed, boundary_, parts,
                        &upload_content_, &part_data_offset);
  DCHECK(part_data_offset.size() == child_requests_.size());
  for (size_t i = 0; i < child_requests_.size(); ++i) {
    child_requests_[i]->data_offset += part_data_offset[i];
  }
  std::move(prepare_callback_).Run(HTTP_SUCCESS);
}

bool BatchUploadRequest::GetContentData(std::string* upload_content_type,
                                        std::string* upload_content_data) {
  upload_content_type->assign(upload_content_.type);
  upload_content_data->assign(upload_content_.data);
  return true;
}

base::WeakPtr<BatchUploadRequest>
BatchUploadRequest::GetWeakPtrAsBatchUploadRequest() {
  return weak_ptr_factory_.GetWeakPtr();
}

GURL BatchUploadRequest::GetURL() const {
  return url_generator_.GetBatchUploadUrl();
}

HttpRequestMethod BatchUploadRequest::GetRequestType() const {
  return HttpRequestMethod::kPut;
}

std::vector<std::string> BatchUploadRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kBatchUploadHeader);
  return headers;
}

void BatchUploadRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  if (!IsSuccessfulDriveApiErrorCode(GetErrorCode())) {
    RunCallbackOnPrematureFailure(GetErrorCode());
    sender_->RequestFinished(this);
    return;
  }

  std::string content_type;
  if (response_head) {
    response_head->headers->EnumerateHeader(
        /* need only first header */ nullptr, "Content-Type", &content_type);
  }

  std::vector<MultipartHttpResponse> parts;
  if (!ParseMultipartResponse(content_type, response_body, &parts) ||
      child_requests_.size() != parts.size()) {
    RunCallbackOnPrematureFailure(PARSE_ERROR);
    sender_->RequestFinished(this);
    return;
  }

  for (size_t i = 0; i < parts.size(); ++i) {
    BatchableDelegate* delegate = child_requests_[i]->request.get();
    // Pass ownership of |delegate| so that child_requests_.clear() won't
    // kill the delegate. It has to be deleted after the notification.
    delegate->NotifyResult(
        parts[i].code, parts[i].body,
        base::BindOnce(&base::DeletePointer<BatchableDelegate>,
                       child_requests_[i]->request.release()));
  }
  child_requests_.clear();

  sender_->RequestFinished(this);
}

void BatchUploadRequest::RunCallbackOnPrematureFailure(ApiErrorCode code) {
  for (const auto& child : child_requests_)
    child->request->NotifyError(code);
  child_requests_.clear();
}

void BatchUploadRequest::OnUploadProgress(int64_t current, int64_t total) {
  for (const auto& child : child_requests_) {
    if (child->data_offset <= current &&
        current <= child->data_offset + child->data_size) {
      child->request->NotifyUploadProgress(current - child->data_offset,
                                           child->data_size);
    } else if (last_progress_value_ < child->data_offset + child->data_size &&
               child->data_offset + child->data_size < current) {
      child->request->NotifyUploadProgress(child->data_size, child->data_size);
    }
  }
  last_progress_value_ = current;
}
}  // namespace google_apis::drive
