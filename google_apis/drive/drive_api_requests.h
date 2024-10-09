// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_API_REQUESTS_H_
#define GOOGLE_APIS_DRIVE_DRIVE_API_REQUESTS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/drive_base_requests.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace google_apis {

// Callback used for requests that the server returns TeamDrive data
// formatted into JSON value.
using TeamDriveListCallback =
    base::OnceCallback<void(ApiErrorCode error,
                            std::unique_ptr<TeamDriveList> entry)>;

// Callback used for requests that the server returns FileList data
// formatted into JSON value.
typedef base::OnceCallback<void(ApiErrorCode error,
                                std::unique_ptr<FileList> entry)>
    FileListCallback;

// DEPRECATED: Please use ChangeListOnceCallback instead
// Callback used for requests that the server returns ChangeList data
// formatted into JSON value.
using ChangeListCallback =
    base::OnceCallback<void(ApiErrorCode error,
                            std::unique_ptr<ChangeList> entry)>;

using ChangeListOnceCallback =
    base::OnceCallback<void(ApiErrorCode error,
                            std::unique_ptr<ChangeList> entry)>;

// Callback used for requests that the server returns StartToken data
// formatted into JSON value.
using StartPageTokenCallback =
    base::OnceCallback<void(ApiErrorCode error,
                            std::unique_ptr<StartPageToken> entry)>;

namespace drive {

// Represents a property for a file or a directory.
// https://developers.google.com/drive/v2/reference/properties
class Property {
 public:
  Property();
  ~Property();

  // Visibility of the property. Either limited to the same client, or to any.
  enum Visibility { VISIBILITY_PRIVATE, VISIBILITY_PUBLIC };

  // Whether the property is public or private.
  Visibility visibility() const { return visibility_; }

  // Name of the property.
  const std::string& key() const { return key_; }

  // Value of the property.
  const std::string& value() const { return value_; }

  void set_visibility(Visibility visibility) { visibility_ = visibility; }
  void set_key(const std::string& key) { key_ = key; }
  void set_value(const std::string& value) { value_ = value; }

 private:
  Visibility visibility_;
  std::string key_;
  std::string value_;
};

// List of properties for a single file or a directory.
typedef std::vector<Property> Properties;

// Child response embedded in multipart parent response.
struct MultipartHttpResponse {
  MultipartHttpResponse();
  ~MultipartHttpResponse();
  ApiErrorCode code = HTTP_SUCCESS;
  std::string body;
};

// Splits multipart |response| into |parts|. Each part must be HTTP sub-response
// of drive batch request. |content_type| is a value of Content-Type response
// header. Returns true on success.
bool ParseMultipartResponse(const std::string& content_type,
                            const std::string& response,
                            std::vector<MultipartHttpResponse>* parts);

//============================ DriveApiPartialFieldRequest ====================

// This is base class of the Drive API related requests. All Drive API requests
// support partial request (to improve the performance). The function can be
// shared among the Drive API requests.
// See also https://developers.google.com/drive/performance
class DriveApiPartialFieldRequest : public DriveUrlFetchRequestBase {
 public:
  explicit DriveApiPartialFieldRequest(RequestSender* sender);

  DriveApiPartialFieldRequest(const DriveApiPartialFieldRequest&) = delete;
  DriveApiPartialFieldRequest& operator=(const DriveApiPartialFieldRequest&) =
      delete;

  ~DriveApiPartialFieldRequest() override;

  // Optional parameter.
  const std::string& fields() const { return fields_; }
  void set_fields(const std::string& fields) { fields_ = fields; }

 protected:
  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;

  // Derived classes should override GetURLInternal instead of GetURL()
  // directly.
  virtual GURL GetURLInternal() const = 0;

 private:
  std::string fields_;
};

//============================ DriveApiDataRequest ===========================

// The base class of Drive API related requests that receive a JSON response
// representing |DataType|.
template <class DataType>
class DriveApiDataRequest : public DriveApiPartialFieldRequest {
 public:
  using Callback = base::OnceCallback<void(ApiErrorCode error,
                                           std::unique_ptr<DataType> data)>;

  // |callback| is called when the request finishes either by success or by
  // failure. On success, a JSON Value object is passed. It must not be null.
  DriveApiDataRequest(RequestSender* sender, Callback callback)
      : DriveApiPartialFieldRequest(sender), callback_(std::move(callback)) {
    DCHECK(!callback_.is_null());
  }

