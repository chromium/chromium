// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include <memory>
#include <optional>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/common/pref_names.h"  // nogncheck
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "components/account_id/account_id.h"  // nogncheck
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

namespace {

using testing::Return;
using testing::_;

using LoadErrorBehavior = ExtensionRegistrar::LoadErrorBehavior;

class TestExtensionSystem : public MockExtensionSystem {
 public:
  explicit TestExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context) {}

  TestExtensionSystem(const TestExtensionSystem&) = delete;
  TestExtensionSystem& operator=(const TestExtensionSystem&) = delete;

  ~TestExtensionSystem() override {}
};

class TestExtensionRegistrarDelegate : public ExtensionRegistrar::Delegate {
 public:
  TestExtensionRegistrarDelegate() = default;

  TestExtensionRegistrarDelegate(const TestExtensionRegistrarDelegate&) =
      delete;
  TestExtensionRegistrarDelegate& operator=(
      const TestExtensionRegistrarDelegate&) = delete;

  ~TestExtensionRegistrarDelegate() override = default;

  // ExtensionRegistrar::Delegate:
  MOCK_METHOD2(PreAddExtension,
               void(const Extension* extension,
                    const Extension* old_extension));
  MOCK_METHOD1(PostActivateExtension,
               void(scoped_refptr<const Extension> extension));
  MOCK_METHOD1(PostDeactivateExtension,
               void(scoped_refptr<const Extension> extension));
  MOCK_METHOD3(LoadExtensionForReload,
               void(const ExtensionId& extension_id,
                    const base::FilePath& path,
                    LoadErrorBehavior load_error_behavior));
  MOCK_METHOD1(CanEnableExtension, bool(const Extension* extension));
  MOCK_METHOD1(CanDisableExtension, bool(const Extension* extension));
  MOCK_METHOD1(ShouldBlockExtension, bool(const Extension* extension));
};

}  // namespace

class ExtensionRegistrarTest : public ExtensionsTest {
 public:
  ExtensionRegistrarTest() = default;

  ExtensionRegistrarTest(const ExtensionRegistrarTest&) = delete;
  ExtensionRegistrarTest& operator=(const ExtensionRegistrarTest&) = delete;

  ~ExtensionRegistrarTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(&factory_);
    extension_ = ExtensionBuilder("extension").Build();
    registrar_.emplace(browser_context(), delegate());

