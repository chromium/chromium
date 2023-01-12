// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/registry_hash_store_contents_win.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr wchar_t kRegistryPath[] = L"Foo\\TestStore";
constexpr wchar_t kStoreKey[] = L"test_store_key";

// Hex-encoded MACs are 64 characters long.
constexpr char kTestStringA[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kTestStringB[] =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

constexpr char kAtomicPrefPath[] = "path1";
constexpr char kSplitPrefPath[] = "extension";

class RegistryHashStoreContentsWinTest : public testing::Test {
 public:
  RegistryHashStoreContentsWinTest(const RegistryHashStoreContentsWinTest&) =
      delete;
  RegistryHashStoreContentsWinTest& operator=(
      const RegistryHashStoreContentsWinTest&) = delete;

 protected:
  RegistryHashStoreContentsWinTest() {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    contents = std::make_unique<RegistryHashStoreContentsWin>(
        kRegistryPath, kStoreKey, nullptr);
  }

  std::unique_ptr<HashStoreContents> contents;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

}  // namespace

TEST_F(RegistryHashStoreContentsWinTest, TestSetAndGetMac) {
  std::string stored_mac;
  EXPECT_FALSE(contents->GetMac(kAtomicPrefPath, &stored_mac));

  contents->SetMac(kAtomicPrefPath, kTestStringA);

  EXPECT_TRUE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);
}

TEST_F(RegistryHashStoreContentsWinTest, TestSetAndGetSplitMacs) {
  std::map<std::string, std::string> split_macs;
  EXPECT_FALSE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));

  contents->SetSplitMac(kSplitPrefPath, "a", kTestStringA);
  contents->SetSplitMac(kSplitPrefPath, "b", kTestStringB);

  EXPECT_TRUE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(2U, split_macs.size());
  EXPECT_EQ(kTestStringA, split_macs.at("a"));
  EXPECT_EQ(kTestStringB, split_macs.at("b"));
}

TEST_F(RegistryHashStoreContentsWinTest, TestRemoveAtomicMac) {
  contents->SetMac(kAtomicPrefPath, kTestStringA);

  std::string stored_mac;
  EXPECT_TRUE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);

  contents->RemoveEntry(kAtomicPrefPath);

  EXPECT_FALSE(contents->GetMac(kAtomicPrefPath, &stored_mac));
}

TEST_F(RegistryHashStoreContentsWinTest, TestRemoveSplitMacs) {
  contents->SetSplitMac(kSplitPrefPath, "a", kTestStringA);
  contents->SetSplitMac(kSplitPrefPath, "b", kTestStringB);

  std::map<std::string, std::string> split_macs;
  EXPECT_TRUE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(2U, split_macs.size());

  contents->RemoveEntry(kSplitPrefPath);

  split_macs.clear();
  EXPECT_FALSE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(0U, split_macs.size());
}

TEST_F(RegistryHashStoreContentsWinTest, TestReset) {
  contents->SetMac(kAtomicPrefPath, kTestStringA);
  contents->SetSplitMac(kSplitPrefPath, "a", kTestStringA);

  std::string stored_mac;
  EXPECT_TRUE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);

  std::map<std::string, std::string> split_macs;
  EXPECT_TRUE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(1U, split_macs.size());

  contents->Reset();

  stored_mac.clear();
  EXPECT_FALSE(contents->GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_TRUE(stored_mac.empty());

  split_macs.clear();
  EXPECT_FALSE(contents->GetSplitMacs(kSplitPrefPath, &split_macs));
  EXPECT_EQ(0U, split_macs.size());
}

TEST(RegistryHashStoreContentsWinScopedTest, TestScopedDirsCleared) {
  std::string stored_mac;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::wstring registry_path =
      temp_dir.GetPath().DirName().BaseName().value();

  RegistryHashStoreContentsWin verifying_contents(registry_path, kStoreKey,
                                                  nullptr);

  scoped_refptr<TempScopedDirRegistryCleaner> temp_scoped_dir_cleaner =
      base::MakeRefCounted<TempScopedDirRegistryCleaner>();
  std::unique_ptr<RegistryHashStoreContentsWin> contentsA =
      std::make_unique<RegistryHashStoreContentsWin>(registry_path, kStoreKey,
                                                     temp_scoped_dir_cleaner);
  std::unique_ptr<RegistryHashStoreContentsWin> contentsB =
      std::make_unique<RegistryHashStoreContentsWin>(registry_path, kStoreKey,
                                                     temp_scoped_dir_cleaner);

  contentsA->SetMac(kAtomicPrefPath, kTestStringA);
  contentsB->SetMac(kAtomicPrefPath, kTestStringB);

  temp_scoped_dir_cleaner = nullptr;
  EXPECT_TRUE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringB, stored_mac);

  contentsB.reset();
  EXPECT_TRUE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringB, stored_mac);

  contentsA.reset();
  EXPECT_FALSE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
}

void OffThreadTempScopedDirDestructor(
    std::wstring registry_path,
    std::unique_ptr<HashStoreContents> contents) {
  std::string stored_mac;

  RegistryHashStoreContentsWin verifying_contents(registry_path, kStoreKey,
                                                  nullptr);

  contents->SetMac(kAtomicPrefPath, kTestStringB);
  EXPECT_TRUE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringB, stored_mac);

  contents.reset();
  EXPECT_FALSE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
}

TEST(RegistryHashStoreContentsWinScopedTest, TestScopedDirsClearedMultiThread) {
  std::string stored_mac;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::wstring registry_path =
      temp_dir.GetPath().DirName().BaseName().value();

  RegistryHashStoreContentsWin verifying_contents(registry_path, kStoreKey,
                                                  nullptr);

  base::Thread test_thread("scoped_dir_cleaner_test_thread");
  test_thread.StartAndWaitForTesting();

  scoped_refptr<TempScopedDirRegistryCleaner> temp_scoped_dir_cleaner =
      base::MakeRefCounted<TempScopedDirRegistryCleaner>();
  std::unique_ptr<RegistryHashStoreContentsWin> contents =
      std::make_unique<RegistryHashStoreContentsWin>(
          registry_path, kStoreKey, std::move(temp_scoped_dir_cleaner));
  base::OnceClosure other_thread_closure = base::BindOnce(
      &OffThreadTempScopedDirDestructor, registry_path, contents->MakeCopy());

  contents->SetMac(kAtomicPrefPath, kTestStringA);
  contents.reset();

  EXPECT_TRUE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
  EXPECT_EQ(kTestStringA, stored_mac);

  test_thread.task_runner()->PostTask(FROM_HERE,
                                      std::move(other_thread_closure));
  test_thread.FlushForTesting();

  EXPECT_FALSE(verifying_contents.GetMac(kAtomicPrefPath, &stored_mac));
}
