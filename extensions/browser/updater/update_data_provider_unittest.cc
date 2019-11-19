// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/updater/extension_installer.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

class UpdateDataProviderExtensionsBrowserClient
    : public TestExtensionsBrowserClient {
 public:
  explicit UpdateDataProviderExtensionsBrowserClient(
      content::BrowserContext* context)
      : TestExtensionsBrowserClient(context) {}
  ~UpdateDataProviderExtensionsBrowserClient() override {}

  bool IsExtensionEnabled(const std::string& id,
                          content::BrowserContext* context) const override {
    return enabled_ids_.find(id) != enabled_ids_.end();
  }

  void AddEnabledExtension(const std::string& id) { enabled_ids_.insert(id); }

 private:
  std::set<std::string> enabled_ids_;

  DISALLOW_COPY_AND_ASSIGN(UpdateDataProviderExtensionsBrowserClient);
};

class UpdateDataProviderTest : public ExtensionsTest {
 public:
  using UpdateClientCallback = UpdateDataProvider::UpdateClientCallback;

  UpdateDataProviderTest() {}
  ~UpdateDataProviderTest() override {}

  void SetUp() override {
    SetExtensionsBrowserClient(
        std::make_unique<UpdateDataProviderExtensionsBrowserClient>(
            browser_context()));
    ExtensionsTest::SetUp();
  }

 protected:
  ExtensionSystem* extension_system() {
    return ExtensionSystem::Get(browser_context());
  }

  ExtensionRegistry* extension_registry() {
    return ExtensionRegistry::Get(browser_context());
  }

  // Helper function that creates a file at |relative_path| within |directory|
  // and fills it with |content|.
  bool AddFileToDirectory(const base::FilePath& directory,
                          const base::FilePath& relative_path,
                          const std::string& content) const {
    const base::FilePath full_path = directory.Append(relative_path);
    if (!base::CreateDirectory(full_path.DirName()))
      return false;
    int result = base::WriteFile(full_path, content.data(), content.size());
    return (static_cast<size_t>(result) == content.size());
  }

  void AddExtension(const std::string& extension_id,
                    const std::string& version,
                    bool enabled,
                    int disable_reasons,
                    Manifest::Location location) {
    AddExtension(extension_id, version, "", enabled, disable_reasons, location);
  }

  void AddExtension(const std::string& extension_id,
                    const std::string& version,
                    const std::string& fingerprint,
                    bool enabled,
                    int disable_reasons,
                    Manifest::Location location) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathExists(temp_dir.GetPath()));

    base::FilePath foo_js(FILE_PATH_LITERAL("foo.js"));
    base::FilePath bar_html(FILE_PATH_LITERAL("bar/bar.html"));
    ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), foo_js, "hello"))
        << "Failed to write " << temp_dir.GetPath().value() << "/"
        << foo_js.value();
    ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), bar_html, "world"));

    ExtensionBuilder builder;
    DictionaryBuilder manifest_builder;
    manifest_builder.Set("name", "My First Extension")
        .Set("version", version)
        .Set("manifest_version", 2);
    if (!fingerprint.empty())
      manifest_builder.Set("differential_fingerprint", fingerprint);
    builder.SetManifest(manifest_builder.Build());
    builder.SetID(extension_id);
    builder.SetPath(temp_dir.GetPath());
    builder.SetLocation(location);

    auto* test_browser_client =
        static_cast<UpdateDataProviderExtensionsBrowserClient*>(
            extensions_browser_client());
    if (enabled) {
      extension_registry()->AddEnabled(builder.Build());
      test_browser_client->AddEnabledExtension(extension_id);
    } else {
      extension_registry()->AddDisabled(builder.Build());
      ExtensionPrefs::Get(browser_context())
          ->AddDisableReasons(extension_id, disable_reasons);
    }

    const Extension* extension =
        extension_registry()->GetInstalledExtension(extension_id);
    ASSERT_NE(nullptr, extension);
    ASSERT_EQ(version, extension->VersionString());
  }

  const std::string kExtensionId1 = "adbncddmehfkgipkidpdiheffobcpfma";
  const std::string kExtensionId2 = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
};

TEST_F(UpdateDataProviderTest, GetData_NoDataAdded) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(nullptr);

  const auto data = data_provider->GetData(
      false /*install_immediately*/, ExtensionUpdateDataMap(), {kExtensionId1});
  EXPECT_EQ(0UL, data.size());
}

TEST_F(UpdateDataProviderTest, GetData_Fingerprint) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  const std::string fingerprint = "1.0123456789abcdef";
  AddExtension(kExtensionId1, version, true,
               disable_reason::DisableReason::DISABLE_NONE, Manifest::INTERNAL);
  AddExtension(kExtensionId2, version, fingerprint, true,
               disable_reason::DisableReason::DISABLE_NONE, Manifest::INTERNAL);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  const auto data =
      data_provider->GetData(false /*install_immediately*/, update_data,
                             {kExtensionId1, kExtensionId2});

  ASSERT_EQ(2UL, data.size());
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_EQ(version, data[1]->version.GetString());
  EXPECT_EQ("2." + version, data[0]->fingerprint);
  EXPECT_EQ(fingerprint, data[1]->fingerprint);
}

