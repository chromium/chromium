// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/nss_util.h"

#include <prtime.h>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/scoped_path_override.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

TEST(NSSUtilTest, PRTimeConversion) {
  EXPECT_EQ(base::Time::UnixEpoch(), PRTimeToBaseTime(0));
  EXPECT_EQ(0, BaseTimeToPRTime(base::Time::UnixEpoch()));

  static constexpr PRExplodedTime kPrxtime = {
      .tm_usec = 342000,
      .tm_sec = 19,
      .tm_min = 52,
      .tm_hour = 2,
      .tm_mday = 10,
      .tm_month = 11,  // 0-based
      .tm_year = 2011,
      .tm_params = {.tp_gmt_offset = 0, .tp_dst_offset = 0}};
  PRTime pr_time = PR_ImplodeTime(&kPrxtime);

  static constexpr base::Time::Exploded kExploded = {.year = 2011,
                                                     .month = 12,  // 1-based
                                                     .day_of_month = 10,
                                                     .hour = 2,
                                                     .minute = 52,
                                                     .second = 19,
                                                     .millisecond = 342};
  base::Time base_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kExploded, &base_time));

  EXPECT_EQ(base_time, PRTimeToBaseTime(pr_time));
  EXPECT_EQ(pr_time, BaseTimeToPRTime(base_time));
}

#if !BUILDFLAG(IS_CHROMEOS)
class NssDirectoryTest : public testing::Test {
 protected:
  void SetUp() override {
    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
    SetHome(scoped_temp_dir_.GetPath());
    SetXdgDataHome(
        scoped_temp_dir_.GetPath().Append(".FROM_ENV-local/share").value());
  }

  void SetHome(const base::FilePath& path) {
    home_path_override_.reset();
    home_path_override_.emplace(base::DIR_HOME, path, true);
  }

  void SetXdgDataHome(const std::string& value) {
    xdg_data_home_override_.reset();
    xdg_data_home_override_ =
        base::ScopedEnvironmentVariableOverride("XDG_DATA_HOME", value);
  }

  base::ScopedTempDir scoped_temp_dir_;
  std::optional<base::ScopedEnvironmentVariableOverride>
      xdg_data_home_override_;
  std::optional<base::ScopedPathOverride> home_path_override_;
};

TEST_F(NssDirectoryTest, UseClassicNssFolderIfExists) {
  base::FilePath nssdb_path = scoped_temp_dir_.GetPath().Append(".pki/nssdb");

  ASSERT_TRUE(base::CreateDirectory(nssdb_path));
  ASSERT_EQ(nssdb_path, GetDefaultNSSConfigDirectory());
}

TEST_F(NssDirectoryTest, UseXdgDataNssFolder) {
  base::FilePath nssdb_path =
      scoped_temp_dir_.GetPath().Append(".FROM_ENV-local/share/pki/nssdb");
  ASSERT_EQ(nssdb_path, GetDefaultNSSConfigDirectory());
}

TEST_F(NssDirectoryTest, UseXdgDataNssFolderFallback) {
  base::FilePath nssdb_path =
      scoped_temp_dir_.GetPath().Append(".local/share/pki/nssdb");

  SetXdgDataHome("");

  ASSERT_EQ(nssdb_path, GetDefaultNSSConfigDirectory());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace crypto
