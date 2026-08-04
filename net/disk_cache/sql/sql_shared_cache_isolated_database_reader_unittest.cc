// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_isolated_database_reader.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_view_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SqlSharedCacheIsolatedDatabaseReaderTest
    : public testing::TestWithParam<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "WalEnabled" : "WalDisabled";
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    if (GetParam()) {
      feature_list_.InitWithFeaturesAndParameters(
          {{net::features::kRendererAccessibleHttpCache,
            {{net::features::kRendererAccessibleHttpCacheWalMode.name,
              "true"}}}},
          {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          {{net::features::kRendererAccessibleHttpCache,
            {{net::features::kRendererAccessibleHttpCacheWalMode.name,
              "false"}}}},
          {});
    }
  }

 protected:
  sqlite_vfs::PendingFileSet PopulateDatabase(std::string_view key_str,
                                              std::string_view header_data,
                                              std::string_view body_data,
                                              bool set_ready = true) {
    SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(),
                                      SqlSharedCacheDbId(1));
    EXPECT_TRUE(db.Init().has_value());

    auto headers =
        base::MakeRefCounted<net::StringIOBuffer>(std::string(header_data));
    CacheEntryKey key{std::string(key_str)};

    if (!set_ready) {
      auto insert_result = db.Insert(key, headers, body_data.size(), nullptr);
      EXPECT_TRUE(insert_result.has_value());
      auto row_id = insert_result.value();

      auto body =
          base::MakeRefCounted<net::StringIOBuffer>(std::string(body_data));
      EXPECT_TRUE(
          db.WriteBody(key, row_id, 0, body, /*set_ready=*/false).has_value());
    } else if (body_data.empty()) {
      auto insert_result = db.Insert(key, headers, 0, nullptr);
      EXPECT_TRUE(insert_result.has_value());
    } else {
      auto body =
          base::MakeRefCounted<net::StringIOBuffer>(std::string(body_data));
      auto insert_result = db.Insert(key, headers, body_data.size(), body);
      EXPECT_TRUE(insert_result.has_value());
    }

    auto pending_file_set = db.GetSharedReadOnlyConnection();
    EXPECT_TRUE(pending_file_set.has_value());
    return std::move(pending_file_set.value());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SqlSharedCacheIsolatedDatabaseReaderTest,
    testing::Bool(),
    &SqlSharedCacheIsolatedDatabaseReaderTest::DescribeParams);

TEST_P(SqlSharedCacheIsolatedDatabaseReaderTest, ReadReady) {
  const std::string key_str = "0/0/https://example.com/ready";
  const std::string header_data = "This is header data.";
  const std::string body_data = "This is a ready body.";

  SqlSharedCacheIsolatedDatabaseReader reader(
      PopulateDatabase(key_str, header_data, body_data));

  auto response = reader.ReadResponse(CacheEntryKey(key_str).resource_url());
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(base::as_string_view(response->TakeHeaders()), header_data);
  EXPECT_EQ(response->GetBodySize(), static_cast<int>(body_data.size()));

  std::vector<uint8_t> read_body(body_data.size());
  EXPECT_TRUE(response->ReadBody(read_body));
  EXPECT_EQ(base::as_string_view(read_body), body_data);
}

TEST_P(SqlSharedCacheIsolatedDatabaseReaderTest, ReadNotReady) {
  const std::string key_str = "0/0/https://example.com/not-ready";
  const std::string header_data = "Header for not ready.";
  const std::string body_data = "This is not ready.";

  SqlSharedCacheIsolatedDatabaseReader reader(
      PopulateDatabase(key_str, header_data, body_data, /*set_ready=*/false));

  auto response = reader.ReadResponse(CacheEntryKey(key_str).resource_url());
  EXPECT_FALSE(response.has_value());
}

TEST_P(SqlSharedCacheIsolatedDatabaseReaderTest, ReadEmptyHeader) {
  const std::string key_str = "0/0/https://example.com/empty-header";
  const std::string header_data = "";
  const std::string body_data = "Body with empty header.";

  SqlSharedCacheIsolatedDatabaseReader reader(
      PopulateDatabase(key_str, header_data, body_data));

  auto response = reader.ReadResponse(CacheEntryKey(key_str).resource_url());
  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(response->TakeHeaders().empty());
  EXPECT_EQ(response->GetBodySize(), static_cast<int>(body_data.size()));

  std::vector<uint8_t> read_body(body_data.size());
  EXPECT_TRUE(response->ReadBody(read_body));
  EXPECT_EQ(base::as_string_view(read_body), body_data);
}

TEST_P(SqlSharedCacheIsolatedDatabaseReaderTest, ReadEmptyBody) {
  const std::string key_str = "0/0/https://example.com/empty-body";
  const std::string header_data = "Header with empty body.";
  const std::string body_data = "";

  SqlSharedCacheIsolatedDatabaseReader reader(
      PopulateDatabase(key_str, header_data, body_data));

  auto response = reader.ReadResponse(CacheEntryKey(key_str).resource_url());
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(base::as_string_view(response->TakeHeaders()), header_data);
  EXPECT_EQ(response->GetBodySize(), 0);

  std::vector<uint8_t> read_body(0);
  EXPECT_TRUE(response->ReadBody(read_body));
}

}  // namespace disk_cache
