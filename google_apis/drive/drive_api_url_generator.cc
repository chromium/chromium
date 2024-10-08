// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_url_generator.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace {

// Hard coded URLs for communication with a google drive server.
// TODO(yamaguchi): Make a utility function to compose some of these URLs by a
// version and a resource name.
const char kDriveV2AboutUrl[] = "drive/v2/about";
const char kDriveV2ChangelistUrl[] = "drive/v2/changes";
const char kDriveV2StartPageTokenUrl[] = "drive/v2/changes/startPageToken";
const char kDriveV2FilesUrl[] = "drive/v2/files";
const char kDriveV2FileUrlPrefix[] = "drive/v2/files/";
const char kDriveV2ChildrenUrlFormat[] = "drive/v2/files/%s/children";
const char kDriveV2ChildrenUrlForRemovalFormat[] =
    "drive/v2/files/%s/children/%s";
const char kDriveV2FileCopyUrlFormat[] = "drive/v2/files/%s/copy";
const char kDriveV2FileDeleteUrlFormat[] = "drive/v2/files/%s";
const char kDriveV2FileTrashUrlFormat[] = "drive/v2/files/%s/trash";
const char kDriveV2UploadNewFileUrl[] = "upload/drive/v2/files";
const char kDriveV2UploadExistingFileUrlPrefix[] = "upload/drive/v2/files/";
const char kDriveV2BatchUploadUrl[] = "upload/drive";
const char kDriveV2PermissionsUrlFormat[] = "drive/v2/files/%s/permissions";
const char kDriveV2DownloadUrlFormat[] = "drive/v2/files/%s?alt=media";
const char kDriveV2ThumbnailUrlFormat[] = "d/%s=w%d-h%d%s";
const char kDriveV2TeamDrivesUrl[] = "drive/v2/teamdrives";

const char kIncludeTeamDriveItems[] = "includeTeamDriveItems";
const char kSupportsTeamDrives[] = "supportsTeamDrives";
const char kCorpora[] = "corpora";
const char kCorporaAllTeamDrives[] = "default,allTeamDrives";
const char kCorporaDefault[] = "default";
const char kCorporaTeamDrive[] = "teamDrive";
const char kTeamDriveId[] = "teamDriveId";

GURL AddResumableUploadParam(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, "uploadType", "resumable");
}

GURL AddMultipartUploadParam(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, "uploadType", "multipart");
}

const char* GetCorporaString(FilesListCorpora corpora) {
  switch (corpora) {
    case FilesListCorpora::DEFAULT:
      return kCorporaDefault;
    case FilesListCorpora::TEAM_DRIVE:
      return kCorporaTeamDrive;
    case FilesListCorpora::ALL_TEAM_DRIVES:
      return kCorporaAllTeamDrives;
  }
  NOTREACHED();
}

}  // namespace

DriveApiUrlGenerator::DriveApiUrlGenerator(const GURL& base_url,
                                           const GURL& base_thumbnail_url)
    : base_url_(base_url), base_thumbnail_url_(base_thumbnail_url) {
  // Do nothing.
}

DriveApiUrlGenerator::DriveApiUrlGenerator(const DriveApiUrlGenerator& src) =
    default;

DriveApiUrlGenerator::~DriveApiUrlGenerator() {
  // Do nothing.
}

const char DriveApiUrlGenerator::kBaseThumbnailUrlForProduction[] =
    "https://lh3.googleusercontent.com";

GURL DriveApiUrlGenerator::GetAboutGetUrl() const {
  return base_url_.Resolve(kDriveV2AboutUrl);
}

