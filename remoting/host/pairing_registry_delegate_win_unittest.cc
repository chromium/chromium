// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "remoting/host/pairing_registry_delegate_win.h"

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::PairingRegistry;

class PairingRegistryDelegateWinTest : public testing::Test {
 public:
  void SetUp() override {
    key_name_ = base::Uuid::GenerateRandomV4().AsLowercaseString();

    base::win::RegKey root;
    EXPECT_TRUE(root.Create(HKEY_CURRENT_USER,
                            base::UTF8ToWide(key_name_).c_str(),
                            KEY_READ | KEY_WRITE) == ERROR_SUCCESS);

    EXPECT_TRUE(privileged_.Create(root.Handle(), L"privileged",
                                   KEY_READ | KEY_WRITE) == ERROR_SUCCESS);
    EXPECT_TRUE(unprivileged_.Create(root.Handle(), L"unprivileged",
                                     KEY_READ | KEY_WRITE) == ERROR_SUCCESS);
  }

  void TearDown() override {
    privileged_.Close();
    unprivileged_.Close();
    EXPECT_TRUE(
        SHDeleteKey(HKEY_CURRENT_USER, base::UTF8ToWide(key_name_).c_str()) ==
        ERROR_SUCCESS);
  }

 protected:
  std::string key_name_;
  base::win::RegKey privileged_;
  base::win::RegKey unprivileged_;
};

TEST_F(PairingRegistryDelegateWinTest, SaveAndLoad) {
  std::unique_ptr<PairingRegistryDelegateWin> delegate(
      new PairingRegistryDelegateWin());
  delegate->SetRootKeys(privileged_.Handle(), unprivileged_.Handle());

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

  // Verify that the only remaining value is |pairing2|.
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
TEST_F(PairingRegistryDelegateWinTest, Stateless) {
  std::unique_ptr<PairingRegistryDelegateWin> load_delegate(
      new PairingRegistryDelegateWin());
  load_delegate->SetRootKeys(privileged_.Handle(), unprivileged_.Handle());
  std::unique_ptr<PairingRegistryDelegateWin> save_delegate(
      new PairingRegistryDelegateWin());
  save_delegate->SetRootKeys(privileged_.Handle(), unprivileged_.Handle());

  PairingRegistry::Pairing pairing(base::Time::Now(), "xxx", "xxx", "xxx");
  EXPECT_TRUE(save_delegate->Save(pairing));
  EXPECT_EQ(load_delegate->Load(pairing.client_id()), pairing);
}

TEST_F(PairingRegistryDelegateWinTest, Unprivileged) {
  std::unique_ptr<PairingRegistryDelegateWin> delegate(
      new PairingRegistryDelegateWin());
  delegate->SetRootKeys(privileged_.Handle(), unprivileged_.Handle());

  PairingRegistry::Pairing pairing(base::Time::Now(), "xxx", "xxx", "xxx");
  EXPECT_TRUE(delegate->Save(pairing));
  EXPECT_EQ(delegate->Load(pairing.client_id()), pairing);

  // Strip the delegate from write access and validate that it still can be used
  // to read the pairings.
  delegate = std::make_unique<PairingRegistryDelegateWin>();
  delegate->SetRootKeys(nullptr, unprivileged_.Handle());

  PairingRegistry::Pairing unprivileged_pairing =
      delegate->Load(pairing.client_id());
  EXPECT_EQ(pairing.client_id(), unprivileged_pairing.client_id());
  EXPECT_EQ(pairing.client_name(), unprivileged_pairing.client_name());
  EXPECT_EQ(pairing.created_time(), unprivileged_pairing.created_time());

  // Verify that the shared secret if not available.
  EXPECT_TRUE(unprivileged_pairing.shared_secret().empty());

  // Verify that a pairing cannot be saved.
  EXPECT_FALSE(delegate->Save(pairing));
}

}  // namespace remoting
