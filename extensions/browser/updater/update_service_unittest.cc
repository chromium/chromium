// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/uninstall_ping_sender.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeUpdateClient : public update_client::UpdateClient {
 public:
  FakeUpdateClient();

  FakeUpdateClient(const FakeUpdateClient&) = delete;
  FakeUpdateClient& operator=(const FakeUpdateClient&) = delete;

  // Returns the data we've gotten from the CrxDataCallback for ids passed to
  // the Update function.
  std::vector<std::optional<update_client::CrxComponent>>* data() {
    return &data_;
  }

  // Used for tests that uninstall pings get requested properly.
  struct UninstallPing {
    std::string id;
    base::Version version;
    int reason;
    UninstallPing(const std::string& id,
                  const base::Version& version,
                  int reason)
        : id(id), version(version), reason(reason) {}
  };
  std::vector<UninstallPing>& uninstall_pings() { return uninstall_pings_; }

  struct UpdateRequest {
    std::vector<std::string> extension_ids;
    CrxStateChangeCallback state_change_callback;
    update_client::Callback callback;
  };

  // update_client::UpdateClient
  void AddObserver(Observer* observer) override {
    if (observer)
      observers_.push_back(observer);
  }

  void RemoveObserver(Observer* observer) override {}

  base::RepeatingClosure Install(
      const std::string& id,
      CrxDataCallback crx_data_callback,
      CrxStateChangeCallback crx_state_change_callback,
      update_client::Callback callback) override {
    return base::DoNothing();
  }

  void Update(const std::vector<std::string>& ids,
              CrxDataCallback crx_data_callback,
              CrxStateChangeCallback crx_state_change_callback,
              bool is_foreground,
              update_client::Callback callback) override;

  void CheckForUpdate(const std::string& id,
                      CrxDataCallback crx_data_callback,
                      CrxStateChangeCallback crx_state_change_callback,
                      bool is_foreground,
                      update_client::Callback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  bool GetCrxUpdateState(
      const std::string& id,
      update_client::CrxUpdateItem* update_item) const override {
    update_item->next_version = base::Version("2.0");
    std::map<std::string, std::string> custom_attributes;
    if (is_malware_update_item_)
      custom_attributes["_malware"] = "true";
    if (allowlist_state == extensions::ALLOWLIST_ALLOWLISTED)
      custom_attributes["_esbAllowlist"] = "true";
    else if (allowlist_state == extensions::ALLOWLIST_NOT_ALLOWLISTED)
      custom_attributes["_esbAllowlist"] = "true";

    if (!custom_attributes.empty())
      update_item->custom_updatecheck_data = custom_attributes;
    return true;
  }

  bool IsUpdating(const std::string& id) const override { return false; }

  void Stop() override {}

  void SendPing(const update_client::CrxComponent& crx_component,
                PingParams ping_params,
                update_client::Callback callback) override {
    uninstall_pings_.emplace_back(crx_component.app_id, crx_component.version,
                                  ping_params.extra_code1);
  }

  void set_delay_update() { delay_update_ = true; }

  void set_is_malware_update_item() { is_malware_update_item_ = true; }

  void set_allowlist_state(extensions::AllowlistState state) {
    allowlist_state = state;
  }

  bool delay_update() const { return delay_update_; }

  UpdateRequest& update_request(int index) { return delayed_requests_[index]; }

  int num_update_requests() const {
    return static_cast<int>(delayed_requests_.size());
  }

  void ChangeDelayedUpdateState(int index,
                                update_client::ComponentState state) {
    UpdateRequest& request = update_request(index);
    ChangeUpdateState(request, state);
  }

  void FinishDelayedUpdate(int index) {
    UpdateRequest& request = update_request(index);
    Finish(request);
  }

  void RunDelayedUpdate(int index) {
    UpdateRequest& request = update_request(index);
    RunUpdate(request);
  }

 protected:
  ~FakeUpdateClient() override = default;

  void ChangeUpdateState(UpdateRequest& request,
                         update_client::ComponentState state) {
    for (const std::string& id : request.extension_ids) {
      update_client::CrxUpdateItem item;
      item.id = id;
      item.state = state;

      std::map<std::string, std::string> custom_attributes;
      if (is_malware_update_item_) {
        custom_attributes["_malware"] = "true";
      }
      if (allowlist_state == extensions::ALLOWLIST_ALLOWLISTED) {
        custom_attributes["_esbAllowlist"] = "true";
      } else if (allowlist_state == extensions::ALLOWLIST_NOT_ALLOWLISTED) {
        custom_attributes["_esbAllowlist"] = "true";
      }

      if (!custom_attributes.empty()) {
        item.custom_updatecheck_data = custom_attributes;
      }

      item.next_version = base::Version("2.0");

      request.state_change_callback.Run(item);
    }
  }

  void Finish(UpdateRequest& request) {
    std::move(request.callback).Run(update_client::Error::NONE);
  }

  void RunUpdate(UpdateRequest& request) {
    ChangeUpdateState(request, update_client::ComponentState::kChecking);
    ChangeUpdateState(request, update_client::ComponentState::kCanUpdate);
    ChangeUpdateState(request, update_client::ComponentState::kDownloading);
    ChangeUpdateState(request, update_client::ComponentState::kUpdating);
    ChangeUpdateState(request, update_client::ComponentState::kUpdated);
    Finish(request);
  }

  std::vector<std::optional<update_client::CrxComponent>> data_;
  std::vector<UninstallPing> uninstall_pings_;
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_;

  bool delay_update_ = false;
  bool is_malware_update_item_ = false;
  extensions::AllowlistState allowlist_state = extensions::ALLOWLIST_UNDEFINED;
  std::vector<UpdateRequest> delayed_requests_;
};

