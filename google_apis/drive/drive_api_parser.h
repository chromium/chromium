// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_
#define GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal
}  // namespace base

namespace google_apis {

// About resource represents the account information about the current user.
// https://developers.google.com/drive/v2/reference/about
class AboutResource {
 public:
  AboutResource();
  ~AboutResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AboutResource>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<AboutResource> CreateFrom(const base::Value& value);

  // Returns the largest change ID number.
  int64_t largest_change_id() const { return largest_change_id_; }
  // Returns total number of quota bytes.
  int64_t quota_bytes_total() const { return quota_bytes_total_; }
  // Returns the number of quota bytes used.
  int64_t quota_bytes_used_aggregate() const {
    return quota_bytes_used_aggregate_;
  }
  // Returns root folder ID.
  const std::string& root_folder_id() const { return root_folder_id_; }

  void set_largest_change_id(int64_t largest_change_id) {
    largest_change_id_ = largest_change_id;
  }
  void set_quota_bytes_total(int64_t quota_bytes_total) {
    quota_bytes_total_ = quota_bytes_total;
  }
  void set_quota_bytes_used_aggregate(int64_t quota_bytes_used_aggregate) {
    quota_bytes_used_aggregate_ = quota_bytes_used_aggregate;
  }
  void set_root_folder_id(const std::string& root_folder_id) {
    root_folder_id_ = root_folder_id;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AboutResourceParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int64_t largest_change_id_;
  int64_t quota_bytes_total_;
  int64_t quota_bytes_used_aggregate_;
  std::string root_folder_id_;

  // This class is copyable on purpose.
};

// Capabilities of a Team Drive indicate the permissions granted to the user
// for the Team Drive and items within the Team Drive.
// See "capabilities" in
// https://developers.google.com/drive/v2/reference/teamdrives#resource.
class TeamDriveCapabilities {
 public:
  TeamDriveCapabilities();
  TeamDriveCapabilities(const TeamDriveCapabilities& src);
  ~TeamDriveCapabilities();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TeamDriveCapabilities>* converter);

  // Creates Team Drive resource from parsed JSON.
  static std::unique_ptr<TeamDriveCapabilities>
      CreateFrom(const base::Value& value);

  // Whether the current user can add children to folders in this Team Drive.
  bool can_add_children() const { return can_add_children_; }
  void set_can_add_children(bool can_add_children) {
    can_add_children_ = can_add_children;
  }
  // Whether the current user can comment on files in this Team Drive.
  bool can_comment() const { return can_comment_; }
  void set_can_comment(bool can_comment) { can_comment_ = can_comment; }
  // Whether files in this Team Drive can be copied by the current user.
  bool can_copy() const { return can_copy_; }
  void set_can_copy(bool can_copy) { can_copy_ = can_copy; }
  // Whether this Team Drive can be deleted by the current user.
  bool can_delete_team_drive() const { return can_delete_team_drive_; }
  void set_can_delete_team_drive(bool can_delete_team_drive) {
    can_delete_team_drive_ = can_delete_team_drive;
  }
  // Whether files in this Team Drive can be edited by the current user.
  bool can_download() const { return can_download_; }
  void set_can_download(bool can_download) { can_download_ = can_download; }
  // Whether files in this Team Drive can be edited by current user.
  bool can_edit() const { return can_edit_; }
  void set_can_edit(bool can_edit) { can_edit_ = can_edit; }
  // Whether the current user can list the children of folders in this Team
  // Drive.
  bool can_list_children() const { return can_list_children_; }
  void set_can_list_children(bool can_list_children) {
    can_list_children_ = can_list_children;
  }
  // Whether the current user can add members to this Team Drive or remove them
  // or change their role.
  bool can_manage_members() const { return can_manage_members_; }
  void set_can_manage_members(bool can_manage_members) {
    can_manage_members_ = can_manage_members;
  }
  // Whether the current user has read access to the Revisions resource of files
  // in this Team Drive.
  bool can_read_revisions() const { return can_read_revisions_; }
  void set_can_read_revisions(bool can_read_revisions) {
    can_read_revisions_ = can_read_revisions;
  }
  // Whether the current user can remove children from folders in this Team
  // Drive.
  bool can_remove_children() const { return can_remove_children_; }
  void set_can_remove_children(bool can_remove_children) {
    can_remove_children_ = can_remove_children;
  }
  // Whether files or folders in this Team Drive can be renamed by the current
  // user.
  bool can_rename() const { return can_rename_; }
  void set_can_rename(bool can_rename) { can_rename_ = can_rename; }
  // Whether this Team Drive can be renamed by the current user.
  bool can_rename_team_drive() const { return can_rename_team_drive_; }
  void set_can_rename_team_drive(bool can_rename_team_drive) {
    can_rename_team_drive_ = can_rename_team_drive;
  }
  // Whether files or folders in this Team Drive can be shared by the current
  // user.
  bool can_share() const { return can_share_; }
  void set_can_share(bool can_share) { can_share_ = can_share; }