TEST_F(UpdateDataProviderTest, GetData_EnabledExtension) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, true,
               disable_reason::DisableReason::DISABLE_NONE, Manifest::INTERNAL);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  const auto data = data_provider->GetData(false /*install_immediately*/,
                                           update_data, {kExtensionId1});

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  EXPECT_EQ(0UL, data[0]->disabled_reasons.size());
  EXPECT_EQ("internal", data[0]->install_location);
}

TEST_F(UpdateDataProviderTest, GetData_EnabledExtensionWithData) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, true,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::EXTERNAL_PREF);

  ExtensionUpdateDataMap update_data;
  auto& info = update_data[kExtensionId1];
  info.is_corrupt_reinstall = true;
  info.install_source = "webstore";

  const auto data = data_provider->GetData(true /*install_immediately*/,
                                           update_data, {kExtensionId1});

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ("0.0.0.0", data[0]->version.GetString());
  EXPECT_EQ("reinstall", data[0]->install_source);
  EXPECT_EQ("external", data[0]->install_location);
  EXPECT_NE(nullptr, data[0]->installer.get());
  EXPECT_EQ(0UL, data[0]->disabled_reasons.size());
}

TEST_F(UpdateDataProviderTest, GetData_DisabledExtension_WithNoReason) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, false,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::EXTERNAL_REGISTRY);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  const auto data = data_provider->GetData(false /*install_immediately*/,
                                           update_data, {kExtensionId1});

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  ASSERT_EQ(1UL, data[0]->disabled_reasons.size());
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_NONE,
            data[0]->disabled_reasons[0]);
  EXPECT_EQ("external", data[0]->install_location);
}

TEST_F(UpdateDataProviderTest, GetData_DisabledExtension_UnknownReason) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, false,
               disable_reason::DisableReason::DISABLE_REASON_LAST,
               Manifest::COMMAND_LINE);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  const auto data = data_provider->GetData(true /*install_immediately*/,
                                           update_data, {kExtensionId1});

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  ASSERT_EQ(1UL, data[0]->disabled_reasons.size());
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_NONE,
            data[0]->disabled_reasons[0]);
  EXPECT_EQ("other", data[0]->install_location);
}

TEST_F(UpdateDataProviderTest, GetData_DisabledExtension_WithReasons) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, false,
               disable_reason::DisableReason::DISABLE_USER_ACTION |
                   disable_reason::DisableReason::DISABLE_CORRUPTED,
               Manifest::EXTERNAL_POLICY_DOWNLOAD);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  const auto data = data_provider->GetData(false /*install_immediately*/,
                                           update_data, {kExtensionId1});

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  ASSERT_EQ(2UL, data[0]->disabled_reasons.size());
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_USER_ACTION,
            data[0]->disabled_reasons[0]);
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_CORRUPTED,
            data[0]->disabled_reasons[1]);
  EXPECT_EQ("policy", data[0]->install_location);
}

TEST_F(UpdateDataProviderTest,
       GetData_DisabledExtension_WithReasonsAndUnknownReason) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, false,
               disable_reason::DisableReason::DISABLE_USER_ACTION |
                   disable_reason::DisableReason::DISABLE_CORRUPTED |
                   disable_reason::DisableReason::DISABLE_REASON_LAST,
               Manifest::EXTERNAL_PREF_DOWNLOAD);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  const auto data = data_provider->GetData(true /*install_immediately*/,
                                           update_data, {kExtensionId1});

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  ASSERT_EQ(3UL, data[0]->disabled_reasons.size());
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_NONE,
            data[0]->disabled_reasons[0]);
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_USER_ACTION,
            data[0]->disabled_reasons[1]);
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_CORRUPTED,
            data[0]->disabled_reasons[2]);
  EXPECT_EQ("external", data[0]->install_location);
}