  DriveApiDataRequest(const DriveApiDataRequest&) = delete;
  DriveApiDataRequest& operator=(const DriveApiDataRequest&) = delete;

  ~DriveApiDataRequest() override {}

 protected:
  // UrlFetchRequestBase overrides.
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override {
    ApiErrorCode error = GetErrorCode();
    switch (error) {
      case HTTP_SUCCESS:
      case HTTP_CREATED:
        blocking_task_runner()->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&DriveApiDataRequest::Parse,
                           std::move(response_body)),
            base::BindOnce(&DriveApiDataRequest::OnDataParsed,
                           weak_ptr_factory_.GetWeakPtr(), error));
        break;
      default:
        RunCallbackOnPrematureFailure(error);
        OnProcessURLFetchResultsComplete();
        break;
    }
  }

  void RunCallbackOnPrematureFailure(ApiErrorCode error) override {
    std::move(callback_).Run(error, std::unique_ptr<DataType>());
  }

 private:
  // Parses the |json| string by using DataType::CreateFrom.
  static std::unique_ptr<DataType> Parse(std::string json) {
    std::unique_ptr<base::Value> value = ParseJson(json);
    return value ? DataType::CreateFrom(*value) : std::unique_ptr<DataType>();
  }

  // Receives the parsed result and invokes the callback.
  void OnDataParsed(ApiErrorCode error, std::unique_ptr<DataType> value) {
    if (!value)
      error = PARSE_ERROR;
    std::move(callback_).Run(error, std::move(value));
    OnProcessURLFetchResultsComplete();
  }

  Callback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveApiDataRequest> weak_ptr_factory_{this};
};

//=============================== FilesGetRequest =============================

// This class performs the request for fetching a file.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/get
class FilesGetRequest : public DriveApiDataRequest<FileResource> {
 public:
  FilesGetRequest(RequestSender* sender,
                  const DriveApiUrlGenerator& url_generator,
                  FileResourceCallback callback);

  FilesGetRequest(const FilesGetRequest&) = delete;
  FilesGetRequest& operator=(const FilesGetRequest&) = delete;

  ~FilesGetRequest() override;

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

  // Optional parameter.
  const GURL& embed_origin() const { return embed_origin_; }
  void set_embed_origin(const GURL& embed_origin) {
    embed_origin_ = embed_origin;
  }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string file_id_;
  GURL embed_origin_;
};

//============================ FilesInsertRequest =============================

// Enumeration type for specifying visibility of files.
enum FileVisibility {
  FILE_VISIBILITY_DEFAULT,
  FILE_VISIBILITY_PRIVATE,
};

// This class performs the request for creating a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/insert
// See also https://developers.google.com/drive/manage-uploads and
// https://developers.google.com/drive/folder
class FilesInsertRequest : public DriveApiDataRequest<FileResource> {
 public:
  FilesInsertRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator,
                     FileResourceCallback callback);

  FilesInsertRequest(const FilesInsertRequest&) = delete;
  FilesInsertRequest& operator=(const FilesInsertRequest&) = delete;

  ~FilesInsertRequest() override;

  // Optional parameter
  void set_visibility(FileVisibility visibility) { visibility_ = visibility; }

  // Optional request body.
  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }

  const std::string& mime_type() const { return mime_type_; }
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

  const base::Time& modified_date() const { return modified_date_; }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }

  const std::vector<std::string>& parents() const { return parents_; }
  void add_parent(const std::string& parent) { parents_.push_back(parent); }

  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  const Properties& properties() const { return properties_; }
  void set_properties(const Properties& properties) {
    properties_ = properties;
  }

 protected:
  // Overridden from GetDataRequest.
  HttpRequestMethod GetRequestType() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;

  FileVisibility visibility_;
  base::Time last_viewed_by_me_date_;
  std::string mime_type_;
  base::Time modified_date_;
  std::vector<std::string> parents_;
  std::string title_;
  Properties properties_;
};

//============================== FilesPatchRequest ============================

// This class performs the request for patching file metadata.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/patch
class FilesPatchRequest : public DriveApiDataRequest<FileResource> {
 public:
  FilesPatchRequest(RequestSender* sender,
                    const DriveApiUrlGenerator& url_generator,
                    FileResourceCallback callback);