 private:
  bool can_add_children_;
  bool can_comment_;
  bool can_copy_;
  bool can_delete_team_drive_;
  bool can_download_;
  bool can_edit_;
  bool can_list_children_;
  bool can_manage_members_;
  bool can_read_revisions_;
  bool can_remove_children_;
  bool can_rename_;
  bool can_rename_team_drive_;
  bool can_share_;
};

// Team Drive resource represents the metadata about Team Drive itself, such as
// the name.
class TeamDriveResource {
 public:
  TeamDriveResource();
  ~TeamDriveResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TeamDriveResource>* converter);

  // Creates Team Drive resource from parsed JSON.
  static std::unique_ptr<TeamDriveResource>
      CreateFrom(const base::Value& value);

  // The ID of this Team Drive. The ID is the same as the top-level folder for
  // this Team Drive.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }
  // The name of this Team Drive.
  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }
  // Capabilities the current user has on this Team Drive.
  const TeamDriveCapabilities& capabilities() const { return capabilities_; }
  void set_capabilities(const TeamDriveCapabilities& capabilities) {
    capabilities_ = capabilities;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, TeamDriveResourceParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string id_;
  std::string name_;
  TeamDriveCapabilities capabilities_;
};

// TeamDriveList represents a collection of Team Drives.
// https://developers.google.com/drive/v2/reference/teamdrives/list
class TeamDriveList {
 public:
  TeamDriveList();

  TeamDriveList(const TeamDriveList&) = delete;
  TeamDriveList& operator=(const TeamDriveList&) = delete;

  ~TeamDriveList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TeamDriveList>* converter);

  // Returns true if the |value| has kind field for TeamDriveList.
  static bool HasTeamDriveListKind(const base::Value& value);

  // Creates file list from parsed JSON.
  static std::unique_ptr<TeamDriveList> CreateFrom(const base::Value& value);

  // Returns a page token for the next page of Team Drives.
  const std::string& next_page_token() const { return next_page_token_; }

  void set_next_page_token(const std::string& next_page_token) {
    this->next_page_token_ = next_page_token;
  }

  // Returns a set of Team Drives in this list.
  const std::vector<std::unique_ptr<TeamDriveResource>>& items() const {
    return items_;
  }

  std::vector<std::unique_ptr<TeamDriveResource>>* mutable_items() {
    return &items_;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, TeamDriveListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string next_page_token_;
  std::vector<std::unique_ptr<TeamDriveResource>> items_;
};

// ParentReference represents a directory.
// https://developers.google.com/drive/v2/reference/parents
class ParentReference {
 public:
  ParentReference();
  ~ParentReference();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ParentReference>* converter);

  // Creates parent reference from parsed JSON.
  static std::unique_ptr<ParentReference> CreateFrom(const base::Value& value);

  // Returns the file id of the reference.
  const std::string& file_id() const { return file_id_; }

  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
};

// FileLabels represents labels for file or folder.
// https://developers.google.com/drive/v2/reference/files
class FileLabels {
 public:
  FileLabels();
  ~FileLabels();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileLabels>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<FileLabels> CreateFrom(const base::Value& value);

  // Whether this file has been trashed.
  bool is_trashed() const { return trashed_; }
  // Whether this file is starred by the user.
  bool is_starred() const { return starred_; }

  void set_trashed(bool trashed) { trashed_ = trashed; }
  void set_starred(bool starred) { starred_ = starred; }

 private:
  friend class FileResource;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  bool trashed_;
  bool starred_;
};

// ImageMediaMetadata represents image metadata for a file.
// https://developers.google.com/drive/v2/reference/files
class ImageMediaMetadata {
 public:
  ImageMediaMetadata();
  ~ImageMediaMetadata();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ImageMediaMetadata>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<ImageMediaMetadata> CreateFrom(
      const base::Value& value);