    // Mock defaults.
    ON_CALL(delegate_, CanEnableExtension(extension_.get()))
        .WillByDefault(Return(true));
    ON_CALL(delegate_, CanDisableExtension(extension_.get()))
        .WillByDefault(Return(true));
    ON_CALL(delegate_, ShouldBlockExtension(extension_.get()))
        .WillByDefault(Return(false));
    EXPECT_CALL(delegate_, PostActivateExtension(_)).Times(0);
    EXPECT_CALL(delegate_, PostDeactivateExtension(_)).Times(0);
  }

  void TearDown() override {
    registrar_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  // Boilerplate to verify the mock's expected calls. With a SCOPED_TRACE at the
  // call site, this includes the caller's function in the Gtest trace on
  // failure. Otherwise, the failures are unhelpfully listed at the end of the
  // test.
  void VerifyMock() {
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&delegate_));

    // Re-add the expectations for functions that must not be called.
    EXPECT_CALL(delegate_, PostActivateExtension(_)).Times(0);
    EXPECT_CALL(delegate_, PostDeactivateExtension(_)).Times(0);
  }

  // Adds the extension as enabled and verifies the result.
  void AddEnabledExtension() {
    SCOPED_TRACE("AddEnabledExtension");
    EXPECT_CALL(delegate_, PostActivateExtension(extension_));
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::ENABLED);
    EXPECT_TRUE(IsExtensionReady());

    EXPECT_EQ(disable_reason::DISABLE_NONE,
              ExtensionPrefs::Get(browser_context())
                  ->GetDisableReasons(extension()->id()));

    VerifyMock();
  }

  // Adds the extension as disabled and verifies the result.
  void AddDisabledExtension() {
    SCOPED_TRACE("AddDisabledExtension");
    ExtensionPrefs::Get(browser_context())
        ->SetExtensionDisabled(extension_->id(),
                               disable_reason::DISABLE_USER_ACTION);
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::DISABLED);
    EXPECT_FALSE(IsExtensionReady());
  }

  // Adds the extension as blocklisted and verifies the result.
  void AddBlocklistedExtension() {
    SCOPED_TRACE("AddBlocklistedExtension");
    blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        extension_->id(), BitMapBlocklistState::BLOCKLISTED_MALWARE,
        ExtensionPrefs::Get(browser_context()));
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::BLOCKLISTED);
    EXPECT_FALSE(IsExtensionReady());
  }

  // Adds the extension as blocked and verifies the result.
  void AddBlockedExtension() {
    SCOPED_TRACE("AddBlockedExtension");
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::BLOCKED);
    EXPECT_FALSE(IsExtensionReady());
  }

  // Removes an enabled extension and verifies the result.
  void RemoveEnabledExtension() {
    SCOPED_TRACE("RemoveEnabledExtension");
    // Calling RemoveExtension removes its entry from the enabled list and
    // removes the extension.
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);
    ExpectInSet(ExtensionRegistry::NONE);

    VerifyMock();
  }

  // Removes a disabled extension and verifies the result.
  void RemoveDisabledExtension() {
    SCOPED_TRACE("RemoveDisabledExtension");
    // Calling RemoveExtension removes its entry from the disabled list and
    // removes the extension.
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);
    ExpectInSet(ExtensionRegistry::NONE);

    ExtensionPrefs::Get(browser_context())
        ->DeleteExtensionPrefs(extension_->id());
  }

  // Removes a blocklisted extension and verifies the result.
  void RemoveBlocklistedExtension() {
    SCOPED_TRACE("RemoveBlocklistedExtension");
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_)).Times(0);
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);

    // RemoveExtension does not un-blocklist the extension.
    ExpectInSet(ExtensionRegistry::BLOCKLISTED);

    VerifyMock();
  }

  // Removes a blocked extension and verifies the result.
  void RemoveBlockedExtension() {
    SCOPED_TRACE("RemoveBlockedExtension");
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_)).Times(0);
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);

    // RemoveExtension does not un-block the extension.
    ExpectInSet(ExtensionRegistry::BLOCKED);

    VerifyMock();
  }

  void EnableExtension() {
    SCOPED_TRACE("EnableExtension");
    EXPECT_CALL(delegate_, PostActivateExtension(extension_));
    registrar_->EnableExtension(extension_->id());
    ExpectInSet(ExtensionRegistry::ENABLED);
    EXPECT_TRUE(IsExtensionReady());

    VerifyMock();
  }

  void DisableEnabledExtension() {
    SCOPED_TRACE("DisableEnabledExtension");
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->DisableExtension(extension_->id(),
                                 disable_reason::DISABLE_USER_ACTION);
    ExpectInSet(ExtensionRegistry::DISABLED);
    EXPECT_FALSE(IsExtensionReady());

    VerifyMock();
  }

  void DisableTerminatedExtension() {
    SCOPED_TRACE("DisableTerminatedExtension");
    // PostDeactivateExtension should not be called.
    registrar_->DisableExtension(extension_->id(),
                                 disable_reason::DISABLE_USER_ACTION);
    ExpectInSet(ExtensionRegistry::DISABLED);
    EXPECT_FALSE(IsExtensionReady());
  }

  void TryDisablingNotAshKeeplistedExtension(bool expect_extension_disabled) {
    if (expect_extension_disabled) {
      EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    }

    // Disable extension because it is not in the ash keep list.
    registrar_->DisableExtension(extension_->id(),
                                 disable_reason::DISABLE_NOT_ASH_KEEPLISTED);

    ExtensionRegistry::IncludeFlag include_flag =
        expect_extension_disabled ? ExtensionRegistry::DISABLED
                                  : ExtensionRegistry::ENABLED;
    ExpectInSet(include_flag);
    EXPECT_NE(IsExtensionReady(), expect_extension_disabled);

    VerifyMock();
  }

  void TerminateExtension() {
    SCOPED_TRACE("TerminateExtension");
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->TerminateExtension(extension_->id());
    ExpectInSet(ExtensionRegistry::TERMINATED);
    EXPECT_FALSE(IsExtensionReady());
    VerifyMock();
  }

  void UntrackTerminatedExtension() {
    SCOPED_TRACE("UntrackTerminatedExtension");
    registrar()->UntrackTerminatedExtension(extension()->id());
    ExpectInSet(ExtensionRegistry::NONE);
  }

  // Directs ExtensionRegistrar to reload the extension and verifies the
  // delegate is invoked correctly.
  void ReloadEnabledExtension() {
    SCOPED_TRACE("ReloadEnabledExtension");
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension()));
    EXPECT_CALL(delegate_,
                LoadExtensionForReload(extension()->id(), extension()->path(),
                                       LoadErrorBehavior::kNoisy));
    registrar()->ReloadExtension(extension()->id(), LoadErrorBehavior::kNoisy);
    VerifyMock();

    // ExtensionRegistrar should have disabled the extension in preparation for
    // a reload.
    ExpectInSet(ExtensionRegistry::DISABLED);
    EXPECT_EQ(disable_reason::DISABLE_RELOAD,
              ExtensionPrefs::Get(browser_context())
                  ->GetDisableReasons(extension()->id()));
  }

  // Directs ExtensionRegistrar to reload the terminated extension and verifies
  // the delegate is invoked correctly.
  void ReloadTerminatedExtension() {
    SCOPED_TRACE("ReloadTerminatedExtension");
    EXPECT_CALL(delegate_,
                LoadExtensionForReload(extension()->id(), extension()->path(),
                                       LoadErrorBehavior::kNoisy));
    registrar()->ReloadExtension(extension()->id(), LoadErrorBehavior::kNoisy);
    VerifyMock();

    // The extension should remain in the terminated set until the reload
    // completes successfully.
    ExpectInSet(ExtensionRegistry::TERMINATED);
    // Unlike when reloading an enabled extension, the extension hasn't been
    // disabled and shouldn't have the DISABLE_RELOAD disable reason.
    EXPECT_EQ(disable_reason::DISABLE_NONE,
              ExtensionPrefs::Get(browser_context())
                  ->GetDisableReasons(extension()->id()));
  }

  // Verifies that the extension is in the given set in the ExtensionRegistry
  // and not in other sets.
  void ExpectInSet(ExtensionRegistry::IncludeFlag set_id) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

    EXPECT_EQ(set_id == ExtensionRegistry::ENABLED,
              registry->enabled_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::DISABLED,
              registry->disabled_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::TERMINATED,
              registry->terminated_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::BLOCKLISTED,
              registry->blocklisted_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::BLOCKED,
              registry->blocked_extensions().Contains(extension_->id()));
  }

  bool IsExtensionReady() {
    return ExtensionRegistry::Get(browser_context())
        ->ready_extensions()
        .Contains(extension_->id());
  }

  ExtensionRegistrar* registrar() { return &registrar_.value(); }
  TestExtensionRegistrarDelegate* delegate() { return &delegate_; }

  scoped_refptr<const Extension> extension() const { return extension_; }

 private:
  MockExtensionSystemFactory<TestExtensionSystem> factory_;
  // Use NiceMock to allow uninteresting calls, so the delegate can be queried
  // any number of times. We will explicitly disallow unexpected calls to
  // PostActivateExtension/PostDeactivateExtension with EXPECT_CALL statements.
  testing::NiceMock<TestExtensionRegistrarDelegate> delegate_;
  scoped_refptr<const Extension> extension_;

  // Initialized in SetUp().
  std::optional<ExtensionRegistrar> registrar_;
};

