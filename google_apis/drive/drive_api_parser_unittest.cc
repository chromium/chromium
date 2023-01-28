// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_parser.h"

#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/test_util.h"
#include "google_apis/common/time_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

// Test about resource parsing.
TEST(DriveAPIParserTest, AboutResourceParser) {
  std::string error;
  std::unique_ptr<base::Value> document =
      test_util::LoadJSONFile("drive/about.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<AboutResource> resource(new AboutResource());
  EXPECT_TRUE(resource->Parse(*document));

  EXPECT_EQ("0AIv7G8yEYAWHUk9123", resource->root_folder_id());
  EXPECT_EQ(5368709120LL, resource->quota_bytes_total());
  EXPECT_EQ(1073741824LL, resource->quota_bytes_used_aggregate());
  EXPECT_EQ(8177LL, resource->largest_change_id());
}

// Test Team Drive resource parsing.
TEST(DriveAPIParserTest, TeamDriveResourceParser) {
  std::unique_ptr<base::Value> document =
      test_util::LoadJSONFile("drive/team_drive.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<TeamDriveResource> resource(new TeamDriveResource());
  EXPECT_TRUE(resource->Parse(*document));

  EXPECT_EQ("TestTeamDriveId", resource->id());
  EXPECT_EQ("My Team", resource->name());
  const TeamDriveCapabilities& capabilities = resource->capabilities();
  EXPECT_TRUE(capabilities.can_add_children());
  EXPECT_TRUE(capabilities.can_comment());
  EXPECT_TRUE(capabilities.can_copy());
  EXPECT_TRUE(capabilities.can_delete_team_drive());
  EXPECT_TRUE(capabilities.can_download());
  EXPECT_TRUE(capabilities.can_edit());
  EXPECT_TRUE(capabilities.can_list_children());
  EXPECT_TRUE(capabilities.can_manage_members());
  EXPECT_TRUE(capabilities.can_read_revisions());
  EXPECT_TRUE(capabilities.can_remove_children());
  EXPECT_TRUE(capabilities.can_rename());
  EXPECT_TRUE(capabilities.can_rename_team_drive());
  EXPECT_TRUE(capabilities.can_share());
}

TEST(DriveAPIParserTest, TeamDriveListParser) {
  std::unique_ptr<base::Value> document(
      test_util::LoadJSONFile("drive/team_drive_list.json"));
  ASSERT_TRUE(document.get());
  EXPECT_TRUE(TeamDriveList::HasTeamDriveListKind(*document));

  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<TeamDriveList> resource(new TeamDriveList());
  EXPECT_TRUE(resource->Parse(*document));
  EXPECT_EQ(3U, resource->items().size());
  EXPECT_EQ("theNextPageToken", resource->next_page_token());
}

// Test file list parsing.
TEST(DriveAPIParserTest, FileListParser) {
  std::string error;
  std::unique_ptr<base::Value> document =
      test_util::LoadJSONFile("drive/filelist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<FileList> filelist(new FileList);
  EXPECT_TRUE(filelist->Parse(*document));

  EXPECT_EQ(GURL("https://www.googleapis.com/drive/v2/files?pageToken=EAIaggEL"
                 "EgA6egpi96It9mH_____f_8AAP__AAD_okhU-cHLz83KzszMxsjMzs_RyNGJ"
                 "nridyrbHs7u9tv8AAP__AP7__n__AP8AokhU-cHLz83KzszMxsjMzs_RyNGJ"
                 "nridyrbHs7u9tv8A__4QZCEiXPTi_wtIgTkAAAAAngnSXUgCDEAAIgsJPgar"
                 "t10AAAAABC"),
            filelist->next_link());

  ASSERT_EQ(3U, filelist->items().size());
  // Check file 1 (a regular file)
  const FileResource& file1 = *filelist->items()[0];
  EXPECT_EQ("0B4v7G8yEYAWHUmRrU2lMS2hLABC", file1.file_id());
  EXPECT_EQ("\"WtRjAPZWbDA7_fkFjc5ojsEvDEF/MTM0MzM2NzgwMDIXYZ\"", file1.etag());
  EXPECT_EQ("My first file data", file1.title());
  EXPECT_EQ("application/octet-stream", file1.mime_type());

  EXPECT_FALSE(file1.labels().is_trashed());
  EXPECT_FALSE(file1.labels().is_starred());
  EXPECT_FALSE(file1.shared());

  EXPECT_EQ(640, file1.image_media_metadata().width());
  EXPECT_EQ(480, file1.image_media_metadata().height());
  EXPECT_EQ(90, file1.image_media_metadata().rotation());

  base::Time created_time;
  ASSERT_TRUE(
      util::GetTimeFromString("2012-07-24T08:51:16.570Z", &created_time));
  EXPECT_EQ(created_time, file1.created_date());

  base::Time modified_time;
  ASSERT_TRUE(
      util::GetTimeFromString("2012-07-27T05:43:20.269Z", &modified_time));
  EXPECT_EQ(modified_time, file1.modified_date());

  base::Time modified_by_me_time;
  ASSERT_TRUE(util::GetTimeFromString("2012-07-27T05:30:20.269Z",
                                      &modified_by_me_time));
  EXPECT_EQ(modified_by_me_time, file1.modified_by_me_date());
  EXPECT_EQ("team_drive_id_1", file1.team_drive_id());

  ASSERT_EQ(1U, file1.parents().size());
  EXPECT_EQ("0B4v7G8yEYAWHYW1OcExsUVZLABC", file1.parents()[0].file_id());

  EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", file1.md5_checksum());
  EXPECT_EQ(1000U, file1.file_size());
  EXPECT_FALSE(file1.IsHostedDocument());

  EXPECT_EQ(GURL("https://docs.google.com/file/d/"
                 "0B4v7G8yEYAWHUmRrU2lMS2hLABC/edit"),
            file1.alternate_link());
  ASSERT_EQ(1U, file1.open_with_links().size());
  EXPECT_EQ("1234567890", file1.open_with_links()[0].app_id);
  EXPECT_EQ(GURL("http://open_with_link/url"),
            file1.open_with_links()[0].open_url);

  const FileResourceCapabilities& capabilities = file1.capabilities();
  EXPECT_FALSE(capabilities.can_add_children());
  EXPECT_TRUE(capabilities.can_change_restricted_download());
  EXPECT_TRUE(capabilities.can_comment());
  EXPECT_TRUE(capabilities.can_copy());
  EXPECT_TRUE(capabilities.can_delete());
  EXPECT_TRUE(capabilities.can_download());
  EXPECT_TRUE(capabilities.can_edit());
  EXPECT_FALSE(capabilities.can_list_children());
  EXPECT_TRUE(capabilities.can_move_item_into_team_drive());
  EXPECT_FALSE(capabilities.can_move_team_drive_item());
  EXPECT_TRUE(capabilities.can_read_revisions());
  EXPECT_FALSE(capabilities.can_read_team_drive());
  EXPECT_FALSE(capabilities.can_remove_children());
  EXPECT_TRUE(capabilities.can_rename());
  EXPECT_TRUE(capabilities.can_share());
  EXPECT_TRUE(capabilities.can_trash());
  EXPECT_TRUE(capabilities.can_untrash());

  // Check file 2 (a Google Document)
  const FileResource& file2 = *filelist->items()[1];
  EXPECT_EQ("Test Google Document", file2.title());
  EXPECT_EQ("application/vnd.google-apps.document", file2.mime_type());
  EXPECT_EQ("team_drive_id_2", file2.team_drive_id());

  EXPECT_TRUE(file2.labels().is_trashed());
  EXPECT_TRUE(file2.labels().is_starred());
  EXPECT_TRUE(file2.shared());

  EXPECT_EQ(-1, file2.image_media_metadata().width());
  EXPECT_EQ(-1, file2.image_media_metadata().height());
  EXPECT_EQ(-1, file2.image_media_metadata().rotation());

  base::Time shared_with_me_time;
  ASSERT_TRUE(util::GetTimeFromString("2012-07-27T04:54:11.030Z",
                                      &shared_with_me_time));
  EXPECT_EQ(shared_with_me_time, file2.shared_with_me_date());

  EXPECT_EQ(-1, file2.file_size());
  EXPECT_TRUE(file2.IsHostedDocument());

  ASSERT_EQ(0U, file2.parents().size());

  EXPECT_EQ(0U, file2.open_with_links().size());
  EXPECT_EQ(GURL("https://drive.google.com/share"
                 "?id=1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC"
                 "&embedOrigin=chrome-extension://test&hl=ja"),
            file2.share_link());

  // Check file 3 (a folder)
  const FileResource& file3 = *filelist->items()[2];
  EXPECT_EQ(-1, file3.file_size());
  EXPECT_FALSE(file3.IsHostedDocument());
  EXPECT_EQ("TestFolder", file3.title());
  EXPECT_EQ("application/vnd.google-apps.folder", file3.mime_type());
  EXPECT_EQ("", file3.team_drive_id());
  ASSERT_TRUE(file3.IsDirectory());
  EXPECT_FALSE(file3.shared());

  ASSERT_EQ(1U, file3.parents().size());
  EXPECT_EQ("0AIv7G8yEYAWHUk9ABC", file3.parents()[0].file_id());
  EXPECT_EQ(0U, file3.open_with_links().size());
}

// Test change list parsing.
TEST(DriveAPIParserTest, ChangeListParser) {
  std::string error;
  std::unique_ptr<base::Value> document =
      test_util::LoadJSONFile("drive/changelist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<ChangeList> changelist(new ChangeList);
  EXPECT_TRUE(changelist->Parse(*document));

  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes?pageToken=8929",
            changelist->next_link().spec());
  EXPECT_EQ(13664, changelist->largest_change_id());

  ASSERT_EQ(5U, changelist->items().size());

  const ChangeResource& change1 = *changelist->items()[0];
  EXPECT_EQ(8421, change1.change_id());
  EXPECT_EQ(ChangeResource::FILE, change1.type());
  EXPECT_FALSE(change1.is_deleted());
  EXPECT_EQ("1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC", change1.file_id());
  EXPECT_EQ(change1.file_id(), change1.file()->file_id());
  EXPECT_FALSE(change1.file()->shared());
  EXPECT_EQ(change1.file()->modified_date(), change1.modification_date());

  const ChangeResource& change2 = *changelist->items()[1];
  EXPECT_EQ(8424, change2.change_id());
  EXPECT_EQ(ChangeResource::FILE, change2.type());
  EXPECT_FALSE(change2.is_deleted());
  EXPECT_EQ("0B4v7G8yEYAWHUmRrU2lMS2hLABC", change2.file_id());
  EXPECT_EQ(change2.file_id(), change2.file()->file_id());
  EXPECT_TRUE(change2.file()->shared());
  EXPECT_EQ(change2.file()->modified_date(), change2.modification_date());

  const ChangeResource& change3 = *changelist->items()[2];
  EXPECT_EQ(8429, change3.change_id());
  EXPECT_EQ(ChangeResource::FILE, change3.type());
  EXPECT_FALSE(change3.is_deleted());
  EXPECT_EQ("0B4v7G8yEYAWHYW1OcExsUVZLABC", change3.file_id());
  EXPECT_EQ(change3.file_id(), change3.file()->file_id());
  EXPECT_FALSE(change3.file()->shared());
  EXPECT_EQ(change3.file()->modified_date(), change3.modification_date());

  // Deleted entry.
  const ChangeResource& change4 = *changelist->items()[3];
  EXPECT_EQ(8430, change4.change_id());
  EXPECT_EQ(ChangeResource::FILE, change4.type());
  EXPECT_EQ("ABCv7G8yEYAWHc3Y5X0hMSkJYXYZ", change4.file_id());
  EXPECT_TRUE(change4.is_deleted());
  base::Time modification_time;
  ASSERT_TRUE(
      util::GetTimeFromString("2012-07-27T12:34:56.789Z", &modification_time));
  EXPECT_EQ(modification_time, change4.modification_date());

  // Team Drive entry.
  const ChangeResource& change5 = *changelist->items()[4];
  EXPECT_EQ(8431, change5.change_id());
  EXPECT_EQ(ChangeResource::TEAM_DRIVE, change5.type());
  EXPECT_EQ("id-of-team-drive-test-data", change5.team_drive()->id());
  EXPECT_EQ("id-of-team-drive-test-data", change5.team_drive_id());
  EXPECT_FALSE(change5.is_deleted());
  ASSERT_TRUE(
      util::GetTimeFromString("2017-07-27T12:34:56.789Z", &modification_time));
  EXPECT_EQ(modification_time, change5.modification_date());
  // capabilities resource inside team_drive should be parsed
  EXPECT_TRUE(change5.team_drive()->capabilities().can_share());
}

// Test change list parsing.
TEST(DriveAPIParserTest, ChangeListParserWithStartToken) {
  std::string error;
  std::unique_ptr<base::Value> document = test_util::LoadJSONFile(
      "drive/changelist_with_new_start_page_token.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<ChangeList> changelist = ChangeList::CreateFrom(*document);
  EXPECT_TRUE(changelist);

  EXPECT_EQ("13665", changelist->new_start_page_token());
  EXPECT_EQ(13664, changelist->largest_change_id());

  ASSERT_EQ(1U, changelist->items().size());

  const ChangeResource& change1 = *changelist->items()[0];
  EXPECT_EQ(8421, change1.change_id());
  EXPECT_EQ(ChangeResource::FILE, change1.type());
  EXPECT_FALSE(change1.is_deleted());
  EXPECT_EQ("1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC", change1.file_id());
  EXPECT_EQ(change1.file_id(), change1.file()->file_id());
  EXPECT_FALSE(change1.file()->shared());
  EXPECT_EQ(change1.file()->modified_date(), change1.modification_date());
}

TEST(DriveAPIParserTest, HasKind) {
  std::unique_ptr<base::Value> change_list_json(
      test_util::LoadJSONFile("drive/changelist.json"));
  std::unique_ptr<base::Value> file_list_json(
      test_util::LoadJSONFile("drive/filelist.json"));

  EXPECT_TRUE(ChangeList::HasChangeListKind(*change_list_json));
  EXPECT_FALSE(ChangeList::HasChangeListKind(*file_list_json));

  EXPECT_FALSE(FileList::HasFileListKind(*change_list_json));
  EXPECT_TRUE(FileList::HasFileListKind(*file_list_json));
}

TEST(DriveAPIParserTest, StartPageToken) {
  std::unique_ptr<base::Value> document(
      test_util::LoadJSONFile("drive/start_page_token.json"));

  ASSERT_TRUE(document.get());
  ASSERT_EQ(base::Value::Type::DICT, document->type());
  std::unique_ptr<StartPageToken> resource =
      StartPageToken::CreateFrom(*document);

  EXPECT_EQ("15734", resource->start_page_token());
}

}  // namespace google_apis