  // Width of the image in pixels.
  int width() const { return width_; }
  // Height of the image in pixels.
  int height() const { return height_; }
  // Rotation of the image in clockwise degrees.
  int rotation() const { return rotation_; }

  void set_width(int width) { width_ = width; }
  void set_height(int height) { height_ = height; }
  void set_rotation(int rotation) { rotation_ = rotation; }

 private:
  friend class FileResource;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int width_;
  int height_;
  int rotation_;
};

// Capabilities of a file resource indicate the permissions granted to the user
// for the file (or items within the folder).
// See "capabilities" in
// https://developers.google.com/drive/v2/reference/files#resource.
class FileResourceCapabilities {
 public:
  FileResourceCapabilities();
  FileResourceCapabilities(const FileResourceCapabilities& src);
  ~FileResourceCapabilities();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileResourceCapabilities>* converter);

  // Creates a FileResourceCapabilities from parsed JSON.
  static std::unique_ptr<FileResourceCapabilities> CreateFrom(
      const base::Value& value);

  // Whether the current user can add children to this folder. This is always
  // false when the item is not a folder.
  bool can_add_children() const { return can_add_children_; }
  void set_can_add_children(bool can_add_children) {
    can_add_children_ = can_add_children;
  }
  // Whether the current user can change the restricted download label of this
  // file.
  bool can_change_restricted_download() const {
    return can_change_restricted_download_;
  }
  void set_can_change_restricted_download(bool can_change_restricted_download) {
    can_change_restricted_download_ = can_change_restricted_download;
  }
  // Whether the current user can comment on this file.
  bool can_comment() const { return can_comment_; }
  void set_can_comment(bool can_comment) { can_comment_ = can_comment; }
  // Whether the current user can copy this file. For a Team Drive item, whether
  // the current user can copy non-folder descendants of this item, or this item
  // itself if it is not a folder.
  bool can_copy() const { return can_copy_; }
  void set_can_copy(bool can_copy) { can_copy_ = can_copy; }
  // Whether the current user can delete this file.
  bool can_delete() const { return can_delete_; }
  void set_can_delete(bool can_delete) { can_delete_ = can_delete; }
  // Whether the current user can download this file.
  bool can_download() const { return can_download_; }
  void set_can_download(bool can_download) { can_download_ = can_download; }
  // Whether the current user can edit this file.
  bool can_edit() const { return can_edit_; }
  void set_can_edit(bool can_edit) { can_edit_ = can_edit; }
  // Whether the current user can list the children of this folder. This is
  // always false when the item is not a folder.
  bool can_list_children() const { return can_list_children_; }
  void set_can_list_children(bool can_list_children) {
    can_list_children_ = can_list_children;
  }
  // Whether the current user can move this item into a Team Drive. If the item
  // is in a Team Drive, this field is equivalent to canMoveTeamDriveItem.
  bool can_move_item_into_team_drive() const {
    return can_move_item_into_team_drive_;
  }
  void set_can_move_item_into_team_drive(bool can_move_item_into_team_drive) {
    can_move_item_into_team_drive_ = can_move_item_into_team_drive;
  }
  // Whether the current user can move this Team Drive item by changing its
  // parent. Note that a request to change the parent for this item may still
  // fail depending on the new parent that is being added. Only populated for
  // Team Drive files.
  bool can_move_team_drive_item() const { return can_move_team_drive_item_; }
  void set_can_move_team_drive_item(bool can_move_team_drive_item) {
    can_move_team_drive_item_ = can_move_team_drive_item;
  }
  // Whether the current user can read the revisions resource of this file. For
  // a Team Drive item, whether revisions of non-folder descendants of this
  // item, or this item itself if it is not a folder, can be read.
  bool can_read_revisions() const { return can_read_revisions_; }
  void set_can_read_revisions(bool can_read_revisions) {
    can_read_revisions_ = can_read_revisions;
  }
  // Whether the current user can read the Team Drive to which this file
  // belongs. Only populated for Team Drive files.
  bool can_read_team_drive() const { return can_read_team_drive_; }
  void set_can_read_team_drive(bool can_read_team_drive) {
    can_read_team_drive_ = can_read_team_drive;
  }
  // Whether the current user can remove children from this folder. This is
  // always false when the item is not a folder.
  bool can_remove_children() const { return can_remove_children_; }
  void set_can_remove_children(bool can_remove_children) {
    can_remove_children_ = can_remove_children;
  }
  // Whether the current user can rename this file.
  bool can_rename() const { return can_rename_; }
  void set_can_rename(bool can_rename) { can_rename_ = can_rename; }
  // Whether the current user can modify the sharing settings for this file.
  bool can_share() const { return can_share_; }
  void set_can_share(bool can_share) { can_share_ = can_share; }
  // Whether the current user can move this file to trash.
  bool can_trash() const { return can_trash_; }
  void set_can_trash(bool can_trash) { can_trash_ = can_trash; }
  // Whether the current user can restore this file from trash.
  bool can_untrash() const { return can_untrash_; }
  void set_can_untrash(bool can_untrash) { can_untrash_ = can_untrash; }

 private:
  bool can_add_children_;
  bool can_change_restricted_download_;
  bool can_comment_;
  bool can_copy_;
  bool can_delete_;
  bool can_download_;
  bool can_edit_;
  bool can_list_children_;
  bool can_move_item_into_team_drive_;
  bool can_move_team_drive_item_;
  bool can_read_revisions_;
  bool can_read_team_drive_;
  bool can_remove_children_;
  bool can_rename_;
  bool can_share_;
  bool can_trash_;
  bool can_untrash_;
};

