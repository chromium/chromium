// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/verifier_formats.h"

namespace extensions {

namespace {

using UpdateClientCallback = UpdateDataProvider::UpdateClientCallback;

void PostErrorTasks(const base::FilePath& unpacked_dir,
                    UpdateClientCallback update_client_callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeletePathRecursivelyCallback(unpacked_dir));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(update_client_callback),
                     update_client::CrxInstaller::Result(
                         update_client::InstallError::GENERIC_ERROR)));
}

}  // namespace

UpdateDataProvider::UpdateDataProvider(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

UpdateDataProvider::~UpdateDataProvider() = default;

void UpdateDataProvider::Shutdown() {
  browser_context_ = nullptr;
}

void UpdateDataProvider::GetData(
    bool install_immediately,
    const ExtensionUpdateDataMap& update_crx_component,
    const std::vector<std::string>& ids,
    base::OnceCallback<
        void(const std::vector<std::optional<update_client::CrxComponent>>&)>
        callback) {
  std::vector<std::optional<update_client::CrxComponent>> data;
  if (!browser_context_) {
    for (size_t i = 0; i < ids.size(); i++) {
      data.push_back(std::nullopt);
    }
    std::move(callback).Run(data);
    return;
  }
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser_context_);
  for (const auto& id : ids) {
    const Extension* extension = registry->GetInstalledExtension(id);
    data.push_back(extension ? std::make_optional<update_client::CrxComponent>()
                             : std::nullopt);
    if (!extension) {
      continue;
    }
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
      crx_component->version =
          extension_data.pending_version
              ? base::Version(*extension_data.pending_version)
              : extension->version();
      crx_component->fingerprint = extension_data.pending_fingerprint.value_or(
          extension->DifferentialFingerprint());
    }
    bool allow_dev = extension_urls::GetWebstoreUpdateUrl() !=
                     extension_urls::GetDefaultWebstoreUpdateUrl();
    crx_component->requires_network_encryption = !allow_dev;
    crx_component->crx_format_requirement =
        extension->from_webstore() ? GetWebstoreVerifierFormat(allow_dev)
                                   : GetPolicyVerifierFormat();
    crx_component->installer = base::MakeRefCounted<ExtensionInstaller>(
        id, extension->path(), install_immediately,
        base::BindRepeating(&UpdateDataProvider::RunInstallCallback, this));
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
        if (disabled_reasons & enum_value) {
          crx_component->disabled_reasons.push_back(enum_value);
        }
      }
    }
    crx_component->install_source = extension_data.is_corrupt_reinstall
                                        ? "reinstall"
                                        : extension_data.install_source;
    crx_component->install_location =
        ManifestFetchData::GetSimpleLocationString(extension->location());
  }
  std::move(callback).Run(data);
}

void UpdateDataProvider::RunInstallCallback(
    const ExtensionId& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    bool install_immediately,
    UpdateClientCallback update_client_callback) {
  VLOG(3) << "UpdateDataProvider::RunInstallCallback " << extension_id << " "
          << public_key;

  if (!browser_context_) {
    PostErrorTasks(unpacked_dir, std::move(update_client_callback));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateDataProvider::InstallUpdateCallback, this,
                     extension_id, public_key, unpacked_dir,
                     install_immediately, std::move(update_client_callback)));
}

void UpdateDataProvider::InstallUpdateCallback(
    const ExtensionId& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    bool install_immediately,
    UpdateClientCallback update_client_callback) {
  if (!browser_context_) {
    PostErrorTasks(unpacked_dir, std::move(update_client_callback));
    return;
  }

  // Error codes are converted into integers and may collide with codes from
  // other embedders. However, for any given extension ID, there should be only
  // one embedder, so the server should be able to figure it out.
  ExtensionSystem::Get(browser_context_)
      ->InstallUpdate(
          extension_id, public_key, unpacked_dir, install_immediately,
          base::BindOnce(
              [](UpdateClientCallback callback,
                 const std::optional<CrxInstallError>& error) {
                DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
                update_client::CrxInstaller::Result result(0);
                if (error.has_value()) {
                  int detail =
                      error->type() ==
                              CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE
                          ? static_cast<int>(error->sandbox_failure_detail())
                          : static_cast<int>(error->detail());
                  result = update_client::CrxInstaller::Result(
                      static_cast<int>(error->type()), detail);
                }
                std::move(callback).Run(result);
              },
              std::move(update_client_callback)));
}

}  // namespace extensions