FakeUpdateClient::FakeUpdateClient() = default;

void FakeUpdateClient::Update(const std::vector<std::string>& ids,
                              CrxDataCallback crx_data_callback,
                              CrxStateChangeCallback crx_state_change_callback,
                              bool is_foreground,
                              update_client::Callback callback) {
  std::move(crx_data_callback)
      .Run(
          ids,
          base::BindLambdaForTesting(
              [&](const std::vector<std::optional<update_client::CrxComponent>>&
                      output) { data_ = output; }));

  UpdateRequest request{ids, crx_state_change_callback, std::move(callback)};

  if (delay_update()) {
    delayed_requests_.push_back(std::move(request));
  } else {
    RunUpdate(request);
  }
}

}  // namespace

namespace extensions {

namespace {

// A global variable for controlling whether uninstalls should cause uninstall
// pings to be sent.
UninstallPingSender::FilterResult g_should_ping =
    UninstallPingSender::DO_NOT_SEND_PING;

// Helper method to serve as an uninstall ping filter.
UninstallPingSender::FilterResult ShouldPing(const Extension* extension,
                                             UninstallReason reason) {
  return g_should_ping;
}

// A fake ExtensionSystem that lets us intercept calls to install new
// versions of an extension.
class FakeExtensionSystem : public MockExtensionSystem {
 public:
  using InstallUpdateCallback = MockExtensionSystem::InstallUpdateCallback;
  explicit FakeExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context) {}
  ~FakeExtensionSystem() override = default;

  struct InstallUpdateRequest {
    InstallUpdateRequest(const ExtensionId& extension_id,
                         const base::FilePath& temp_dir,
                         bool install_immediately)
        : extension_id(extension_id),
          temp_dir(temp_dir),
          install_immediately(install_immediately) {}
    ExtensionId extension_id;
    base::FilePath temp_dir;
    bool install_immediately;
  };

  std::vector<InstallUpdateRequest>* install_requests() {
    return &install_requests_;
  }

  void set_install_callback(base::OnceClosure callback) {
    next_install_callback_ = std::move(callback);
  }