TEST_F(ExtensionRegistrarTest, Basic) {
  AddEnabledExtension();
  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, AlreadyEnabled) {
  AddEnabledExtension();

  // As the extension is already enabled, this is a no-op.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::ENABLED);
  EXPECT_TRUE(IsExtensionReady());

  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, Disable) {
  AddEnabledExtension();

  // Disable the extension before removing it.
  DisableEnabledExtension();
  RemoveDisabledExtension();
}

TEST_F(ExtensionRegistrarTest, DisableAndEnable) {
  AddEnabledExtension();

  // Disable then enable the extension.
  DisableEnabledExtension();
  EnableExtension();

  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, AddDisabled) {
  // An extension can be added as disabled, then removed.
  AddDisabledExtension();
  RemoveDisabledExtension();

  // An extension can be added as disabled, then enabled.
  AddDisabledExtension();
  EnableExtension();
  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, AddForceEnabled) {
  // Prevent the extension from being disabled.
  ON_CALL(*delegate(), CanDisableExtension(extension().get()))
      .WillByDefault(Return(false));
  AddEnabledExtension();

  // Extension cannot be disabled.
  registrar()->DisableExtension(extension()->id(),
                                disable_reason::DISABLE_USER_ACTION);
  ExpectInSet(ExtensionRegistry::ENABLED);
}

TEST_F(ExtensionRegistrarTest, AddForceDisabled) {
  // Prevent the extension from being enabled.
  ON_CALL(*delegate(), CanEnableExtension(extension().get()))
      .WillByDefault(Return(false));
  AddDisabledExtension();

  // Extension cannot be enabled.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::DISABLED);
}

TEST_F(ExtensionRegistrarTest, AddBlocklisted) {
  AddBlocklistedExtension();

  // A blocklisted extension cannot be enabled/disabled/reloaded.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::BLOCKLISTED);
  registrar()->DisableExtension(extension()->id(),
                                disable_reason::DISABLE_USER_ACTION);
  ExpectInSet(ExtensionRegistry::BLOCKLISTED);
  registrar()->ReloadExtension(extension()->id(), LoadErrorBehavior::kQuiet);
  ExpectInSet(ExtensionRegistry::BLOCKLISTED);

  RemoveBlocklistedExtension();
}

