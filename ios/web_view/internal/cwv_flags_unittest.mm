// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_flags_internal.h"

#include <memory>
#include <set>

#import "base/containers/contains.h"
#include "base/test/task_environment.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

class CWVFlagsTest : public PlatformTest {
 protected:
  CWVFlagsTest() {
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    flags_ui::PrefServiceFlagsStorage::RegisterPrefs(pref_registry.get());

    scoped_refptr<PersistentPrefStore> pref_store = new InMemoryPrefStore();
    PrefServiceFactory factory;
    factory.set_user_prefs(pref_store);

    pref_service_ = factory.Create(pref_registry.get());
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        pref_service_.get());
    flags_ = [[CWVFlags alloc] initWithPrefService:pref_service_.get()];
  }

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<flags_ui::PrefServiceFlagsStorage> flags_storage_;
  CWVFlags* flags_;
};

// Tests CWVFlags' usesSyncAndWalletSandbox setter.
TEST_F(CWVFlagsTest, SetUsesSyncAndWalletSandbox) {
  flags_.usesSyncAndWalletSandbox = YES;
  std::set<std::string> stored_flags = flags_storage_->GetFlags();
  EXPECT_TRUE(base::Contains(stored_flags, kUseSyncSandboxFlagName));
  EXPECT_TRUE(base::Contains(stored_flags, kUseWalletSandboxFlagNameEnabled));

  flags_.usesSyncAndWalletSandbox = NO;
  stored_flags = flags_storage_->GetFlags();
  EXPECT_FALSE(base::Contains(stored_flags, kUseSyncSandboxFlagName));
  EXPECT_FALSE(base::Contains(stored_flags, kUseWalletSandboxFlagNameEnabled));
}

// Tests CWVFlag's usesSyncAndWalletSandbox getter.
TEST_F(CWVFlagsTest, GetUsesSyncAndWalletSadnbox) {
  flags_storage_->SetFlags(
      {kUseSyncSandboxFlagName, kUseWalletSandboxFlagNameEnabled});
  EXPECT_TRUE(flags_.usesSyncAndWalletSandbox);

  flags_storage_->SetFlags({kUseWalletSandboxFlagNameDisabled});
  EXPECT_FALSE(flags_.usesSyncAndWalletSandbox);
}

}  // namespace ios_web_view