// FileResource represents a file or folder metadata in Drive.
// https://developers.google.com/drive/v2/reference/files
class FileResource {
 public:
  // Link to open a file resource on a web app with |app_id|.
  struct OpenWithLink {
    std::string app_id;
    GURL open_url;
  };

  FileResource();
  FileResource(const FileResource& other);
  ~FileResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileResource>* converter);

  // Creates file resource from parsed JSON.
  static std::unique_ptr<FileResource> CreateFrom(const base::Value& value);

  // Returns true if this is a directory.
  // Note: "folder" is used elsewhere in this file to match Drive API reference,
  // but outside this file we use "directory" to match HTML5 filesystem API.
  bool IsDirectory() const;

  // Returns true if this is a hosted document.
  // A hosted document is a document in one of Google Docs formats (Documents,
  // Spreadsheets, Slides, ...) whose content is not exposed via the API. It is
  // available only as |alternate_link()| to the document hosted on the server.
  bool IsHostedDocument() const;

  // Returns file ID.  This is unique in all files in Google Drive.
  const std::string& file_id() const { return file_id_; }

  // Returns ETag for this file.
  const std::string& etag() const { return etag_; }

  // Returns the title of this file.
  const std::string& title() const { return title_; }

  // Returns MIME type of this file.
  const std::string& mime_type() const { return mime_type_; }

  // Returns labels for this file.
  const FileLabels& labels() const { return labels_; }

  // Returns image media metadata for this file.
  const ImageMediaMetadata& image_media_metadata() const {
    return image_media_metadata_;
  }

  // Returns created time of this file.
  const base::Time& created_date() const { return created_date_; }

  // Returns modified time of this file.
  const base::Time& modified_date() const { return modified_date_; }

  // Returns last modified time by the user.
  const base::Time& modified_by_me_date() const { return modified_by_me_date_; }

  // Returns last access time by the user.
  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }

  // Returns time when the file was shared with the user.
  const base::Time& shared_with_me_date() const {
    return shared_with_me_date_;
  }

  // Returns the 'shared' attribute of the file.
  bool shared() const { return shared_; }

  // Returns MD5 checksum of this file.
  const std::string& md5_checksum() const { return md5_checksum_; }

  // Returns the size of this file in bytes.
  int64_t file_size() const { return file_size_; }

  // Return the link to open the file in Google editor or viewer.
  // E.g. Google Document, Google Spreadsheet.
  const GURL& alternate_link() const { return alternate_link_; }

  // Returns URL to the share dialog UI.
  const GURL& share_link() const { return share_link_; }

  // Returns parent references (directories) of this file.
  const std::vector<ParentReference>& parents() const { return parents_; }

  // Returns the list of links to open the resource with a web app.
  const std::vector<OpenWithLink>& open_with_links() const {
    return open_with_links_;
  }

  void set_file_id(const std::string& file_id) {
    file_id_ = file_id;
  }
  void set_etag(const std::string& etag) {
    etag_ = etag;
  }
  void set_title(const std::string& title) {
    title_ = title;
  }
  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }
  FileLabels* mutable_labels() {
    return &labels_;
  }
  ImageMediaMetadata* mutable_image_media_metadata() {
    return &image_media_metadata_;
  }
  void set_created_date(const base::Time& created_date) {
    created_date_ = created_date;
  }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }
  void set_modified_by_me_date(const base::Time& modified_by_me_date) {
    modified_by_me_date_ = modified_by_me_date;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }
  void set_shared_with_me_date(const base::Time& shared_with_me_date) {
    shared_with_me_date_ = shared_with_me_date;
  }
  void set_shared(bool shared) {
    shared_ = shared;
  }
  void set_md5_checksum(const std::string& md5_checksum) {
    md5_checksum_ = md5_checksum;
  }
  void set_file_size(int64_t file_size) { file_size_ = file_size; }
  void set_alternate_link(const GURL& alternate_link) {
    alternate_link_ = alternate_link;
  }
  void set_share_link(const GURL& share_link) {
    share_link_ = share_link;
  }
  std::vector<ParentReference>* mutable_parents() { return &parents_; }
  std::vector<OpenWithLink>* mutable_open_with_links() {
    return &open_with_links_;
  }
  // Capabilities the current user has on this file resource.
  const FileResourceCapabilities& capabilities() const { return capabilities_; }
  void set_capabilities(const FileResourceCapabilities& capabilities) {
    capabilities_ = capabilities;
  }

  // ID of the Team Drive the file resides in. Will be empty if the file
  // is not in a team drive.
  const std::string& team_drive_id() const { return team_drive_id_; }
  void set_team_drive_id(const std::string& team_drive_id) {
    team_drive_id_ = team_drive_id;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<FileResource>;
  friend class ChangeResource;
  friend class FileList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
  std::string etag_;
  std::string title_;
  std::string mime_type_;
  FileLabels labels_;
  ImageMediaMetadata image_media_metadata_;
  base::Time created_date_;
  base::Time modified_date_;
  base::Time modified_by_me_date_;
  base::Time last_viewed_by_me_date_;
  base::Time shared_with_me_date_;
  bool shared_;
  std::string md5_checksum_;
  int64_t file_size_;
  GURL alternate_link_;
  GURL share_link_;
  std::vector<ParentReference> parents_;
  std::vector<OpenWithLink> open_with_links_;
  FileResourceCapabilities capabilities_;
  std::string team_drive_id_;
};