  // ExtensionSystem override
  void InstallUpdate(const ExtensionId& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override {
    base::DeletePathRecursively(temp_dir);
    install_requests_.emplace_back(extension_id, temp_dir, install_immediately);
    if (!next_install_callback_.is_null()) {
      std::move(next_install_callback_).Run();
    }
    std::move(install_update_callback).Run(std::nullopt);
  }

  void PerformActionBasedOnOmahaAttributes(
      const ExtensionId& extension_id,
      const base::Value::Dict& attributes) override {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("1").SetVersion("1.2").SetID(extension_id).Build();
    const bool is_malware = attributes.FindBool("_malware").value_or(false);
    if (is_malware) {
      registry->AddDisabled(extension);
    } else {
      registry->AddEnabled(extension);
    }

    const std::optional<bool> maybe_allowlisted =
        attributes.FindBool("_esbAllowlist");
    if (maybe_allowlisted) {
      extension_allowlist_states_[extension_id] =
          maybe_allowlisted.value() ? ALLOWLIST_ALLOWLISTED
                                    : ALLOWLIST_NOT_ALLOWLISTED;
    }
  }

  bool FinishDelayedInstallationIfReady(const ExtensionId& extension_id,
                                        bool install_immediately) override {
    return false;
  }

  AllowlistState GetExtensionAllowlistState(const ExtensionId& extension_id) {
    if (!base::Contains(extension_allowlist_states_, extension_id))
      return ALLOWLIST_UNDEFINED;

    return extension_allowlist_states_[extension_id];
  }

 private:
  std::vector<InstallUpdateRequest> install_requests_;
  base::OnceClosure next_install_callback_;
  base::flat_map<std::string, AllowlistState> extension_allowlist_states_;
};

class UpdateServiceTest : public ExtensionsTest {
 public:
  UpdateServiceTest() = default;
  ~UpdateServiceTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(
        &fake_extension_system_factory_);
    extensions_browser_client()->SetUpdateClientFactory(base::BindRepeating(
        &UpdateServiceTest::CreateUpdateClient, base::Unretained(this)));

    update_service_ = UpdateService::Get(browser_context());
  }

 protected:
  UpdateService* update_service() const { return update_service_; }
  FakeUpdateClient* update_client() const { return update_client_.get(); }

  update_client::UpdateClient* CreateUpdateClient() {
    // We only expect that this will get called once, so consider it an error
    // if our update_client_ is already non-null.
    EXPECT_EQ(nullptr, update_client_.get());
    update_client_ = base::MakeRefCounted<FakeUpdateClient>();
    return update_client_.get();
  }

  // Helper function that creates a file at |relative_path| within |directory|
  // and fills it with |content|.
  bool AddFileToDirectory(const base::FilePath& directory,
                          const base::FilePath& relative_path,
                          const std::string& content) {
    base::FilePath full_path = directory.Append(relative_path);
    return base::CreateDirectory(full_path.DirName()) &&
           base::WriteFile(full_path, content);
  }

  FakeExtensionSystem* extension_system() {
    return static_cast<FakeExtensionSystem*>(
        fake_extension_system_factory_.GetForBrowserContext(browser_context()));
  }