  FilesPatchRequest(const FilesPatchRequest&) = delete;
  FilesPatchRequest& operator=(const FilesPatchRequest&) = delete;

  ~FilesPatchRequest() override;

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

  // Optional parameter.
  bool set_modified_date() const { return set_modified_date_; }
  void set_set_modified_date(bool set_modified_date) {
    set_modified_date_ = set_modified_date;
  }

  bool update_viewed_date() const { return update_viewed_date_; }
  void set_update_viewed_date(bool update_viewed_date) {
    update_viewed_date_ = update_viewed_date;
  }

  // Optional request body.
  // Note: "Files: patch" accepts any "Files resource" data, but this class
  // only supports limited members of it for now. We can extend it upon
  // requirments.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  const base::Time& modified_date() const { return modified_date_; }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }

  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }

  const std::vector<std::string>& parents() const { return parents_; }
  void add_parent(const std::string& parent) { parents_.push_back(parent); }

  const Properties& properties() const { return properties_; }
  void set_properties(const Properties& properties) {
    properties_ = properties;
  }

 protected:
  // Overridden from URLFetchRequestBase.
  HttpRequestMethod GetRequestType() const override;
  std::vector<std::string> GetExtraRequestHeaders() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;

  std::string file_id_;
  bool set_modified_date_;
  bool update_viewed_date_;

  std::string title_;
  base::Time modified_date_;
  base::Time last_viewed_by_me_date_;
  std::vector<std::string> parents_;
  Properties properties_;
};

//============================= FilesCopyRequest ==============================

// This class performs the request for copying a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/copy
class FilesCopyRequest : public DriveApiDataRequest<FileResource> {
 public:
  // Upon completion, |callback| will be called. |callback| must not be null.
  FilesCopyRequest(RequestSender* sender,
                   const DriveApiUrlGenerator& url_generator,
                   FileResourceCallback callback);

  FilesCopyRequest(const FilesCopyRequest&) = delete;
  FilesCopyRequest& operator=(const FilesCopyRequest&) = delete;

  ~FilesCopyRequest() override;

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

  // Optional parameter
  void set_visibility(FileVisibility visibility) { visibility_ = visibility; }

  // Optional request body.
  const std::vector<std::string>& parents() const { return parents_; }
  void add_parent(const std::string& parent) { parents_.push_back(parent); }

  const base::Time& modified_date() const { return modified_date_; }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }

  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

 protected:
  // Overridden from URLFetchRequestBase.
  HttpRequestMethod GetRequestType() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;

  std::string file_id_;
  FileVisibility visibility_;
  base::Time modified_date_;
  std::vector<std::string> parents_;
  std::string title_;
};

//========================== TeamDriveListRequest =============================

// This class performs the request for fetching TeamDrive list.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by another request using this class, by
// setting the next_page_token from previous call, to page_token.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/teamdrives/list
class TeamDriveListRequest : public DriveApiDataRequest<TeamDriveList> {
 public:
  TeamDriveListRequest(RequestSender* sender,
                       const DriveApiUrlGenerator& url_generator,
                       TeamDriveListCallback callback);

  TeamDriveListRequest(const TeamDriveListRequest&) = delete;
  TeamDriveListRequest& operator=(const TeamDriveListRequest&) = delete;

  ~TeamDriveListRequest() override;

  // Optional parameter
  int max_results() const { return max_results_; }
  void set_max_results(int max_results) { max_results_ = max_results; }

  const std::string& page_token() const { return page_token_; }
  void set_page_token(const std::string& page_token) {
    page_token_ = page_token;
  }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  int max_results_;
  std::string page_token_;
};

//========================== StartPageTokenRequest =============================

// This class performs the request for fetching the start page token.
// |team_drive_id_| may be empty, in which case the start page token will be
// returned for the users changes.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/changes/getStartPageToken
class StartPageTokenRequest : public DriveApiDataRequest<StartPageToken> {
 public:
  StartPageTokenRequest(RequestSender* sender,
                        const DriveApiUrlGenerator& url_generator,
                        StartPageTokenCallback callback);

  StartPageTokenRequest(const StartPageTokenRequest&) = delete;
  StartPageTokenRequest& operator=(const StartPageTokenRequest&) = delete;

  ~StartPageTokenRequest() override;

