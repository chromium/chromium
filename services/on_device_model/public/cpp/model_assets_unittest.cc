// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"

namespace on_device_model {
namespace {

constexpr std::string_view kCacheData = "cache data";

std::string ReadFile(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return contents;
}

std::string DawnVersion() {
  return std::string(reinterpret_cast<const char*>(dawnProcGetVersion()), 20);
}

class ModelAssetsTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    paths_.weights = Path("weights.bin");
    ASSERT_TRUE(base::WriteFile(paths_.weights, "weights"));
  }

  base::FilePath Path(std::string_view name) const {
    return temp_dir_.GetPath().AppendASCII(name);
  }

  // Writes a cache file at `path`, and its fingerprint if provided.
  void WriteCache(const base::FilePath& path,
                  std::optional<std::string> fingerprint) {
    ASSERT_TRUE(base::WriteFile(path, kCacheData));
    if (fingerprint) {
      ASSERT_TRUE(base::WriteFile(GetCacheFingerprintPath(path), *fingerprint));
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  ModelAssetPaths paths_;
};

TEST_F(ModelAssetsTest, KeepsWeightCachesWithMatchingFingerprint) {
  paths_.cache = Path("cache.bin");
  paths_.encoder_cache = Path("encoder_cache.bin");
  paths_.adapter_cache = Path("adapter_cache.bin");
  paths_.mtp_drafter_cache = Path("mtp_drafter_cache.bin");
  paths_.weight_cache_fingerprint = "gpu";
  for (const auto& path : {paths_.cache, paths_.encoder_cache,
                           paths_.adapter_cache, paths_.mtp_drafter_cache}) {
    WriteCache(path, "gpu");
  }

  ModelAssets assets = LoadModelAssets(paths_);

  for (base::File* file : {&assets.cache, &assets.encoder_cache,
                           &assets.adapter_cache, &assets.mtp_drafter_cache}) {
    ASSERT_TRUE(file->IsValid());
    EXPECT_EQ(file->GetLength(), static_cast<int64_t>(kCacheData.size()));
  }
}

TEST_F(ModelAssetsTest, TruncatesWeightCachesWithDifferentFingerprint) {
  paths_.cache = Path("cache.bin");
  paths_.encoder_cache = Path("encoder_cache.bin");
  paths_.adapter_cache = Path("adapter_cache.bin");
  paths_.mtp_drafter_cache = Path("mtp_drafter_cache.bin");
  paths_.weight_cache_fingerprint = "gpu";
  for (const auto& path : {paths_.cache, paths_.encoder_cache,
                           paths_.adapter_cache, paths_.mtp_drafter_cache}) {
    WriteCache(path, "cpu");
  }

  ModelAssets assets = LoadModelAssets(paths_);

  for (base::File* file : {&assets.cache, &assets.encoder_cache,
                           &assets.adapter_cache, &assets.mtp_drafter_cache}) {
    ASSERT_TRUE(file->IsValid());
    EXPECT_EQ(file->GetLength(), 0);
  }
  for (const auto& path : {paths_.cache, paths_.encoder_cache,
                           paths_.adapter_cache, paths_.mtp_drafter_cache}) {
    EXPECT_EQ(ReadFile(GetCacheFingerprintPath(path)), "gpu");
  }
}

TEST_F(ModelAssetsTest, TruncatesWeightCacheWithoutFingerprint) {
  paths_.cache = Path("cache.bin");
  paths_.weight_cache_fingerprint = "cpu";
  WriteCache(paths_.cache, std::nullopt);

  ModelAssets assets = LoadModelAssets(paths_);

  ASSERT_TRUE(assets.cache.IsValid());
  EXPECT_EQ(assets.cache.GetLength(), 0);
  EXPECT_EQ(ReadFile(GetCacheFingerprintPath(paths_.cache)), "cpu");
}

// Fuchsia opens caches with FLAG_OPEN, so missing caches aren't created.
#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(ModelAssetsTest, CreatesMissingWeightCacheWithFingerprint) {
  paths_.cache = Path("cache.bin");
  paths_.weight_cache_fingerprint = "gpu";

  ModelAssets assets = LoadModelAssets(paths_);

  ASSERT_TRUE(assets.cache.IsValid());
  EXPECT_EQ(assets.cache.GetLength(), 0);
  EXPECT_TRUE(base::PathExists(paths_.cache));
  EXPECT_EQ(ReadFile(GetCacheFingerprintPath(paths_.cache)), "gpu");
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(ModelAssetsTest, SkipsWeightCacheIfFingerprintCannotBeWritten) {
  paths_.cache = Path("cache.bin");
  paths_.weight_cache_fingerprint = "gpu";
  WriteCache(paths_.cache, std::nullopt);
  // A directory at the fingerprint path makes writing the fingerprint fail.
  ASSERT_TRUE(base::CreateDirectory(GetCacheFingerprintPath(paths_.cache)));

  ModelAssets assets = LoadModelAssets(paths_);

  EXPECT_FALSE(assets.cache.IsValid());
  EXPECT_FALSE(base::PathExists(GetCacheFingerprintPath(paths_.cache)));
}

TEST_F(ModelAssetsTest, KeepsWeightCacheWithoutExpectedFingerprint) {
  paths_.cache = Path("cache.bin");
  WriteCache(paths_.cache, std::nullopt);

  ModelAssets assets = LoadModelAssets(paths_);

  ASSERT_TRUE(assets.cache.IsValid());
  EXPECT_EQ(assets.cache.GetLength(), static_cast<int64_t>(kCacheData.size()));
  EXPECT_FALSE(base::PathExists(GetCacheFingerprintPath(paths_.cache)));
}

TEST_F(ModelAssetsTest, ProgramCacheIgnoresWeightCacheFingerprint) {
  paths_.program_cache = Path("program_cache.bin");
  paths_.weight_cache_fingerprint = "gpu";
  WriteCache(paths_.program_cache, DawnVersion());

  ModelAssets assets = LoadModelAssets(paths_);

  ASSERT_TRUE(assets.program_cache.IsValid());
  EXPECT_EQ(assets.program_cache.GetLength(),
            static_cast<int64_t>(kCacheData.size()));
}

TEST_F(ModelAssetsTest, TruncatesProgramCacheWithDifferentDawnVersion) {
  paths_.program_cache = Path("program_cache.bin");
  WriteCache(paths_.program_cache, "old dawn version");

  ModelAssets assets = LoadModelAssets(paths_);

  ASSERT_TRUE(assets.program_cache.IsValid());
  EXPECT_EQ(assets.program_cache.GetLength(), 0);
  EXPECT_EQ(ReadFile(GetCacheFingerprintPath(paths_.program_cache)),
            DawnVersion());
}

TEST_F(ModelAssetsTest, MigratesLegacyDawnVersionFile) {
  paths_.program_cache = Path("program_cache.bin");
  WriteCache(paths_.program_cache, std::nullopt);
  base::FilePath legacy_path =
      paths_.program_cache.AddExtension(FILE_PATH_LITERAL(".dawn_version"));
  ASSERT_TRUE(base::WriteFile(legacy_path, DawnVersion()));

  ModelAssets assets = LoadModelAssets(paths_);

  ASSERT_TRUE(assets.program_cache.IsValid());
  EXPECT_EQ(assets.program_cache.GetLength(),
            static_cast<int64_t>(kCacheData.size()));
  EXPECT_FALSE(base::PathExists(legacy_path));
  EXPECT_EQ(ReadFile(GetCacheFingerprintPath(paths_.program_cache)),
            DawnVersion());
}

}  // namespace
}  // namespace on_device_model