  void BasicUpdateOperations(bool install_immediately,
                             bool provide_update_found_callback) {
    // Create a temporary directory that a fake extension will live in and fill
    // it with some test files.
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath foo_js(FILE_PATH_LITERAL("foo.js"));
    base::FilePath bar_html(FILE_PATH_LITERAL("bar/bar.html"));
    ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), foo_js, "hello"))
        << "Failed to write " << temp_dir.GetPath().value() << "/"
        << foo_js.value();
    ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), bar_html, "world"));

    scoped_refptr<const Extension> extension1 =
        ExtensionBuilder("Foo")
            .SetVersion("1.0")
            .SetID(crx_file::id_util::GenerateId("foo_extension"))
            .SetPath(temp_dir.GetPath())
            .Build();

    ExtensionRegistry::Get(browser_context())->AddEnabled(extension1);

    ExtensionUpdateCheckParams update_check_params;
    update_check_params.update_info[extension1->id()] = ExtensionUpdateData();
    update_check_params.install_immediately = install_immediately;

    // Start an update check and verify that the UpdateClient was sent the right
    // data.
    bool executed = false;
    std::string found_id;
    base::Version found_version;

    UpdateFoundCallback update_found_callback;
    if (provide_update_found_callback) {
      update_found_callback = base::BindLambdaForTesting(
          [&found_id, &found_version](const std::string& id,
                                      const base::Version& version) {
            found_id = id;
            found_version = version;
          });
    }
    update_service()->StartUpdateCheck(
        update_check_params, update_found_callback,
        base::BindOnce([](bool* executed) { *executed = true; }, &executed));
    ASSERT_TRUE(executed);
    if (provide_update_found_callback) {
      EXPECT_EQ(found_id, extension1->id());
      EXPECT_EQ(found_version, base::Version("2.0"));
    }
    const auto* data = update_client()->data();
    ASSERT_NE(nullptr, data);
    ASSERT_EQ(1u, data->size());

    ASSERT_EQ(data->at(0)->version, extension1->version());
    update_client::CrxInstaller* installer = data->at(0)->installer.get();
    ASSERT_NE(installer, nullptr);

    // Test the install callback.
    base::ScopedTempDir new_version_dir;
    ASSERT_TRUE(new_version_dir.CreateUniqueTempDir());

    bool done = false;
    installer->Install(
        new_version_dir.GetPath(), std::string(), nullptr, base::DoNothing(),
        base::BindOnce(
            [](bool* done, const update_client::CrxInstaller::Result& result) {
              *done = true;
              EXPECT_EQ(result.result.category_,
                        update_client::ErrorCategory::kNone);
              EXPECT_EQ(result.result.code_, 0);
              EXPECT_EQ(result.result.extra_, 0);
            },
            &done));

    base::RunLoop run_loop;
    extension_system()->set_install_callback(run_loop.QuitClosure());
    run_loop.Run();

    std::vector<FakeExtensionSystem::InstallUpdateRequest>* requests =
        extension_system()->install_requests();
    ASSERT_EQ(1u, requests->size());

    const auto& request = requests->at(0);
    EXPECT_EQ(request.extension_id, extension1->id());
    EXPECT_EQ(request.temp_dir.value(), new_version_dir.GetPath().value());
    EXPECT_EQ(install_immediately, request.install_immediately);
    EXPECT_TRUE(done);
  }

 private:
  raw_ptr<UpdateService, DanglingUntriaged> update_service_ = nullptr;
  scoped_refptr<FakeUpdateClient> update_client_;
  MockExtensionSystemFactory<FakeExtensionSystem>
      fake_extension_system_factory_;
};

TEST_F(UpdateServiceTest, BasicUpdateOperations_InstallImmediately) {
  BasicUpdateOperations(/* install_immediately */ true,
                        /* provide_update_found_callback */ true);
}

TEST_F(UpdateServiceTest, BasicUpdateOperations_NotInstallImmediately) {
  BasicUpdateOperations(/* install_immediately */ false,
                        /* provide_update_found_callback */ true);
}

TEST_F(UpdateServiceTest, BasicUpdateOperations_NoUpdateFoundCallback) {
  BasicUpdateOperations(/* install_immediately */ true,
                        /* provide_update_found_callback */ false);
}