  // Optional parameter
  const std::string& team_drive_id() const { return team_drive_id_; }
  void set_team_drive_id(const std::string& team_drive_id) {
    team_drive_id_ = team_drive_id;
  }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string team_drive_id_;
};

//============================= FilesListRequest =============================

// This class performs the request for fetching FileList.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListRequest defined below,
// or by FilesListRequest with setting page token.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/list
class FilesListRequest : public DriveApiDataRequest<FileList> {
 public:
  FilesListRequest(RequestSender* sender,
                   const DriveApiUrlGenerator& url_generator,
                   FileListCallback callback);

  FilesListRequest(const FilesListRequest&) = delete;
  FilesListRequest& operator=(const FilesListRequest&) = delete;

  ~FilesListRequest() override;

  // Optional parameter
  int max_results() const { return max_results_; }
  void set_max_results(int max_results) { max_results_ = max_results; }

  const std::string& page_token() const { return page_token_; }
  void set_page_token(const std::string& page_token) {
    page_token_ = page_token;
  }

  FilesListCorpora corpora() const { return corpora_; }
  void set_corpora(FilesListCorpora corpora) { corpora_ = corpora; }

  const std::string& team_drive_id() const { return team_drive_id_; }
  void set_team_drive_id(const std::string& team_drive_id) {
    team_drive_id_ = team_drive_id;
  }

  const std::string& q() const { return q_; }
  void set_q(const std::string& q) { q_ = q; }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  int max_results_;
  std::string page_token_;
  FilesListCorpora corpora_;
  std::string team_drive_id_;
  std::string q_;
};

//========================= FilesListNextPageRequest ==========================

// There are two ways to obtain next pages of "Files: list" result (if paged).
// 1) Set pageToken and all params used for the initial request.
// 2) Use URL in the nextLink field in the previous response.
// This class implements 2)'s request.
class FilesListNextPageRequest : public DriveApiDataRequest<FileList> {
 public:
  FilesListNextPageRequest(RequestSender* sender, FileListCallback callback);

  FilesListNextPageRequest(const FilesListNextPageRequest&) = delete;
  FilesListNextPageRequest& operator=(const FilesListNextPageRequest&) = delete;

  ~FilesListNextPageRequest() override;

  const GURL& next_link() const { return next_link_; }
  void set_next_link(const GURL& next_link) { next_link_ = next_link; }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  GURL next_link_;
};

//============================= FilesDeleteRequest =============================

// This class performs the request for deleting a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/delete
class FilesDeleteRequest : public EntryActionRequest {
 public:
  FilesDeleteRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator,
                     EntryActionCallback callback);

  FilesDeleteRequest(const FilesDeleteRequest&) = delete;
  FilesDeleteRequest& operator=(const FilesDeleteRequest&) = delete;

  ~FilesDeleteRequest() override;

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }
  void set_etag(const std::string& etag) { etag_ = etag; }

 protected:
  // Overridden from UrlFetchRequestBase.
  HttpRequestMethod GetRequestType() const override;
  GURL GetURL() const override;
  std::vector<std::string> GetExtraRequestHeaders() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string file_id_;
  std::string etag_;
};

//============================= FilesTrashRequest ==============================

// This class performs the request for trashing a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/trash
class FilesTrashRequest : public DriveApiDataRequest<FileResource> {
 public:
  FilesTrashRequest(RequestSender* sender,
                    const DriveApiUrlGenerator& url_generator,
                    FileResourceCallback callback);

  FilesTrashRequest(const FilesTrashRequest&) = delete;
  FilesTrashRequest& operator=(const FilesTrashRequest&) = delete;

  ~FilesTrashRequest() override;

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

 protected:
  // Overridden from UrlFetchRequestBase.
  HttpRequestMethod GetRequestType() const override;

  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string file_id_;
};

//============================== AboutGetRequest =============================

// This class performs the request for fetching About data.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/about/get
class AboutGetRequest : public DriveApiDataRequest<AboutResource> {
 public:
  AboutGetRequest(RequestSender* sender,
                  const DriveApiUrlGenerator& url_generator,
                  AboutResourceCallback callback);

  AboutGetRequest(const AboutGetRequest&) = delete;
  AboutGetRequest& operator=(const AboutGetRequest&) = delete;

  ~AboutGetRequest() override;

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
};

//============================ ChangesListRequest ============================

