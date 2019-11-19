// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/verifier_formats.h"

namespace extensions {

namespace {

using UpdateClientCallback = UpdateDataProvider::UpdateClientCallback;

void InstallUpdateCallback(content::BrowserContext* context,
                           const std::string& extension_id,
                           const std::string& public_key,
                           const base::FilePath& unpacked_dir,
                           bool install_immediately,
                           UpdateClientCallback update_client_callback) {
  // Note that error codes are converted into custom error codes, which are all
  // based on a constant (see ToInstallerResult). This means that custom codes
  // from different embedders may collide. However, for any given extension ID,
  // there should be only one embedder, so this should be OK from Omaha.
  ExtensionSystem::Get(context)->InstallUpdate(
      extension_id, public_key, unpacked_dir, install_immediately,
      base::BindOnce(
          [](UpdateClientCallback callback,
             const base::Optional<CrxInstallError>& error) {
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            update_client::CrxInstaller::Result result(0);
            if (error.has_value()) {
              int detail =
                  error->type() ==
                          CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE
                      ? static_cast<int>(error->sandbox_failure_detail())
                      : static_cast<int>(error->detail());
              result = update_client::ToInstallerResult(error->type(), detail);
            }
            std::move(callback).Run(result);
          },
          std::move(update_client_callback)));
}

}  // namespace

UpdateDataProvider::UpdateDataProvider(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

UpdateDataProvider::~UpdateDataProvider() {}

void UpdateDataProvider::Shutdown() {
  browser_context_ = nullptr;
}

std::vector<base::Optional<update_client::CrxComponent>>
UpdateDataProvider::GetData(bool install_immediately,
                            const ExtensionUpdateDataMap& update_crx_component,
                            const std::vector<std::string>& ids) {
  std::vector<base::Optional<update_client::CrxComponent>> data;
  if (!browser_context_)
    return data;
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser_context_);
  for (const auto& id : ids) {
    const Extension* extension = registry->GetInstalledExtension(id);
    data.push_back(extension
                       ? base::make_optional<update_client::CrxComponent>()
                       : base::nullopt);
    if (!extension)
      continue;
    DCHECK_NE(0u, update_crx_component.count(id));
    const ExtensionUpdateData& extension_data = update_crx_component.at(id);
    auto& crx_component = data.back();
    std::string pubkey_bytes;
    base::Base64Decode(extension->public_key(), &pubkey_bytes);
    crx_component->pk_hash.resize(crypto::kSHA256Length, 0);
    crypto::SHA256HashString(pubkey_bytes, crx_component->pk_hash.data(),
                             crx_component->pk_hash.size());
    crx_component->app_id =
        update_client::GetCrxIdFromPublicKeyHash(crx_component->pk_hash);
    if (extension_data.is_corrupt_reinstall) {
      crx_component->version = base::Version("0.0.0.0");
    } else {
      crx_component->version = extension->version();
      crx_component->fingerprint = extension->DifferentialFingerprint();
    }
    crx_component->allows_background_download = false;
    crx_component->requires_network_encryption = true;
    crx_component->crx_format_requirement =
        extension->from_webstore() ? GetWebstoreVerifierFormat(false)
                                   : GetPolicyVerifierFormat();
    crx_component->installer = base::MakeRefCounted<ExtensionInstaller>(
        id, extension->path(), install_immediately,
        base::BindOnce(&UpdateDataProvider::RunInstallCallback, this));
    if (!ExtensionsBrowserClient::Get()->IsExtensionEnabled(id,
                                                            browser_context_)) {
      int disabled_reasons = extension_prefs->GetDisableReasons(id);
      if (disabled_reasons == extensions::disable_reason::DISABLE_NONE ||
          disabled_reasons >= extensions::disable_reason::DISABLE_REASON_LAST) {
        crx_component->disabled_reasons.push_back(0);
      }
      for (int enum_value = 1;
           enum_value < extensions::disable_reason::DISABLE_REASON_LAST;
           enum_value <<= 1) {
        if (disabled_reasons & enum_value)
          crx_component->disabled_reasons.push_back(enum_value);
      }
    }
    crx_component->install_source = extension_data.is_corrupt_reinstall
                                        ? "reinstall"
                                        : extension_data.install_source;
    crx_component->install_location =
        ManifestFetchData::GetSimpleLocationString(extension->location());
  }
  return data;
}

void UpdateDataProvider::RunInstallCallback(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    bool install_immediately,
    UpdateClientCallback update_client_callback) {
  VLOG(3) << "UpdateDataProvider::RunInstallCallback " << extension_id << " "
          << public_key;

  if (!browser_context_) {
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), unpacked_dir,
                       true));
    return;
  }

  base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(InstallUpdateCallback, browser_context_, extension_id,
                         public_key, unpacked_dir, install_immediately,
                         std::move(update_client_callback)));
}

}  // namespace extensions