GURL DriveApiUrlGenerator::GetFilesGetUrl(const std::string& file_id,
                                          const GURL& embed_origin) const {
  GURL url =
      base_url_.Resolve(kDriveV2FileUrlPrefix + base::EscapePath(file_id));
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");

  if (!embed_origin.is_empty()) {
    // Construct a valid serialized embed origin from an url, according to
    // WD-html5-20110525. Such string has to be built manually, since
    // GURL::spec() always adds the trailing slash. Moreover, ports are
    // currently not supported.
    DCHECK(!embed_origin.has_port());
    DCHECK(!embed_origin.has_path() || embed_origin.path() == "/");
    const std::string serialized_embed_origin =
        embed_origin.scheme() + "://" + embed_origin.host();
    url = net::AppendOrReplaceQueryParameter(
        url, "embedOrigin", serialized_embed_origin);
  }
  return url;
}

GURL DriveApiUrlGenerator::GetFilesInsertUrl(
    const std::string& visibility) const {
  GURL url =  base_url_.Resolve(kDriveV2FilesUrl);

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  if (!visibility.empty())
    url = net::AppendOrReplaceQueryParameter(url, "visibility", visibility);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesPatchUrl(const std::string& file_id,
                                            bool set_modified_date,
                                            bool update_viewed_date) const {
  GURL url =
      base_url_.Resolve(kDriveV2FileUrlPrefix + base::EscapePath(file_id));

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  // updateViewedDate is "true" by default.
  if (!update_viewed_date)
    url = net::AppendOrReplaceQueryParameter(url, "updateViewedDate", "false");

  return url;
}

GURL DriveApiUrlGenerator::GetFilesCopyUrl(
    const std::string& file_id,
    const std::string& visibility) const {
  GURL url = base_url_.Resolve(base::StringPrintf(
      kDriveV2FileCopyUrlFormat, base::EscapePath(file_id).c_str()));

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  if (!visibility.empty())
    url = net::AppendOrReplaceQueryParameter(url, "visibility", visibility);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesListUrl(int max_results,
                                           const std::string& page_token,
                                           FilesListCorpora corpora,
                                           const std::string& team_drive_id,
                                           const std::string& q) const {
  GURL url = base_url_.Resolve(kDriveV2FilesUrl);
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  url = net::AppendOrReplaceQueryParameter(url, kIncludeTeamDriveItems, "true");
  url = net::AppendOrReplaceQueryParameter(url, kCorpora,
                                           GetCorporaString(corpora));
  if (!team_drive_id.empty())
    url = net::AppendOrReplaceQueryParameter(url, kTeamDriveId, team_drive_id);
  // maxResults is 100 by default.
  if (max_results != 100) {
    url = net::AppendOrReplaceQueryParameter(url, "maxResults",
                                             base::NumberToString(max_results));
  }

  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  if (!q.empty())
    url = net::AppendOrReplaceQueryParameter(url, "q", q);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesDeleteUrl(const std::string& file_id) const {
  GURL url = base_url_.Resolve(base::StringPrintf(
      kDriveV2FileDeleteUrlFormat, base::EscapePath(file_id).c_str()));
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  return url;
}

GURL DriveApiUrlGenerator::GetFilesTrashUrl(const std::string& file_id) const {
  GURL url = base_url_.Resolve(base::StringPrintf(
      kDriveV2FileTrashUrlFormat, base::EscapePath(file_id).c_str()));
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  return url;
}

GURL DriveApiUrlGenerator::GetChangesListUrl(
    bool include_deleted,
    int max_results,
    const std::string& page_token,
    int64_t start_change_id,
    const std::string& team_drive_id) const {
  DCHECK_GE(start_change_id, 0);

  GURL url = base_url_.Resolve(kDriveV2ChangelistUrl);
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  url = net::AppendOrReplaceQueryParameter(url, kIncludeTeamDriveItems, "true");
  if (!team_drive_id.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kTeamDriveId, team_drive_id);
  }
  // includeDeleted is "true" by default.
  if (!include_deleted)
    url = net::AppendOrReplaceQueryParameter(url, "includeDeleted", "false");

  // maxResults is "100" by default.
  if (max_results != 100) {
    url = net::AppendOrReplaceQueryParameter(url, "maxResults",
                                             base::NumberToString(max_results));
  }

  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  if (start_change_id > 0)
    url = net::AppendOrReplaceQueryParameter(
        url, "startChangeId", base::NumberToString(start_change_id));

  return url;
}

GURL DriveApiUrlGenerator::GetChildrenInsertUrl(
    const std::string& file_id) const {
  GURL url = base_url_.Resolve(base::StringPrintf(
      kDriveV2ChildrenUrlFormat, base::EscapePath(file_id).c_str()));
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  return url;
}

GURL DriveApiUrlGenerator::GetChildrenDeleteUrl(
    const std::string& child_id, const std::string& folder_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2ChildrenUrlForRemovalFormat, base::EscapePath(folder_id).c_str(),
      base::EscapePath(child_id).c_str()));
}

