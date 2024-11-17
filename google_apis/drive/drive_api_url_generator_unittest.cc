// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/drive/drive_api_url_generator.h"

#include <stddef.h>
#include <stdint.h>

#include "google_apis/common/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace google_apis {
namespace {
// The URLs used for production may be different for Chromium OS and Chrome
// OS, so use testing base urls.
const char kBaseUrlForTesting[] = "https://www.example.com";
const char kBaseThumbnailUrlForTesting[] = "https://thumbnail.example.com";
}  // namespace

class DriveApiUrlGeneratorTest : public testing::Test {
 public:
  DriveApiUrlGeneratorTest()
      : url_generator_(GURL(kBaseUrlForTesting),
                       GURL(kBaseThumbnailUrlForTesting)) {
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  }

 protected:
  DriveApiUrlGenerator url_generator_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

// Make sure the hard-coded urls are returned.
TEST_F(DriveApiUrlGeneratorTest, GetAboutGetUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2/about",
            url_generator_.GetAboutGetUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesGetUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg"
      "?supportsTeamDrives=true",
      url_generator_.GetFilesGetUrl("0ADK06pfg", GURL()).spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074"
      "?supportsTeamDrives=true",
      url_generator_.GetFilesGetUrl("0Bz0bd074", GURL()).spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afile_id"
      "?supportsTeamDrives=true",
      url_generator_.GetFilesGetUrl("file:file_id", GURL()).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesInsertUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2/files?supportsTeamDrives=true",
            url_generator_.GetFilesInsertUrl("").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/"
      "files?supportsTeamDrives=true&visibility=DEFAULT",
      url_generator_.GetFilesInsertUrl("DEFAULT").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/"
      "files?supportsTeamDrives=true&visibility=PRIVATE",
      url_generator_.GetFilesInsertUrl("PRIVATE").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilePatchUrl) {
  struct TestPattern {
    bool set_modified_date;
    bool update_viewed_date;
    const std::string expected_query;
  };
  const TestPattern kTestPatterns[] = {
      {false, true, ""},
      {true, true, "&setModifiedDate=true"},
      {false, false, "&updateViewedDate=false"},
      {true, false, "&setModifiedDate=true&updateViewedDate=false"},
  };

  for (size_t i = 0; i < std::size(kTestPatterns); ++i) {
    EXPECT_EQ(
        "https://www.example.com/drive/v2/files/0ADK06pfg"
        "?supportsTeamDrives=true" +
            kTestPatterns[i].expected_query,
        url_generator_
            .GetFilesPatchUrl("0ADK06pfg", kTestPatterns[i].set_modified_date,
                              kTestPatterns[i].update_viewed_date)
            .spec());

    EXPECT_EQ(
        "https://www.example.com/drive/v2/files/0Bz0bd074"
        "?supportsTeamDrives=true" +
            kTestPatterns[i].expected_query,
        url_generator_
            .GetFilesPatchUrl("0Bz0bd074", kTestPatterns[i].set_modified_date,
                              kTestPatterns[i].update_viewed_date)
            .spec());

    EXPECT_EQ(
        "https://www.example.com/drive/v2/files/file%3Afile_id"
        "?supportsTeamDrives=true" +
            kTestPatterns[i].expected_query,
        url_generator_
            .GetFilesPatchUrl("file:file_id",
                              kTestPatterns[i].set_modified_date,
                              kTestPatterns[i].update_viewed_date)
            .spec());
  }
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesCopyUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg/copy"
      "?supportsTeamDrives=true",
      url_generator_.GetFilesCopyUrl("0ADK06pfg", "").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074/copy"
      "?supportsTeamDrives=true",
      url_generator_.GetFilesCopyUrl("0Bz0bd074", "").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afile_id/copy"
      "?supportsTeamDrives=true",
      url_generator_.GetFilesCopyUrl("file:file_id", "").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074/copy"
      "?supportsTeamDrives=true&visibility=DEFAULT",
      url_generator_.GetFilesCopyUrl("0Bz0bd074", "DEFAULT").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afile_id/copy"
      "?supportsTeamDrives=true&visibility=PRIVATE",
      url_generator_.GetFilesCopyUrl("file:file_id", "PRIVATE").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesListUrl) {
  struct TestPattern {
    int max_results;
    const std::string page_token;
    const std::string q;
    const std::string expected_query;
  };
  const TestPattern kTestPatterns[] = {
      {100, "", "", ""},
      {150, "", "", "maxResults=150"},
      {10, "", "", "maxResults=10"},
      {100, "token", "", "pageToken=token"},
      {150, "token", "", "maxResults=150&pageToken=token"},
      {10, "token", "", "maxResults=10&pageToken=token"},
      {100, "", "query", "q=query"},
      {150, "", "query", "maxResults=150&q=query"},
      {10, "", "query", "maxResults=10&q=query"},
      {100, "token", "query", "pageToken=token&q=query"},
      {150, "token", "query", "maxResults=150&pageToken=token&q=query"},
      {10, "token", "query", "maxResults=10&pageToken=token&q=query"},
  };
  const std::string kV2FilesUrlPrefixWithTeamDrives =
      "https://www.example.com/drive/v2/files?supportsTeamDrives=true&"
      "includeTeamDriveItems=true&corpora=default%2CallTeamDrives";

  for (size_t i = 0; i < std::size(kTestPatterns); ++i) {
    EXPECT_EQ(kV2FilesUrlPrefixWithTeamDrives +
                  (kTestPatterns[i].expected_query.empty() ? "" : "&") +
                  kTestPatterns[i].expected_query,
              url_generator_
                  .GetFilesListUrl(kTestPatterns[i].max_results,
                                   kTestPatterns[i].page_token,
                                   FilesListCorpora::ALL_TEAM_DRIVES,
                                   std::string(), kTestPatterns[i].q)
                  .spec());
  }

  EXPECT_EQ(
      "https://www.example.com/drive/v2/files?supportsTeamDrives=true&"
      "includeTeamDriveItems=true&corpora=teamDrive&"
      "teamDriveId=TheTeamDriveId&q=query",
      url_generator_
          .GetFilesListUrl(100, std::string() /* page_token */,
                           FilesListCorpora::TEAM_DRIVE, "TheTeamDriveId",
                           "query")
          .spec());

  // includeTeamDriveItems should be true for default corpora, so that a file
  // that is shared individually is listed for users who are not member of the
  // Team Drive which owns the file.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files?supportsTeamDrives=true&"
      "includeTeamDriveItems=true&corpora=default",
      url_generator_
          .GetFilesListUrl(100, std::string() /* page_token */,
                           FilesListCorpora::DEFAULT, std::string(),
                           std::string())
          .spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesDeleteUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg?"
      "supportsTeamDrives=true",
      url_generator_.GetFilesDeleteUrl("0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074?"
      "supportsTeamDrives=true",
      url_generator_.GetFilesDeleteUrl("0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afile_id?"
      "supportsTeamDrives=true",
      url_generator_.GetFilesDeleteUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesTrashUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg/trash?"
      "supportsTeamDrives=true",
      url_generator_.GetFilesTrashUrl("0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074/trash?"
      "supportsTeamDrives=true",
      url_generator_.GetFilesTrashUrl("0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afile_id/trash?"
      "supportsTeamDrives=true",
      url_generator_.GetFilesTrashUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChangesListUrl) {
  struct TestPattern {
    bool include_deleted;
    int max_results;
    const std::string page_token;
    int64_t start_change_id;
    const std::string expected_query;
  };
  const TestPattern kTestPatterns[] = {
      {true, 100, "", 0, ""},
      {false, 100, "", 0, "includeDeleted=false"},
      {true, 150, "", 0, "maxResults=150"},
      {false, 150, "", 0, "includeDeleted=false&maxResults=150"},
      {true, 10, "", 0, "maxResults=10"},
      {false, 10, "", 0, "includeDeleted=false&maxResults=10"},

      {true, 100, "token", 0, "pageToken=token"},
      {false, 100, "token", 0, "includeDeleted=false&pageToken=token"},
      {true, 150, "token", 0, "maxResults=150&pageToken=token"},
      {false, 150, "token", 0,
       "includeDeleted=false&maxResults=150&pageToken=token"},
      {true, 10, "token", 0, "maxResults=10&pageToken=token"},
      {false, 10, "token", 0,
       "includeDeleted=false&maxResults=10&pageToken=token"},

      {true, 100, "", 12345, "startChangeId=12345"},
      {false, 100, "", 12345, "includeDeleted=false&startChangeId=12345"},
      {true, 150, "", 12345, "maxResults=150&startChangeId=12345"},
      {false, 150, "", 12345,
       "includeDeleted=false&maxResults=150&startChangeId=12345"},
      {true, 10, "", 12345, "maxResults=10&startChangeId=12345"},
      {false, 10, "", 12345,
       "includeDeleted=false&maxResults=10&startChangeId=12345"},

      {true, 100, "token", 12345, "pageToken=token&startChangeId=12345"},
      {false, 100, "token", 12345,
       "includeDeleted=false&pageToken=token&startChangeId=12345"},
      {true, 150, "token", 12345,
       "maxResults=150&pageToken=token&startChangeId=12345"},
      {false, 150, "token", 12345,
       "includeDeleted=false&maxResults=150&pageToken=token"
       "&startChangeId=12345"},
      {true, 10, "token", 12345,
       "maxResults=10&pageToken=token&startChangeId=12345"},
      {false, 10, "token", 12345,
       "includeDeleted=false&maxResults=10&pageToken=token"
       "&startChangeId=12345"},
  };

  const std::string kV2ChangesUrlPrefixWithTeamDrives =
      "https://www.example.com/drive/v2/changes?"
      "supportsTeamDrives=true&includeTeamDriveItems=true";
  for (size_t i = 0; i < std::size(kTestPatterns); ++i) {
    EXPECT_EQ(kV2ChangesUrlPrefixWithTeamDrives +
                  (kTestPatterns[i].expected_query.empty() ? "" : "&") +
                  kTestPatterns[i].expected_query,
              url_generator_
                  .GetChangesListUrl(
                      kTestPatterns[i].include_deleted,
                      kTestPatterns[i].max_results, kTestPatterns[i].page_token,
                      kTestPatterns[i].start_change_id, "" /* team_drive_id */)
                  .spec());
  }

  EXPECT_EQ(kV2ChangesUrlPrefixWithTeamDrives + "&teamDriveId=TEAM_DRIVE_ID",
            url_generator_.GetChangesListUrl(true, 100, "", 0, "TEAM_DRIVE_ID")
                .spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChildrenInsertUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg/children?"
      "supportsTeamDrives=true",
      url_generator_.GetChildrenInsertUrl("0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074/children?"
      "supportsTeamDrives=true",
      url_generator_.GetChildrenInsertUrl("0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afolder_id/children?"
      "supportsTeamDrives=true",
      url_generator_.GetChildrenInsertUrl("file:folder_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChildrenDeleteUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg/children/0Bz0bd074",
      url_generator_.GetChildrenDeleteUrl("0Bz0bd074", "0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074/children/0ADK06pfg",
      url_generator_.GetChildrenDeleteUrl("0ADK06pfg", "0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afolder_id/children"
      "/file%3Achild_id",
      url_generator_.GetChildrenDeleteUrl("file:child_id", "file:folder_id")
          .spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetInitiateUploadNewFileUrl) {
  const bool kSetModifiedDate = true;

  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files?uploadType=resumable"
      "&supportsTeamDrives=true",
      url_generator_.GetInitiateUploadNewFileUrl(!kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files?uploadType=resumable"
      "&supportsTeamDrives=true&setModifiedDate=true",
      url_generator_.GetInitiateUploadNewFileUrl(kSetModifiedDate).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetInitiateUploadExistingFileUrl) {
  const bool kSetModifiedDate = true;

  // |resource_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0ADK06pfg"
      "?uploadType=resumable&supportsTeamDrives=true",
      url_generator_
          .GetInitiateUploadExistingFileUrl("0ADK06pfg", !kSetModifiedDate)
          .spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0Bz0bd074"
      "?uploadType=resumable&supportsTeamDrives=true",
      url_generator_
          .GetInitiateUploadExistingFileUrl("0Bz0bd074", !kSetModifiedDate)
          .spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=resumable&supportsTeamDrives=true",
      url_generator_
          .GetInitiateUploadExistingFileUrl("file:file_id", !kSetModifiedDate)
          .spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=resumable&supportsTeamDrives=true&setModifiedDate=true",
      url_generator_
          .GetInitiateUploadExistingFileUrl("file:file_id", kSetModifiedDate)
          .spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetMultipartUploadNewFileUrl) {
  EXPECT_EQ(url_generator_
                .GetMultipartUploadNewFileUrl(/*set_modified_date=*/false,
                                              /*convert=*/false)
                .spec(),
            "https://www.example.com/upload/drive/v2/files?uploadType=multipart"
            "&supportsTeamDrives=true");
  EXPECT_EQ(
      url_generator_
          .GetMultipartUploadNewFileUrl(/*set_modified_date=*/true,
                                        /*convert=*/false)
          .spec(),
      "https://www.example.com/upload/drive/v2/files?uploadType=multipart&"
      "supportsTeamDrives=true&setModifiedDate=true");

  EXPECT_EQ(
      url_generator_
          .GetMultipartUploadNewFileUrl(/*set_modified_date=*/false,
                                        /*convert=*/true)
          .spec(),
      "https://www.example.com/upload/drive/v2/files?uploadType=multipart&"
      "supportsTeamDrives=true&convert=true");
  EXPECT_EQ(
      url_generator_
          .GetMultipartUploadNewFileUrl(/*set_modified_date=*/true,
                                        /*convert=*/true)
          .spec(),
      "https://www.example.com/upload/drive/v2/files?uploadType=multipart&"
      "supportsTeamDrives=true&setModifiedDate=true&convert=true");
}

TEST_F(DriveApiUrlGeneratorTest, GetMultipartUploadExistingFileUrl) {
  const bool kSetModifiedDate = true;

  // |resource_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0ADK06pfg"
      "?uploadType=multipart&supportsTeamDrives=true",
      url_generator_
          .GetMultipartUploadExistingFileUrl("0ADK06pfg", !kSetModifiedDate)
          .spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0Bz0bd074"
      "?uploadType=multipart&supportsTeamDrives=true",
      url_generator_
          .GetMultipartUploadExistingFileUrl("0Bz0bd074", !kSetModifiedDate)
          .spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=multipart&supportsTeamDrives=true",
      url_generator_
          .GetMultipartUploadExistingFileUrl("file:file_id", !kSetModifiedDate)
          .spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=multipart&supportsTeamDrives=true&setModifiedDate=true",
      url_generator_
          .GetMultipartUploadExistingFileUrl("file:file_id", kSetModifiedDate)
          .spec());
}

TEST_F(DriveApiUrlGeneratorTest, GenerateDownloadFileUrl) {
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/resourceId?alt=media"
      "&supportsTeamDrives=true",
      url_generator_.GenerateDownloadFileUrl("resourceId").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3AresourceId?alt=media"
      "&supportsTeamDrives=true",
      url_generator_.GenerateDownloadFileUrl("file:resourceId").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GeneratePermissionsInsertUrl) {
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg/permissions"
      "?supportsTeamDrives=true",
      url_generator_.GetPermissionsInsertUrl("0ADK06pfg").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GenerateThumbnailUrl) {
  EXPECT_EQ(
      "https://thumbnail.example.com/d/0ADK06pfg=w500-h480",
      url_generator_.GetThumbnailUrl("0ADK06pfg", 500, 480, false).spec());

  EXPECT_EQ("https://thumbnail.example.com/d/0ADK06pfg=w360-h380-c",
            url_generator_.GetThumbnailUrl("0ADK06pfg", 360, 380, true).spec());
}

TEST_F(DriveApiUrlGeneratorTest, BatchUploadUrl) {
  EXPECT_EQ("https://www.example.com/upload/drive?supportsTeamDrives=true",
            url_generator_.GetBatchUploadUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GenerateTeamDriveListUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2/teamdrives",
            url_generator_.GetTeamDriveListUrl(10, "").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/teamdrives?maxResults=100",
            url_generator_.GetTeamDriveListUrl(100, "").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/"
      "teamdrives?maxResults=100&pageToken=theToken",
      url_generator_.GetTeamDriveListUrl(100, "theToken").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GeneraeStartPageTokenUrl) {
  EXPECT_EQ(
      "https://www.example.com/drive/v2/changes/"
      "startPageToken?supportsTeamDrives=true",
      url_generator_.GetStartPageTokenUrl("").spec());

  EXPECT_EQ(
      "https://www.example.com/drive/v2/changes/"
      "startPageToken?supportsTeamDrives=true&teamDriveId=team_drive_id",
      url_generator_.GetStartPageTokenUrl("team_drive_id").spec());
}

}  // namespace google_apis