// This class performs the request for fetching ChangeList.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListRequest defined below.
// or by ChangesListRequest with setting page token.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/changes/list
class ChangesListRequest : public DriveApiDataRequest<ChangeList> {
 public:
  ChangesListRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator,
                     ChangeListCallback callback);

  ChangesListRequest(const ChangesListRequest&) = delete;
  ChangesListRequest& operator=(const ChangesListRequest&) = delete;

  ~ChangesListRequest() override;

  // Optional parameter
  bool include_deleted() const { return include_deleted_; }
  void set_include_deleted(bool include_deleted) {
    include_deleted_ = include_deleted;
  }

  int max_results() const { return max_results_; }
  void set_max_results(int max_results) { max_results_ = max_results; }

  const std::string& page_token() const { return page_token_; }
  void set_page_token(const std::string& page_token) {
    page_token_ = page_token;
  }

  int64_t start_change_id() const { return start_change_id_; }
  void set_start_change_id(int64_t start_change_id) {
    start_change_id_ = start_change_id;
  }

  const std::string& team_drive_id() const { return team_drive_id_; }
  void set_team_drive_id(const std::string& team_drive_id) {
    team_drive_id_ = team_drive_id;
  }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  bool include_deleted_;
  int max_results_;
  std::string page_token_;
  int64_t start_change_id_;
  std::string team_drive_id_;
};

//======================== ChangesListNextPageRequest =========================

// There are two ways to obtain next pages of "Changes: list" result (if paged).
// 1) Set pageToken and all params used for the initial request.
// 2) Use URL in the nextLink field in the previous response.
// This class implements 2)'s request.
class ChangesListNextPageRequest : public DriveApiDataRequest<ChangeList> {
 public:
  ChangesListNextPageRequest(RequestSender* sender,
                             ChangeListCallback callback);

  ChangesListNextPageRequest(const ChangesListNextPageRequest&) = delete;
  ChangesListNextPageRequest& operator=(const ChangesListNextPageRequest&) =
      delete;

  ~ChangesListNextPageRequest() override;

  const GURL& next_link() const { return next_link_; }
  void set_next_link(const GURL& next_link) { next_link_ = next_link; }

 protected:
  // Overridden from DriveApiDataRequest.
  GURL GetURLInternal() const override;

 private:
  GURL next_link_;
};

//========================== ChildrenInsertRequest ============================

// This class performs the request for inserting a resource to a directory.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/children/insert
class ChildrenInsertRequest : public EntryActionRequest {
 public:
  ChildrenInsertRequest(RequestSender* sender,
                        const DriveApiUrlGenerator& url_generator,
                        EntryActionCallback callback);

  ChildrenInsertRequest(const ChildrenInsertRequest&) = delete;
  ChildrenInsertRequest& operator=(const ChildrenInsertRequest&) = delete;

  ~ChildrenInsertRequest() override;

  // Required parameter.
  const std::string& folder_id() const { return folder_id_; }
  void set_folder_id(const std::string& folder_id) { folder_id_ = folder_id; }

  // Required body.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

 protected:
  // UrlFetchRequestBase overrides.
  HttpRequestMethod GetRequestType() const override;
  GURL GetURL() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string folder_id_;
  std::string id_;
};

//========================== ChildrenDeleteRequest ============================

// This class performs the request for removing a resource from a directory.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/children/delete
class ChildrenDeleteRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  ChildrenDeleteRequest(RequestSender* sender,
                        const DriveApiUrlGenerator& url_generator,
                        EntryActionCallback callback);

  ChildrenDeleteRequest(const ChildrenDeleteRequest&) = delete;
  ChildrenDeleteRequest& operator=(const ChildrenDeleteRequest&) = delete;

  ~ChildrenDeleteRequest() override;

  // Required parameter.
  const std::string& child_id() const { return child_id_; }
  void set_child_id(const std::string& child_id) { child_id_ = child_id; }

  const std::string& folder_id() const { return folder_id_; }
  void set_folder_id(const std::string& folder_id) { folder_id_ = folder_id; }

 protected:
  // UrlFetchRequestBase overrides.
  HttpRequestMethod GetRequestType() const override;
  GURL GetURL() const override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string child_id_;
  std::string folder_id_;
};

//======================= InitiateUploadNewFileRequest =======================