TEST_F(UpdateServiceTest, UninstallPings) {
  UninstallPingSender sender(ExtensionRegistry::Get(browser_context()),
                             base::BindRepeating(&ShouldPing));

  // Build 3 extensions.
  scoped_refptr<const Extension> extension1 =
      ExtensionBuilder("1").SetVersion("1.2").Build();
  scoped_refptr<const Extension> extension2 =
      ExtensionBuilder("2").SetVersion("2.3").Build();
  scoped_refptr<const Extension> extension3 =
      ExtensionBuilder("3").SetVersion("3.4").Build();
  EXPECT_TRUE(extension1->id() != extension2->id() &&
              extension1->id() != extension3->id() &&
              extension2->id() != extension3->id());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  // Run tests for each uninstall reason.
  for (int reason_val = static_cast<int>(UNINSTALL_REASON_FOR_TESTING);
       reason_val < static_cast<int>(UNINSTALL_REASON_MAX); ++reason_val) {
    UninstallReason reason = static_cast<UninstallReason>(reason_val);

    // Start with 2 enabled and 1 disabled extensions.
    EXPECT_TRUE(registry->AddEnabled(extension1)) << reason;
    EXPECT_TRUE(registry->AddEnabled(extension2)) << reason;
    EXPECT_TRUE(registry->AddDisabled(extension3)) << reason;

    // Uninstall the first extension, instructing our filter not to send pings,
    // and verify none were sent.
    g_should_ping = UninstallPingSender::DO_NOT_SEND_PING;
    EXPECT_TRUE(registry->RemoveEnabled(extension1->id())) << reason;
    registry->TriggerOnUninstalled(extension1.get(), reason);
    EXPECT_TRUE(update_client()->uninstall_pings().empty()) << reason;

    // Uninstall the second and third extensions, instructing the filter to
    // send pings, and make sure we got the expected data.
    g_should_ping = UninstallPingSender::SEND_PING;
    EXPECT_TRUE(registry->RemoveEnabled(extension2->id())) << reason;
    registry->TriggerOnUninstalled(extension2.get(), reason);
    EXPECT_TRUE(registry->RemoveDisabled(extension3->id())) << reason;
    registry->TriggerOnUninstalled(extension3.get(), reason);

    std::vector<FakeUpdateClient::UninstallPing>& pings =
        update_client()->uninstall_pings();
    ASSERT_EQ(2u, pings.size()) << reason;

    EXPECT_EQ(extension2->id(), pings[0].id) << reason;
    EXPECT_EQ(extension2->version(), pings[0].version) << reason;
    EXPECT_EQ(reason, pings[0].reason) << reason;

    EXPECT_EQ(extension3->id(), pings[1].id) << reason;
    EXPECT_EQ(extension3->version(), pings[1].version) << reason;
    EXPECT_EQ(reason, pings[1].reason) << reason;

    pings.clear();
  }
}