// FileList represents a collection of files and folders.
// https://developers.google.com/drive/v2/reference/files/list
class FileList {
 public:
  FileList();

  FileList(const FileList&) = delete;
  FileList& operator=(const FileList&) = delete;

  ~FileList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileList>* converter);

  // Returns true if the |value| has kind field for FileList.
  static bool HasFileListKind(const base::Value& value);

  // Creates file list from parsed JSON.
  static std::unique_ptr<FileList> CreateFrom(const base::Value& value);

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns a set of files in this list.
  const std::vector<std::unique_ptr<FileResource>>& items() const {
    return items_;
  }
  std::vector<std::unique_ptr<FileResource>>* mutable_items() {
    return &items_;
  }

  void set_next_link(const GURL& next_link) {
    next_link_ = next_link;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, FileListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  GURL next_link_;
  std::vector<std::unique_ptr<FileResource>> items_;
};

// ChangeResource represents a change in a file.
// https://developers.google.com/drive/v2/reference/changes
class ChangeResource {
 public:
  enum ChangeType {
    UNKNOWN,  // Uninitialized state.
    FILE,
    TEAM_DRIVE,
  };
  ChangeResource();

  ChangeResource(const ChangeResource&) = delete;
  ChangeResource& operator=(const ChangeResource&) = delete;

