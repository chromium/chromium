// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_API_URL_GENERATOR_H_
#define GOOGLE_APIS_DRIVE_DRIVE_API_URL_GENERATOR_H_

#include <stdint.h>

#include <string>

#include "url/gurl.h"

namespace google_apis {

// This enum class is used to express a corpora parameter configuration for
// Files:list.
enum class FilesListCorpora {
  // 'default': The user's subscribed items.
  DEFAULT,
  // 'teamDrives': A Team Drive.
  TEAM_DRIVE,
  // 'default,allTeamDrives': All Team Drives and the user's subscribed items.
  ALL_TEAM_DRIVES
};

// This class is used to generate URLs for communicating with drive api
// servers for production, and a local server for testing.
class DriveApiUrlGenerator {
 public:
  // |base_url| is the path to the target drive api server.
  // Note that this is an injecting point for a testing server.
  DriveApiUrlGenerator(const GURL& base_url, const GURL& base_thumbnail_url);
  DriveApiUrlGenerator(const DriveApiUrlGenerator& src);
  ~DriveApiUrlGenerator();

  // The base URL for the thumbnail download server for production.
  static const char kBaseThumbnailUrlForProduction[];

  // Returns a URL to invoke "About: get" method.
  GURL GetAboutGetUrl() const;

  // Returns a URL to fetch a file metadata.
  GURL GetFilesGetUrl(const std::string& file_id,
                      const GURL& embed_origin) const;

  // Returns a URL to create a resource.
  GURL GetFilesInsertUrl(const std::string& visibility) const;

  // Returns a URL to patch file metadata.
  GURL GetFilesPatchUrl(const std::string& file_id,
                        bool set_modified_date,
                        bool update_viewed_date) const;

  // Returns a URL to copy a resource specified by |file_id|.
  GURL GetFilesCopyUrl(const std::string& file_id,
                       const std::string& visibility) const;

  // Returns a URL to fetch file list.
  GURL GetFilesListUrl(int max_results,
                       const std::string& page_token,
                       FilesListCorpora corpora,
                       const std::string& team_drive_id,
                       const std::string& q) const;

  // Returns a URL to delete a resource with the given |file_id|.
  GURL GetFilesDeleteUrl(const std::string& file_id) const;

  // Returns a URL to trash a resource with the given |file_id|.
  GURL GetFilesTrashUrl(const std::string& file_id) const;

  // Returns a URL to invoke "TeamDrives: list" method.
  GURL GetTeamDriveListUrl(int max_results,
                           const std::string& page_token) const;

  // Returns a URL to fetch a list of changes.
  GURL GetChangesListUrl(bool include_deleted,
                         int max_results,
                         const std::string& page_token,
                         int64_t start_change_id,
                         const std::string& team_dirve_id) const;

  // Returns a URL to add a resource to a directory with |folder_id|.
  GURL GetChildrenInsertUrl(const std::string& folder_id) const;

  // Returns a URL to remove a resource with |child_id| from a directory
  // with |folder_id|.
  GURL GetChildrenDeleteUrl(const std::string& child_id,
                            const std::string& folder_id) const;

  // Returns a URL to initiate "resumable upload" of a new file that uploads
  // chunked data by multiple HTTP posts.
  GURL GetInitiateUploadNewFileUrl(bool set_modified_date) const;

  // Returns a URL to initiate "resumable upload" of an existing file specified
  // by |resource_id| that uploads chunked data by multiple HTTP posts.
  GURL GetInitiateUploadExistingFileUrl(const std::string& resource_id,
                                        bool set_modified_date) const;

  // Returns a URL for "multipart upload" of a new file that sends both metadata
  // and file content in a single HTTP post.
  GURL GetMultipartUploadNewFileUrl(bool set_modified_date, bool convert) const;

  // Returns a URL for "multipart upload" of an existing file specified by
  // |resource_id| that sends both metadata and file content in a single HTTP
  // post.
  GURL GetMultipartUploadExistingFileUrl(const std::string& resource_id,
                                         bool set_modified_date) const;

  // Generates a URL for downloading a file.
  GURL GenerateDownloadFileUrl(const std::string& resource_id) const;

  // Generates a URL for adding permissions.
  GURL GetPermissionsInsertUrl(const std::string& resource_id) const;

  // Generates a URL for a thumbnail with specified dimensions. Set |crop| to
  // true to get a cropped thumbnail in the dimensions.
  GURL GetThumbnailUrl(const std::string& resource_id,
                       int width,
                       int height,
                       bool crop) const;

  // Generates a URL for batch upload.
  GURL GetBatchUploadUrl() const;

  // Returns a URL for the start page token for a |team_drive|. |team_drive|
  // may be empty, in which case the start page token will be returned for
  // the users changes.
  GURL GetStartPageTokenUrl(const std::string& team_drive) const;

 private:
  const GURL base_url_;
  const GURL base_download_url_;
  const GURL base_thumbnail_url_;

  // This class is copyable hence no DISALLOW_COPY_AND_ASSIGN here.
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_API_URL_GENERATOR_H_