TEST_F(UpdateDataProviderTest, GetData_MultipleExtensions) {
  // GetData with more than 1 extension.
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version1 = "0.1.2.3";
  const std::string version2 = "9.8.7.6";
  AddExtension(kExtensionId1, version1, true,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::EXTERNAL_REGISTRY);
  AddExtension(kExtensionId2, version2, true,
               disable_reason::DisableReason::DISABLE_NONE, Manifest::UNPACKED);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  const auto data =
      data_provider->GetData(false /*install_immediately*/, update_data,
                             {kExtensionId1, kExtensionId2});

  ASSERT_EQ(2UL, data.size());
  EXPECT_EQ(version1, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  EXPECT_EQ(0UL, data[0]->disabled_reasons.size());
  EXPECT_EQ("external", data[0]->install_location);
  EXPECT_EQ(version2, data[1]->version.GetString());
  EXPECT_NE(nullptr, data[1]->installer.get());
  EXPECT_EQ(0UL, data[1]->disabled_reasons.size());
  EXPECT_EQ("other", data[1]->install_location);
}

TEST_F(UpdateDataProviderTest, GetData_MultipleExtensions_DisabledExtension) {
  // One extension is disabled.
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version1 = "0.1.2.3";
  const std::string version2 = "9.8.7.6";
  AddExtension(kExtensionId1, version1, false,
               disable_reason::DisableReason::DISABLE_CORRUPTED,
               Manifest::INTERNAL);
  AddExtension(kExtensionId2, version2, true,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::EXTERNAL_PREF_DOWNLOAD);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  const auto data =
      data_provider->GetData(true /*install_immediately*/, update_data,
                             {kExtensionId1, kExtensionId2});

  ASSERT_EQ(2UL, data.size());
  EXPECT_EQ(version1, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  ASSERT_EQ(1UL, data[0]->disabled_reasons.size());
  EXPECT_EQ(disable_reason::DisableReason::DISABLE_CORRUPTED,
            data[0]->disabled_reasons[0]);
  EXPECT_EQ("internal", data[0]->install_location);

  EXPECT_EQ(version2, data[1]->version.GetString());
  EXPECT_NE(nullptr, data[1]->installer.get());
  EXPECT_EQ(0UL, data[1]->disabled_reasons.size());
  EXPECT_EQ("external", data[1]->install_location);
}

TEST_F(UpdateDataProviderTest,
       GetData_MultipleExtensions_NotInstalledExtension) {
  // One extension is not installed.
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  AddExtension(kExtensionId1, version, true,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::COMPONENT);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  const auto data =
      data_provider->GetData(false /*install_immediately*/, update_data,
                             {kExtensionId1, kExtensionId2});

  ASSERT_EQ(2UL, data.size());
  ASSERT_NE(base::nullopt, data[0]);
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  EXPECT_EQ(0UL, data[0]->disabled_reasons.size());
  EXPECT_EQ("other", data[0]->install_location);

  EXPECT_EQ(base::nullopt, data[1]);
}

TEST_F(UpdateDataProviderTest, GetData_MultipleExtensions_CorruptExtension) {
  // With non-default data, one extension is corrupted:
  // is_corrupt_reinstall=true.
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version1 = "0.1.2.3";
  const std::string version2 = "9.8.7.6";
  const std::string initial_version = "0.0.0.0";
  AddExtension(kExtensionId1, version1, true,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::EXTERNAL_COMPONENT);
  AddExtension(kExtensionId2, version2, true,
               disable_reason::DisableReason::DISABLE_NONE,
               Manifest::EXTERNAL_POLICY);

  ExtensionUpdateDataMap update_data;
  auto& info1 = update_data[kExtensionId1];
  auto& info2 = update_data[kExtensionId2];

  info1.install_source = "webstore";
  info2.is_corrupt_reinstall = true;
  info2.install_source = "sideload";

  const auto data =
      data_provider->GetData(true /*install_immediately*/, update_data,
                             {kExtensionId1, kExtensionId2});

  ASSERT_EQ(2UL, data.size());
  EXPECT_EQ(version1, data[0]->version.GetString());
  EXPECT_EQ("webstore", data[0]->install_source);
  EXPECT_EQ("other", data[0]->install_location);
  EXPECT_NE(nullptr, data[0]->installer.get());
  EXPECT_EQ(0UL, data[0]->disabled_reasons.size());
  EXPECT_EQ(initial_version, data[1]->version.GetString());
  EXPECT_EQ("reinstall", data[1]->install_source);
  EXPECT_EQ("policy", data[1]->install_location);
  EXPECT_NE(nullptr, data[1]->installer.get());
  EXPECT_EQ(0UL, data[1]->disabled_reasons.size());
}

TEST_F(UpdateDataProviderTest, GetData_InstallImmediately) {
  // Verify that GetData propagtes install_immediately correctly to the crx
  // installer.
  AddExtension(kExtensionId1, "0.1.1.3", true,
               disable_reason::DisableReason::DISABLE_NONE, Manifest::INTERNAL);

  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  const auto data1 = data_provider->GetData(false /*install_immediately*/,
                                            update_data, {kExtensionId1});
  ASSERT_EQ(1UL, data1.size());
  ASSERT_NE(nullptr, data1[0]->installer.get());
  const ExtensionInstaller* installer1 =
      static_cast<ExtensionInstaller*>(data1[0]->installer.get());
  EXPECT_FALSE(installer1->install_immediately());

  const auto data2 = data_provider->GetData(true /*install_immediately*/,
                                            update_data, {kExtensionId1});
  ASSERT_EQ(1UL, data2.size());
  ASSERT_NE(nullptr, data2[0]->installer.get());
  const ExtensionInstaller* installer2 =
      static_cast<ExtensionInstaller*>(data2[0]->installer.get());
  EXPECT_TRUE(installer2->install_immediately());
}

}  // namespace

}  // namespace extensions