TEST_F(UpdateServiceTest, CheckOmahaMalwareAttributes) {
  ExtensionId extension_id = crx_file::id_util::GenerateId("id");
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  scoped_refptr<const Extension> extension1 =
      ExtensionBuilder("1").SetVersion("1.2").SetID(extension_id).Build();
  EXPECT_TRUE(registry->AddEnabled(extension1));

  update_client()->set_is_malware_update_item();
  update_client()->set_delay_update();

  ExtensionUpdateCheckParams update_check_params;
  update_check_params.update_info[extension_id] = ExtensionUpdateData();

  bool executed = false;
  bool found_update = false;
  update_service()->StartUpdateCheck(
      update_check_params,
      base::BindLambdaForTesting(
          [&found_update](const std::string& id, const base::Version& version) {
            found_update = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed));
  EXPECT_FALSE(found_update);
  EXPECT_FALSE(executed);

  const auto& request = update_client()->update_request(0);
  EXPECT_THAT(request.extension_ids, testing::ElementsAre(extension_id));

  update_client()->ChangeDelayedUpdateState(
      0, update_client::ComponentState::kChecking);

  EXPECT_FALSE(registry->disabled_extensions().GetByID(extension_id));
  EXPECT_EQ(extensions::ALLOWLIST_UNDEFINED,
            extension_system()->GetExtensionAllowlistState(extension_id));

  update_client()->ChangeDelayedUpdateState(
      0, update_client::ComponentState::kUpToDate);
  update_client()->FinishDelayedUpdate(0);

  EXPECT_TRUE(registry->disabled_extensions().GetByID(extension_id));
}

TEST_F(UpdateServiceTest, CheckOmahaAllowlistAttributes) {
  ExtensionId extension_id = crx_file::id_util::GenerateId("id");
  scoped_refptr<const Extension> extension1 =
      ExtensionBuilder("1").SetVersion("1.2").SetID(extension_id).Build();

  update_client()->set_allowlist_state(extensions::ALLOWLIST_ALLOWLISTED);
  update_client()->set_delay_update();

  ExtensionUpdateCheckParams update_check_params;
  update_check_params.update_info[extension_id] = ExtensionUpdateData();

  bool executed = false;
  bool found_update = false;
  update_service()->StartUpdateCheck(
      update_check_params,
      base::BindLambdaForTesting(
          [&found_update](const std::string& id, const base::Version& version) {
            found_update = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed));
  EXPECT_FALSE(found_update);
  EXPECT_FALSE(executed);

  const auto& request = update_client()->update_request(0);
  EXPECT_THAT(request.extension_ids, testing::ElementsAre(extension_id));

  update_client()->RunDelayedUpdate(0);

  EXPECT_EQ(extensions::ALLOWLIST_ALLOWLISTED,
            extension_system()->GetExtensionAllowlistState(extension_id));
}

TEST_F(UpdateServiceTest, CheckNoOmahaAttributes) {
  ExtensionId extension_id = crx_file::id_util::GenerateId("id");
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  scoped_refptr<const Extension> extension1 =
      ExtensionBuilder("1").SetVersion("1.2").SetID(extension_id).Build();
  EXPECT_TRUE(registry->AddDisabled(extension1));

  update_client()->set_delay_update();

  ExtensionUpdateCheckParams update_check_params;
  update_check_params.update_info[extension_id] = ExtensionUpdateData();

  bool found_update = false;
  bool executed = false;
  update_service()->StartUpdateCheck(
      update_check_params,
      base::BindLambdaForTesting(
          [&found_update](const std::string& id, const base::Version& version) {
            found_update = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed));
  EXPECT_FALSE(found_update);
  EXPECT_FALSE(executed);

  const auto& request = update_client()->update_request(0);
  EXPECT_THAT(request.extension_ids, testing::ElementsAre(extension_id));

  update_client()->ChangeDelayedUpdateState(
      0, update_client::ComponentState::kChecking);
  update_client()->ChangeDelayedUpdateState(
      0, update_client::ComponentState::kUpToDate);
  update_client()->FinishDelayedUpdate(0);

  EXPECT_TRUE(registry->enabled_extensions().GetByID(extension_id));
  EXPECT_EQ(extensions::ALLOWLIST_UNDEFINED,
            extension_system()->GetExtensionAllowlistState(extension_id));
}

TEST_F(UpdateServiceTest, InProgressUpdate_Successful) {
  base::HistogramTester histogram_tester;
  update_client()->set_delay_update();
  ExtensionUpdateCheckParams update_check_params;

  // Extensions with empty IDs will be ignored.
  update_check_params.update_info["A"] = ExtensionUpdateData();
  update_check_params.update_info["B"] = ExtensionUpdateData();
  update_check_params.update_info["C"] = ExtensionUpdateData();
  update_check_params.update_info["D"] = ExtensionUpdateData();
  update_check_params.update_info["E"] = ExtensionUpdateData();

  bool found_update = false;
  bool executed = false;
  update_service()->StartUpdateCheck(
      update_check_params,
      base::BindLambdaForTesting(
          [&found_update](const std::string& id, const base::Version& version) {
            found_update = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed));
  EXPECT_FALSE(found_update);
  EXPECT_FALSE(executed);

  const auto& request = update_client()->update_request(0);
  EXPECT_THAT(request.extension_ids,
              testing::ElementsAre("A", "B", "C", "D", "E"));

  update_client()->RunDelayedUpdate(0);
  EXPECT_TRUE(executed);
}

// Incorrect deduplicating of the same extension ID but with different flags may
// lead to incorrect behaviour: corrupted extension won't be reinstalled.
TEST_F(UpdateServiceTest, InProgressUpdate_DuplicateWithDifferentData) {
  base::HistogramTester histogram_tester;
  update_client()->set_delay_update();
  ExtensionUpdateCheckParams uc1, uc2;
  uc1.update_info["A"] = ExtensionUpdateData();

  uc2.update_info["A"] = ExtensionUpdateData();
  uc2.update_info["A"].install_source = "reinstall";
  uc2.update_info["A"].is_corrupt_reinstall = true;

  bool found_update1 = false;
  bool executed1 = false;
  update_service()->StartUpdateCheck(
      uc1,
      base::BindLambdaForTesting(
          [&found_update1](const std::string& id,
                           const base::Version& version) {
            found_update1 = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed1));
  EXPECT_FALSE(found_update1);
  EXPECT_FALSE(executed1);

  bool found_update2 = false;
  bool executed2 = false;
  update_service()->StartUpdateCheck(
      uc2,
      base::BindLambdaForTesting(
          [&found_update2](const std::string& id,
                           const base::Version& version) {
            found_update2 = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed2));
  EXPECT_FALSE(found_update2);
  EXPECT_FALSE(executed2);

  ASSERT_EQ(2, update_client()->num_update_requests());

  {
    const auto& request = update_client()->update_request(0);
    EXPECT_THAT(request.extension_ids, testing::ElementsAre("A"));
  }

  {
    const auto& request = update_client()->update_request(1);
    EXPECT_THAT(request.extension_ids, testing::ElementsAre("A"));
  }

  update_client()->RunDelayedUpdate(0);
  EXPECT_TRUE(found_update1);
  EXPECT_TRUE(executed1);
  EXPECT_FALSE(found_update2);
  EXPECT_FALSE(executed2);

  update_client()->RunDelayedUpdate(1);
  EXPECT_TRUE(found_update2);
  EXPECT_TRUE(executed2);
}

TEST_F(UpdateServiceTest, InProgressUpdate_NonOverlapped) {
  // 2 non-overallped update requests.
  base::HistogramTester histogram_tester;
  update_client()->set_delay_update();
  ExtensionUpdateCheckParams uc1, uc2;

  uc1.update_info["A"] = ExtensionUpdateData();
  uc1.update_info["B"] = ExtensionUpdateData();
  uc1.update_info["C"] = ExtensionUpdateData();

  uc2.update_info["D"] = ExtensionUpdateData();
  uc2.update_info["E"] = ExtensionUpdateData();

  bool found_update1 = false;
  bool executed1 = false;
  update_service()->StartUpdateCheck(
      uc1,
      base::BindLambdaForTesting(
          [&found_update1](const std::string& id,
                           const base::Version& version) {
            found_update1 = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed1));
  EXPECT_FALSE(found_update1);
  EXPECT_FALSE(executed1);

  bool found_update2 = false;
  bool executed2 = false;
  update_service()->StartUpdateCheck(
      uc2,
      base::BindLambdaForTesting(
          [&found_update2](const std::string& id,
                           const base::Version& version) {
            found_update2 = true;
          }),
      base::BindOnce([](bool* executed) { *executed = true; }, &executed2));
  EXPECT_FALSE(found_update2);
  EXPECT_FALSE(executed2);

  ASSERT_EQ(2, update_client()->num_update_requests());
  const auto& request1 = update_client()->update_request(0);
  const auto& request2 = update_client()->update_request(1);

  EXPECT_THAT(request1.extension_ids, testing::ElementsAre("A", "B", "C"));
  EXPECT_THAT(request2.extension_ids, testing::ElementsAre("D", "E"));

  update_client()->RunDelayedUpdate(0);
  EXPECT_TRUE(found_update1);
  EXPECT_TRUE(executed1);
  EXPECT_FALSE(found_update2);
  EXPECT_FALSE(executed2);

  update_client()->RunDelayedUpdate(1);
  EXPECT_TRUE(found_update2);
  EXPECT_TRUE(executed2);
}

}  // namespace

}  // namespace extensions