GURL DriveApiUrlGenerator::GetInitiateUploadNewFileUrl(
    bool set_modified_date) const {
  GURL url = AddResumableUploadParam(
      base_url_.Resolve(kDriveV2UploadNewFileUrl));

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GetInitiateUploadExistingFileUrl(
    const std::string& resource_id,
    bool set_modified_date) const {
  GURL url = base_url_.Resolve(kDriveV2UploadExistingFileUrlPrefix +
                               base::EscapePath(resource_id));
  url = AddResumableUploadParam(url);

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GetMultipartUploadNewFileUrl(bool set_modified_date,
                                                        bool convert) const {
  GURL url = AddMultipartUploadParam(
      base_url_.Resolve(kDriveV2UploadNewFileUrl));

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");
  if (convert) {
    url = net::AppendOrReplaceQueryParameter(url, "convert", "true");
  }

  return url;
}

GURL DriveApiUrlGenerator::GetMultipartUploadExistingFileUrl(
    const std::string& resource_id,
    bool set_modified_date) const {
  GURL url = base_url_.Resolve(kDriveV2UploadExistingFileUrlPrefix +
                               base::EscapePath(resource_id));
  url = AddMultipartUploadParam(url);

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GenerateDownloadFileUrl(
    const std::string& resource_id) const {
  GURL url = base_url_.Resolve(base::StringPrintf(
      kDriveV2DownloadUrlFormat, base::EscapePath(resource_id).c_str()));
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  return url;
}

GURL DriveApiUrlGenerator::GetPermissionsInsertUrl(
    const std::string& resource_id) const {
  GURL url = base_url_.Resolve(base::StringPrintf(
      kDriveV2PermissionsUrlFormat, base::EscapePath(resource_id).c_str()));
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  return url;
}

GURL DriveApiUrlGenerator::GetThumbnailUrl(const std::string& resource_id,
                                           int width,
                                           int height,
                                           bool crop) const {
  return base_thumbnail_url_.Resolve(base::StringPrintf(
      kDriveV2ThumbnailUrlFormat, base::EscapePath(resource_id).c_str(), width,
      height, crop ? "-c" : ""));
}

GURL DriveApiUrlGenerator::GetBatchUploadUrl() const {
  GURL url = base_url_.Resolve(kDriveV2BatchUploadUrl);
  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
  return url;
}

GURL DriveApiUrlGenerator::GetTeamDriveListUrl(
    int max_results,
    const std::string& page_token) const {
  GURL url = base_url_.Resolve(kDriveV2TeamDrivesUrl);

  // maxResults is 10 by default.
  if (max_results != 10) {
    url = net::AppendOrReplaceQueryParameter(url, "maxResults",
                                             base::NumberToString(max_results));
  }
  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  return url;
}

GURL DriveApiUrlGenerator::GetStartPageTokenUrl(
    const std::string& team_drive) const {
  GURL url = base_url_.Resolve(kDriveV2StartPageTokenUrl);

  url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");

  if (!team_drive.empty())
    url = net::AppendOrReplaceQueryParameter(url, kTeamDriveId, team_drive);

  return url;
}

}  // namespace google_apis