// This class performs the request for initiating the upload of a new file.
class InitiateUploadNewFileRequest : public InitiateUploadRequestBase {
 public:
  // |parent_resource_id| should be the resource id of the parent directory.
  // |title| should be set.
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadNewFileRequest(RequestSender* sender,
                               const DriveApiUrlGenerator& url_generator,
                               const std::string& content_type,
                               int64_t content_length,
                               const std::string& parent_resource_id,
                               const std::string& title,
                               InitiateUploadCallback callback);

  InitiateUploadNewFileRequest(const InitiateUploadNewFileRequest&) = delete;
  InitiateUploadNewFileRequest& operator=(const InitiateUploadNewFileRequest&) =
      delete;

  ~InitiateUploadNewFileRequest() override;

  // Optional parameters.
  const base::Time& modified_date() const { return modified_date_; }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }
  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }
  const Properties& properties() const { return properties_; }
  void set_properties(const Properties& properties) {
    properties_ = properties;
  }

 protected:
  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string title_;

  base::Time modified_date_;
  base::Time last_viewed_by_me_date_;
  Properties properties_;
};

//==================== InitiateUploadExistingFileRequest =====================

// This class performs the request for initiating the upload of an existing
// file.
class InitiateUploadExistingFileRequest : public InitiateUploadRequestBase {
 public:
  // |upload_url| should be the upload_url() of the file
  //    (resumable-create-media URL)
  // |etag| should be set if it is available to detect the upload confliction.
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadExistingFileRequest(RequestSender* sender,
                                    const DriveApiUrlGenerator& url_generator,
                                    const std::string& content_type,
                                    int64_t content_length,
                                    const std::string& resource_id,
                                    const std::string& etag,
                                    InitiateUploadCallback callback);

  InitiateUploadExistingFileRequest(const InitiateUploadExistingFileRequest&) =
      delete;
  InitiateUploadExistingFileRequest& operator=(
      const InitiateUploadExistingFileRequest&) = delete;

  ~InitiateUploadExistingFileRequest() override;

  // Optional parameters.
  const std::string& parent_resource_id() const { return parent_resource_id_; }
  void set_parent_resource_id(const std::string& parent_resource_id) {
    parent_resource_id_ = parent_resource_id;
  }
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }
  const base::Time& modified_date() const { return modified_date_; }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }
  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }
  const Properties& properties() const { return properties_; }
  void set_properties(const Properties& properties) {
    properties_ = properties;
  }

 protected:
  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;
  std::vector<std::string> GetExtraRequestHeaders() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string etag_;

  std::string parent_resource_id_;
  std::string title_;
  base::Time modified_date_;
  base::Time last_viewed_by_me_date_;
  Properties properties_;
};

// Callback used for ResumeUpload() and GetUploadStatus().
typedef base::OnceCallback<void(const UploadRangeResponse& response,
                                std::unique_ptr<FileResource> new_resource)>
    UploadRangeCallback;

//============================ ResumeUploadRequest ===========================

// Performs the request for resuming the upload of a file.
class ResumeUploadRequest : public ResumeUploadRequestBase {
 public:
  // See also ResumeUploadRequestBase's comment for parameters meaning.
  // |callback| must not be null. |progress_callback| may be null.
  ResumeUploadRequest(RequestSender* sender,
                      const GURL& upload_location,
                      int64_t start_position,
                      int64_t end_position,
                      int64_t content_length,
                      const std::string& content_type,
                      const base::FilePath& local_file_path,
                      UploadRangeCallback callback,
                      ProgressCallback progress_callback);

  ResumeUploadRequest(const ResumeUploadRequest&) = delete;
  ResumeUploadRequest& operator=(const ResumeUploadRequest&) = delete;

  ~ResumeUploadRequest() override;

 protected:
  // UploadRangeRequestBase overrides.
  void OnRangeRequestComplete(const UploadRangeResponse& response,
                              std::unique_ptr<base::Value> value) override;

 private:
  UploadRangeCallback callback_;
};

//========================== GetUploadStatusRequest ==========================

// Performs the request to fetch the current upload status of a file.
class GetUploadStatusRequest : public GetUploadStatusRequestBase {
 public:
  // See also GetUploadStatusRequestBase's comment for parameters meaning.
  // |callback| must not be null.
  GetUploadStatusRequest(RequestSender* sender,
                         const GURL& upload_url,
                         int64_t content_length,
                         UploadRangeCallback callback);

