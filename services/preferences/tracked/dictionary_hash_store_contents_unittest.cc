// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/dictionary_hash_store_contents.h"

#include <map>
#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(DictionaryHashStoreContentsTest, CopyContents) {
  base::DictValue source;
  {
    DictionaryHashStoreContents contents(source);
    contents.SetAtomicPrefAuthenticator("atomic.pref", "atomic_mac");
    contents.SetSplitPrefAuthenticator("split.pref", "key", "split_mac");
    contents.SetSuperHmac("super_mac");
    contents.SetSuperEncryptedHash("super_encrypted_hash");
  }
  source.Set("unrelated", "value");

  base::DictValue dest;
  DictionaryHashStoreContents::CopyContents(source, dest);
  EXPECT_FALSE(dest.contains("unrelated"));

  DictionaryHashStoreContents contents(dest);
  std::string atomic_mac;
  EXPECT_TRUE(contents.GetAtomicPrefAuthenticator("atomic.pref", &atomic_mac));
  EXPECT_EQ("atomic_mac", atomic_mac);
  std::map<std::string, std::string> split_macs;
  EXPECT_TRUE(contents.GetSplitPrefAuthenticators("split.pref", &split_macs));
  EXPECT_EQ((std::map<std::string, std::string>{{"key", "split_mac"}}),
            split_macs);
  EXPECT_EQ("super_mac", contents.GetSuperHmac());
  EXPECT_EQ("super_encrypted_hash", contents.GetSuperEncryptedHash());

  // Copying from a dict with no hash store contents adds nothing.
  base::DictValue empty_dest;
  DictionaryHashStoreContents::CopyContents(base::DictValue(), empty_dest);
  EXPECT_TRUE(empty_dest.empty());
}

}  // namespace