TEST_F(ExtensionRegistrarTest, AddBlocked) {
  // Block extensions.
  ON_CALL(*delegate(), ShouldBlockExtension(extension().get()))
      .WillByDefault(Return(true));

  // A blocked extension can be added.
  AddBlockedExtension();

  // Extension cannot be enabled/disabled.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::BLOCKED);
  registrar()->DisableExtension(extension()->id(),
                                disable_reason::DISABLE_USER_ACTION);
  ExpectInSet(ExtensionRegistry::BLOCKED);

  RemoveBlockedExtension();
}

TEST_F(ExtensionRegistrarTest, TerminateExtension) {
  AddEnabledExtension();
  TerminateExtension();

  // Calling RemoveExtension removes its entry from the terminated list.
  registrar()->RemoveExtension(extension()->id(),
                               UnloadedExtensionReason::UNINSTALL);
  ExpectInSet(ExtensionRegistry::NONE);
}

TEST_F(ExtensionRegistrarTest, DisableTerminatedExtension) {
  AddEnabledExtension();
  TerminateExtension();
  DisableTerminatedExtension();
  RemoveDisabledExtension();
}

TEST_F(ExtensionRegistrarTest, EnableTerminatedExtension) {
  AddEnabledExtension();
  TerminateExtension();

  // Enable the terminated extension.
  UntrackTerminatedExtension();
  AddEnabledExtension();

  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, ReloadExtension) {
  AddEnabledExtension();
  ReloadEnabledExtension();

  // Add the now-reloaded extension back into the registrar.
  AddEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, RemoveReloadedExtension) {
  AddEnabledExtension();
  ReloadEnabledExtension();

  // Simulate the delegate failing to load the extension and removing it
  // instead.
  RemoveDisabledExtension();

  // Attempting to reload it silently fails.
  registrar()->ReloadExtension(extension()->id(), LoadErrorBehavior::kQuiet);
  ExpectInSet(ExtensionRegistry::NONE);
}

TEST_F(ExtensionRegistrarTest, ReloadTerminatedExtension) {
  AddEnabledExtension();
  TerminateExtension();

  // Reload the terminated extension.
  ReloadTerminatedExtension();

  // Complete the reload by adding the extension. Expect the extension to be
  // enabled once re-added to the registrar, since ExtensionPrefs shouldn't say
  // it's disabled.
  AddEnabledExtension();
}

// Test that an extension which is not controlled (e.g. by policy) and which is
// not on the ash keep-list can be disabled.
TEST_F(ExtensionRegistrarTest, DisableNotAshKeeplistedExtension) {
  ON_CALL(*delegate(), CanDisableExtension(extension().get()))
      .WillByDefault(Return(true));
  AddEnabledExtension();

  TryDisablingNotAshKeeplistedExtension(/* expect_extension_disabled= */ true);
}

#if BUILDFLAG(IS_CHROMEOS)
// Test that a controlled extension that is not on the ash keep-list can be
// disabled if ash is disabled.
TEST_F(ExtensionRegistrarTest,
       DisableNotAshKeeplistedForceInstalledExtensionIfAshDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(ash::standalone_browser::GetFeatureRefs(), {});
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
  auto* primary_user =
      fake_user_manager->AddUser(AccountId::FromUserEmail("test@test"));
  fake_user_manager->UserLoggedIn(primary_user->GetAccountId(),
                                  primary_user->username_hash(),
                                  /*browser_restart=*/false,
                                  /*is_child=*/false);
  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));

  static_cast<TestingPrefServiceSimple*>(pref_service())
      ->registry()
      ->RegisterIntegerPref(
          prefs::kLacrosLaunchSwitch,
          static_cast<int>(
              ash::standalone_browser::LacrosAvailability::kLacrosOnly));
  EXPECT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());

  // Prevent the extension from being disabled (by the user).
  ON_CALL(*delegate(), CanDisableExtension(extension().get()))
      .WillByDefault(Return(false));
  AddEnabledExtension();

  TryDisablingNotAshKeeplistedExtension(/* expect_extension_disabled= */ true);
}

// Test that a controlled extension that is not on the ash keep-list cannot be
// disabled if ash is still enabled.
TEST_F(ExtensionRegistrarTest,
       NotDisableNotAshKeeplistedForceInstalledExtensionIfAshEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, ash::standalone_browser::GetFeatureRefs());
  static_cast<TestingPrefServiceSimple*>(pref_service())
      ->registry()
      ->RegisterIntegerPref(
          prefs::kLacrosLaunchSwitch,
          static_cast<int>(
              ash::standalone_browser::LacrosAvailability::kLacrosOnly));
  EXPECT_TRUE(crosapi::browser_util::IsAshWebBrowserEnabled());

  // Prevent the extension from being disabled (by the user).
  ON_CALL(*delegate(), CanDisableExtension(extension().get()))
      .WillByDefault(Return(false));
  AddEnabledExtension();

  TryDisablingNotAshKeeplistedExtension(/* expect_extension_disabled= */ false);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