  GetUploadStatusRequest(const GetUploadStatusRequest&) = delete;
  GetUploadStatusRequest& operator=(const GetUploadStatusRequest&) = delete;

  ~GetUploadStatusRequest() override;

 protected:
  // UploadRangeRequestBase overrides.
  void OnRangeRequestComplete(const UploadRangeResponse& response,
                              std::unique_ptr<base::Value> value) override;

 private:
  UploadRangeCallback callback_;
};

//======================= MultipartUploadNewFileDelegate =======================

// This class performs the request for initiating the upload of a new file.
class MultipartUploadNewFileDelegate : public MultipartUploadRequestBase {
 public:
  // |parent_resource_id| should be the resource id of the parent directory.
  // |title| should be set.
  // `converted_mime_type` is the desired MIME type of the file after uploading.
  // This should only be set if it is expected that Drive will convert the
  // uploaded file into another format such as Google Docs:
  // https://developers.google.com/drive/api/guides/manage-uploads#import-docs
  // See also the comments of MultipartUploadRequestBase for more details
  // about the other parameters.
  MultipartUploadNewFileDelegate(
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
      ProgressCallback progress_callback);

  MultipartUploadNewFileDelegate(const MultipartUploadNewFileDelegate&) =
      delete;
  MultipartUploadNewFileDelegate& operator=(
      const MultipartUploadNewFileDelegate&) = delete;

  ~MultipartUploadNewFileDelegate() override;

 protected:
  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;

 private:
  const bool has_modified_date_;
  const bool convert_;
  const DriveApiUrlGenerator url_generator_;
};

//====================== MultipartUploadExistingFileDelegate ===================

// This class performs the request for initiating the upload of a new file.
class MultipartUploadExistingFileDelegate : public MultipartUploadRequestBase {
 public:
  // |parent_resource_id| should be the resource id of the parent directory.
  // |title| should be set.
  // See also the comments of MultipartUploadRequestBase for more details
  // about the other parameters.
  MultipartUploadExistingFileDelegate(base::SequencedTaskRunner* task_runner,
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
                                      ProgressCallback progress_callback);

  MultipartUploadExistingFileDelegate(
      const MultipartUploadExistingFileDelegate&) = delete;
  MultipartUploadExistingFileDelegate& operator=(
      const MultipartUploadExistingFileDelegate&) = delete;

  ~MultipartUploadExistingFileDelegate() override;

 protected:
  // UrlFetchRequestBase overrides.
  std::vector<std::string> GetExtraRequestHeaders() const override;
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;

 private:
  const std::string resource_id_;
  const std::string etag_;
  const bool has_modified_date_;
  const DriveApiUrlGenerator url_generator_;
};

//========================== DownloadFileRequest ==========================

// This class performs the request for downloading of a specified file.
class DownloadFileRequest : public DownloadFileRequestBase {
 public:
  // See also DownloadFileRequestBase's comment for parameters meaning.
  DownloadFileRequest(RequestSender* sender,
                      const DriveApiUrlGenerator& url_generator,
                      const std::string& resource_id,
                      const base::FilePath& output_file_path,
                      DownloadActionCallback download_action_callback,
                      const GetContentCallback& get_content_callback,
                      ProgressCallback progress_callback);

  DownloadFileRequest(const DownloadFileRequest&) = delete;
  DownloadFileRequest& operator=(const DownloadFileRequest&) = delete;

  ~DownloadFileRequest() override;
};

//========================== PermissionsInsertRequest ==========================

// Enumeration type for specifying type of permissions.
enum PermissionType {
  PERMISSION_TYPE_ANYONE,
  PERMISSION_TYPE_DOMAIN,
  PERMISSION_TYPE_GROUP,
  PERMISSION_TYPE_USER,
};

// Enumeration type for specifying the role of permissions.
enum PermissionRole {
  PERMISSION_ROLE_OWNER,
  PERMISSION_ROLE_READER,
  PERMISSION_ROLE_WRITER,
  PERMISSION_ROLE_COMMENTER,
};

// This class performs the request for adding permission on a specified file.
class PermissionsInsertRequest : public EntryActionRequest {
 public:
  // See https://developers.google.com/drive/v2/reference/permissions/insert.
  PermissionsInsertRequest(RequestSender* sender,
                           const DriveApiUrlGenerator& url_generator,
                           EntryActionCallback callback);