  ~ChangeResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ChangeResource>* converter);

  // Creates change resource from parsed JSON.
  static std::unique_ptr<ChangeResource> CreateFrom(const base::Value& value);

  // Returns change ID for this change.  This is a monotonically increasing
  // number.
  int64_t change_id() const { return change_id_; }

  // Returns whether this is a change of a file or a team drive.
  ChangeType type() const { return type_; }

  // Returns a string file ID for corresponding file of the change.
  // Valid only when type == FILE.
  const std::string& file_id() const {
    DCHECK_EQ(FILE, type_);
    return file_id_;
  }

  // Returns true if this file is deleted in the change.
  bool is_deleted() const { return deleted_; }

  // Returns FileResource of the file which the change refers to.
  // Valid only when type == FILE.
  const FileResource* file() const {
    DCHECK_EQ(FILE, type_);
    return file_.get();
  }
  FileResource* mutable_file() {
    DCHECK_EQ(FILE, type_);
    return file_.get();
  }

  // Returns TeamDriveResource which the change refers to.
  // Valid only when type == TEAM_DRIVE.
  const TeamDriveResource* team_drive() const {
    DCHECK_EQ(TEAM_DRIVE, type_);
    return team_drive_.get();
  }
  TeamDriveResource* mutable_team_drive() {
    DCHECK_EQ(TEAM_DRIVE, type_);
    return team_drive_.get();
  }

  // Returns the ID of the Team Drive. Valid only when type == TEAM_DRIVE.
  const std::string& team_drive_id() const {
    DCHECK_EQ(TEAM_DRIVE, type_);
    return team_drive_id_;
  }

  // Returns the time of this modification.
  const base::Time& modification_date() const { return modification_date_; }

  void set_change_id(int64_t change_id) { change_id_ = change_id; }
  void set_type(ChangeType type) { type_ = type; }
  void set_file_id(const std::string& file_id) {
    file_id_ = file_id;
  }
  void set_deleted(bool deleted) {
    deleted_ = deleted;
  }
  void set_file(std::unique_ptr<FileResource> file) { file_ = std::move(file); }
  void set_team_drive(std::unique_ptr<TeamDriveResource> team_drive) {
    team_drive_ = std::move(team_drive);
  }
  void set_team_drive_id(const std::string& team_drive_id) {
    team_drive_id_ = team_drive_id;
  }
  void set_modification_date(const base::Time& modification_date) {
    modification_date_ = modification_date;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<ChangeResource>;
  friend class ChangeList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  // Extracts the change type from the given string. Returns false and does
  // not change |result| when |type_name| has an unrecognizable value.
  static bool GetType(std::string_view type_name,
                      ChangeResource::ChangeType* result);

  int64_t change_id_;
  ChangeType type_;
  std::string file_id_;
  bool deleted_;
  std::unique_ptr<FileResource> file_;
  base::Time modification_date_;
  std::string team_drive_id_;
  std::unique_ptr<TeamDriveResource> team_drive_;
};

// ChangeList represents a set of changes in the drive.
// https://developers.google.com/drive/v2/reference/changes/list
class ChangeList {
 public:
  ChangeList();

  ChangeList(const ChangeList&) = delete;
  ChangeList& operator=(const ChangeList&) = delete;

  ~ChangeList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ChangeList>* converter);

  // Returns true if the |value| has kind field for ChangeList.
  static bool HasChangeListKind(const base::Value& value);

  // Creates change list from parsed JSON.
  static std::unique_ptr<ChangeList> CreateFrom(const base::Value& value);

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns the largest change ID number.
  int64_t largest_change_id() const { return largest_change_id_; }

  // Returns the new start page token, only if the end of current change list
  // was reached.
  const std::string& new_start_page_token() const {
    return new_start_page_token_;
  }

  // Returns a set of changes in this list.
  const std::vector<std::unique_ptr<ChangeResource>>& items() const {
    return items_;
  }
  std::vector<std::unique_ptr<ChangeResource>>* mutable_items() {
    return &items_;
  }

  void set_next_link(const GURL& next_link) {
    next_link_ = next_link;
  }
  void set_largest_change_id(int64_t largest_change_id) {
    largest_change_id_ = largest_change_id;
  }
  void set_new_start_page_token(const std::string& new_start_page_token) {
    new_start_page_token_ = new_start_page_token;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, ChangeListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  GURL next_link_;
  int64_t largest_change_id_;
  std::string new_start_page_token_;
  std::vector<std::unique_ptr<ChangeResource>> items_;
};

// StartPageToken represets the starting pageToken for listing changes in the
// users corpus or in a team drive.
// https://developers.google.com/drive/v2/reference/changes/getStartPageToken
class StartPageToken {
 public:
  StartPageToken();
  ~StartPageToken();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<StartPageToken>* converter);

  // Creates StartPageToken from parsed JSON
  static std::unique_ptr<StartPageToken> CreateFrom(const base::Value& value);

  const std::string& start_page_token() const { return start_page_token_; }

  void set_start_page_token(const std::string& token) {
    start_page_token_ = token;
  }

 private:
  // Pareses and initializes data members from content of |value|.
  // Returns false if parsing fails.
  bool Parse(const base::Value& value);

  std::string start_page_token_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_
