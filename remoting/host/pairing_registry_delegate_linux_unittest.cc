// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_linux.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::PairingRegistry;

class PairingRegistryDelegateLinuxTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a temporary directory in order to get a unique name and use a
    // subdirectory to ensure that PairingRegistryDelegateLinux::Save() creates
    // the parent directory if it doesn't exist.
    base::CreateNewTempDirectory("chromoting-test", &temp_dir_);
    temp_registry_ = temp_dir_.Append("paired-clients");
  }

  void TearDown() override { base::DeletePathRecursively(temp_dir_); }

 protected:
  base::FilePath temp_dir_;
  base::FilePath temp_registry_;
};

TEST_F(PairingRegistryDelegateLinuxTest, SaveAndLoad) {
  std::unique_ptr<PairingRegistryDelegateLinux> delegate(
      new PairingRegistryDelegateLinux());
  delegate->SetRegistryPathForTesting(temp_registry_);

  // Check that registry is initially empty.
  EXPECT_TRUE(delegate->LoadAll().empty());

  // Add a couple of pairings.
  PairingRegistry::Pairing pairing1(base::Time::Now(), "xxx", "xxx", "xxx");
  PairingRegistry::Pairing pairing2(base::Time::Now(), "yyy", "yyy", "yyy");
  EXPECT_TRUE(delegate->Save(pairing1));
  EXPECT_TRUE(delegate->Save(pairing2));

  // Verify that there are two pairings in the store now.
  EXPECT_EQ(delegate->LoadAll().size(), 2u);

  // Verify that they can be retrieved.
  EXPECT_EQ(delegate->Load(pairing1.client_id()), pairing1);
  EXPECT_EQ(delegate->Load(pairing2.client_id()), pairing2);

  // Delete the first pairing.
  EXPECT_TRUE(delegate->Delete(pairing1.client_id()));

  // Verify that there is only one pairing left.
  EXPECT_EQ(delegate->Load(pairing1.client_id()), PairingRegistry::Pairing());
  EXPECT_EQ(delegate->Load(pairing2.client_id()), pairing2);

  // Verify that the only value that left is |pairing2|.
  EXPECT_EQ(delegate->LoadAll().size(), 1u);
  base::Value::List pairings = delegate->LoadAll();
  ASSERT_TRUE(pairings[0].is_dict());
  EXPECT_EQ(PairingRegistry::Pairing::CreateFromValue(
                std::move(pairings[0]).TakeDict()),
            pairing2);

  // Delete the rest and verify.
  EXPECT_TRUE(delegate->DeleteAll());
  EXPECT_TRUE(delegate->LoadAll().empty());
}

// Verifies that the delegate is stateless by using two different instances.
TEST_F(PairingRegistryDelegateLinuxTest, Stateless) {
  std::unique_ptr<PairingRegistryDelegateLinux> save_delegate(
      new PairingRegistryDelegateLinux());
  std::unique_ptr<PairingRegistryDelegateLinux> load_delegate(
      new PairingRegistryDelegateLinux());
  save_delegate->SetRegistryPathForTesting(temp_registry_);
  load_delegate->SetRegistryPathForTesting(temp_registry_);

  PairingRegistry::Pairing pairing(base::Time::Now(), "xxx", "xxx", "xxx");
  EXPECT_TRUE(save_delegate->Save(pairing));
  EXPECT_EQ(load_delegate->Load(pairing.client_id()), pairing);
}

}  // namespace remoting
