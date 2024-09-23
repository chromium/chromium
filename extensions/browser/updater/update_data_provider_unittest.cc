// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/updater/extension_installer.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

class UpdateDataProviderExtensionsBrowserClient
    : public TestExtensionsBrowserClient {
 public:
  explicit UpdateDataProviderExtensionsBrowserClient(
      content::BrowserContext* context)
      : TestExtensionsBrowserClient(context) {}

  UpdateDataProviderExtensionsBrowserClient(
      const UpdateDataProviderExtensionsBrowserClient&) = delete;
  UpdateDataProviderExtensionsBrowserClient& operator=(
      const UpdateDataProviderExtensionsBrowserClient&) = delete;

  ~UpdateDataProviderExtensionsBrowserClient() override = default;

  bool IsExtensionEnabled(const std::string& id,
                          content::BrowserContext* context) const override {
    return base::Contains(enabled_ids_, id);
  }

  void AddEnabledExtension(const std::string& id) { enabled_ids_.insert(id); }

 private:
  std::set<std::string> enabled_ids_;
};

class UpdateDataProviderTest : public ExtensionsTest {
 public:
  using UpdateClientCallback = UpdateDataProvider::UpdateClientCallback;

  UpdateDataProviderTest() = default;
  ~UpdateDataProviderTest() override = default;

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
    return base::CreateDirectory(full_path.DirName()) &&
           base::WriteFile(full_path, content);
  }

  void AddExtension(const ExtensionId& extension_id,
                    const std::string& version,
                    bool enabled,
                    int disable_reasons,
                    ManifestLocation location) {
    AddExtension(extension_id, version, "", enabled, disable_reasons, location);
  }

  void AddExtension(const ExtensionId& extension_id,
                    const std::string& version,
                    const std::string& fingerprint,
                    bool enabled,
                    int disable_reasons,
                    ManifestLocation location) {
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
    auto manifest_builder = base::Value::Dict()
                                .Set("name", "My First Extension")
                                .Set("version", version)
                                .Set("manifest_version", 2);
    if (!fingerprint.empty()) {
      manifest_builder.Set("differential_fingerprint", fingerprint);
    }
    builder.SetManifest(std::move(manifest_builder));
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

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, ExtensionUpdateDataMap(), {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));
  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(data[0], std::nullopt);
}

TEST_F(UpdateDataProviderTest, GetData_Fingerprint) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  const std::string fingerprint = "1.0123456789abcdef";
  AddExtension(kExtensionId1, version, true,
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kInternal);
  AddExtension(kExtensionId2, version, fingerprint, true,
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kInternal);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data,
      {kExtensionId1, kExtensionId2},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kInternal);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kExternalPref);

  ExtensionUpdateDataMap update_data;
  auto& info = update_data[kExtensionId1];
  info.is_corrupt_reinstall = true;
  info.install_source = "webstore";

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      true /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kExternalRegistry);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kCommandLine);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      true /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kExternalPolicyDownload);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kExternalPrefDownload);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      true /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kExternalRegistry);
  AddExtension(kExtensionId2, version2, true,
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kUnpacked);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data,
      {kExtensionId1, kExtensionId2},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kInternal);
  AddExtension(kExtensionId2, version2, true,
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kExternalPrefDownload);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      true /*install_immediately*/, update_data, {kExtensionId1, kExtensionId2},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               ManifestLocation::kComponent);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId2] = {};

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data,
      {kExtensionId1, kExtensionId2},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

  ASSERT_EQ(2UL, data.size());
  ASSERT_NE(std::nullopt, data[0]);
  EXPECT_EQ(version, data[0]->version.GetString());
  EXPECT_NE(nullptr, data[0]->installer.get());
  EXPECT_EQ(0UL, data[0]->disabled_reasons.size());
  EXPECT_EQ("other", data[0]->install_location);

  EXPECT_EQ(std::nullopt, data[1]);
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
               ManifestLocation::kExternalComponent);
  AddExtension(kExtensionId2, version2, true,
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kExternalPolicy);

  ExtensionUpdateDataMap update_data;
  auto& info1 = update_data[kExtensionId1];
  auto& info2 = update_data[kExtensionId2];

  info1.install_source = "webstore";
  info2.is_corrupt_reinstall = true;
  info2.install_source = "sideload";

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      true /*install_immediately*/, update_data, {kExtensionId1, kExtensionId2},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

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
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kInternal);

  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};

  std::vector<std::optional<update_client::CrxComponent>> data1;
  data_provider->GetData(
      false /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data1 = output; }));
  ASSERT_EQ(1UL, data1.size());
  ASSERT_NE(nullptr, data1[0]->installer.get());
  const ExtensionInstaller* installer1 =
      static_cast<ExtensionInstaller*>(data1[0]->installer.get());
  EXPECT_FALSE(installer1->install_immediately());

  std::vector<std::optional<update_client::CrxComponent>> data2;
  data_provider->GetData(
      true /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data2 = output; }));
  ASSERT_EQ(1UL, data2.size());
  ASSERT_NE(nullptr, data2[0]->installer.get());
  const ExtensionInstaller* installer2 =
      static_cast<ExtensionInstaller*>(data2[0]->installer.get());
  EXPECT_TRUE(installer2->install_immediately());
}

TEST_F(UpdateDataProviderTest, GetData_Pending_Version) {
  scoped_refptr<UpdateDataProvider> data_provider =
      base::MakeRefCounted<UpdateDataProvider>(browser_context());

  const std::string version = "0.1.2.3";
  const std::string pending_version = "0.1.2.4";
  const std::string pending_fingerprint = "fingerprint";

  AddExtension(kExtensionId1, version, true,
               disable_reason::DisableReason::DISABLE_NONE,
               ManifestLocation::kInternal);

  ExtensionUpdateDataMap update_data;
  update_data[kExtensionId1] = {};
  update_data[kExtensionId1].pending_version = pending_version;
  update_data[kExtensionId1].pending_fingerprint = pending_fingerprint;

  std::vector<std::optional<update_client::CrxComponent>> data;
  data_provider->GetData(
      false /*install_immediately*/, update_data, {kExtensionId1},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  output) { data = output; }));

  ASSERT_EQ(1UL, data.size());
  EXPECT_EQ(pending_version, data[0]->version.GetString());
  EXPECT_EQ(pending_fingerprint, data[0]->fingerprint);
}

}  // namespace

}  // namespace extensions