  PermissionsInsertRequest(const PermissionsInsertRequest&) = delete;
  PermissionsInsertRequest& operator=(const PermissionsInsertRequest&) = delete;

  ~PermissionsInsertRequest() override;

  void set_id(const std::string& id) { id_ = id; }
  void set_type(PermissionType type) { type_ = type; }
  void set_role(PermissionRole role) { role_ = role; }
  void set_value(const std::string& value) { value_ = value; }

  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string id_;
  PermissionType type_;
  PermissionRole role_;
  std::string value_;
};

//======================= SingleBatchableDelegateRequest =======================

// Request that is operated by single BatchableDelegate.
class SingleBatchableDelegateRequest : public DriveUrlFetchRequestBase {
 public:
  SingleBatchableDelegateRequest(RequestSender* sender,
                                 std::unique_ptr<BatchableDelegate> delegate);

  SingleBatchableDelegateRequest(const SingleBatchableDelegateRequest&) =
      delete;
  SingleBatchableDelegateRequest& operator=(
      const SingleBatchableDelegateRequest&) = delete;

  ~SingleBatchableDelegateRequest() override;

 private:
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;
  std::vector<std::string> GetExtraRequestHeaders() const override;
  void Prepare(PrepareCallback callback) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void OnUploadProgress(int64_t current, int64_t total);
  std::unique_ptr<BatchableDelegate> delegate_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SingleBatchableDelegateRequest> weak_ptr_factory_{this};
};

//========================== BatchUploadRequest ==========================

class BatchUploadChildEntry {
 public:
  explicit BatchUploadChildEntry(BatchableDelegate* request);

  BatchUploadChildEntry(const BatchUploadChildEntry&) = delete;
  BatchUploadChildEntry& operator=(const BatchUploadChildEntry&) = delete;

  ~BatchUploadChildEntry();
  std::unique_ptr<BatchableDelegate> request;
  bool prepared;
  int64_t data_offset;
  int64_t data_size;
};

class BatchUploadRequest : public DriveUrlFetchRequestBase {
 public:
  BatchUploadRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator);

  BatchUploadRequest(const BatchUploadRequest&) = delete;
  BatchUploadRequest& operator=(const BatchUploadRequest&) = delete;

  ~BatchUploadRequest() override;

  // Adds request to the batch request. The instance takes ownership of
  // |request|.
  void AddRequest(BatchableDelegate* request);

  // Completes building batch upload request, and starts to send the request to
  // server. Must add at least one request before calling |Commit|.
  void Commit();

  // Obtains weak pointer of this.
  base::WeakPtr<BatchUploadRequest> GetWeakPtrAsBatchUploadRequest();

  // Set boundary. Only tests can use this method.
  void SetBoundaryForTesting(const std::string& boundary);

  // Obtains reference to RequestSender that owns the request.
  RequestSender* sender() const { return sender_; }

  // Obtains URLGenerator.
  const DriveApiUrlGenerator& url_generator() const { return url_generator_; }

  // UrlFetchRequestBase overrides.
  void Prepare(PrepareCallback callback) override;
  void Cancel() override;
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;
  std::vector<std::string> GetExtraRequestHeaders() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

  // Called by UrlFetchRequestBase to report upload progress.
  void OnUploadProgress(int64_t current, int64_t total);

 private:
  typedef void* RequestID;
  // Obtains corresponding child entry of |request_id|. Returns NULL if the
  // entry is not found.
  std::vector<std::unique_ptr<BatchUploadChildEntry>>::iterator GetChildEntry(
      RequestID request_id);

  // Called after child requests' |Prepare| method.
  void OnChildRequestPrepared(RequestID request_id, ApiErrorCode result);

  // Complete |Prepare| if possible.
  void MayCompletePrepare();

  // Process result for each child.
  void ProcessURLFetchResultsForChild(RequestID id, const std::string& body);

  const raw_ptr<RequestSender> sender_;
  const DriveApiUrlGenerator url_generator_;
  std::vector<std::unique_ptr<BatchUploadChildEntry>> child_requests_;

  PrepareCallback prepare_callback_;
  bool committed_;

  // Boundary of multipart body.
  std::string boundary_;

  // Multipart of child requests.
  ContentTypeAndData upload_content_;

  // Last reported progress value.
  int64_t last_progress_value_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BatchUploadRequest> weak_ptr_factory_{this};
};

}  // namespace drive
}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_API_REQUESTS_H_
