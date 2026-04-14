// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/hash.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pending_extension_info.h"
#include "extensions/browser/pending_extension_manager.h"
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

update_client::CrxComponent FromExtension(const Extension& extension,
                                          const ExtensionPrefs& extension_prefs,
                                          bool allow_dev,
                                          bool enabled) {
  update_client::CrxComponent crx_component;
  if (auto pubkey = base::Base64Decode(extension.public_key())) {
    crx_component.pk_hash = base::ToVector(crypto::hash::Sha256(*pubkey));
  }
  crx_component.version = extension.version();
  crx_component.install_location =
      ManifestFetchData::GetSimpleLocationString(extension.location());
  crx_component.crx_format_requirement =
      extension.from_webstore() ? GetWebstoreVerifierFormat(allow_dev)
                                : GetPolicyVerifierFormat();
  if (!enabled) {
    DisableReasonSet disable_reasons =
        extension_prefs.GetDisableReasons(extension.id());
    if (disable_reasons.empty() ||
        disable_reasons.contains(disable_reason::DISABLE_UNKNOWN)) {
      // DISABLE_UNKNOWN is transcoded to 0.
      crx_component.disabled_reasons.push_back(0);
    }
    disable_reasons.erase(disable_reason::DISABLE_UNKNOWN);
    for (int reason : disable_reasons) {
      crx_component.disabled_reasons.push_back(reason);
    }
  }
  return crx_component;
}

update_client::CrxComponent FromPendingExtensionInfo(
    const PendingExtensionInfo& pending_info,
    bool allow_dev) {
  update_client::CrxComponent crx_component;
  crx_component.version = pending_info.version().IsValid()
                              ? pending_info.version()
                              : base::Version("0.0.0.0");
  crx_component.install_location =
      ManifestFetchData::GetSimpleLocationString(pending_info.install_source());
  const bool from_webstore =
      pending_info.update_url().is_empty() ||
      extension_urls::IsWebstoreUpdateUrl(pending_info.update_url());
  crx_component.crx_format_requirement =
      from_webstore ? GetWebstoreVerifierFormat(allow_dev)
                    : GetPolicyVerifierFormat();
  return crx_component;
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
  const PendingExtensionManager* pending_manager =
      PendingExtensionManager::Get(browser_context_);
  bool allow_dev = extension_urls::GetWebstoreUpdateUrl() !=
                   extension_urls::GetDefaultWebstoreUpdateUrl();
  CHECK(extension_prefs);

  for (const auto& id : ids) {
    // Initialize update_client::CrxComponents from the appropriate source.
    if (const Extension* extension = registry->GetInstalledExtension(id)) {
      data.push_back(
          FromExtension(*extension, *extension_prefs, allow_dev,
                        ExtensionsBrowserClient::Get()->IsExtensionEnabled(
                            id, browser_context_)));
    } else if (const PendingExtensionInfo* pending_info =
                   pending_manager->GetById(id)) {
      data.push_back(FromPendingExtensionInfo(*pending_info, allow_dev));
    } else {
      // Extension no longer installed nor pending: abandon the update attempt.
      data.push_back(std::nullopt);
      continue;
    }

    CHECK_NE(0u, update_crx_component.count(id));

    // Fill out common data.
    auto& crx_component = data.back();
    crx_component->app_id = id;
    crx_component->requires_network_encryption = !allow_dev;
    crx_component->installer = base::MakeRefCounted<ExtensionInstaller>(
        id, install_immediately,
        base::BindRepeating(&UpdateDataProvider::RunInstallCallback, this));

    // Overwrite some fields with data captured at the start of the update
    // operation.
    const ExtensionUpdateData& extension_data = update_crx_component.at(id);
    if (extension_data.pending_version) {
      crx_component->version = base::Version(*extension_data.pending_version);
    }

    if (extension_data.is_corrupt_reinstall) {
      crx_component->install_source = "reinstall";
      crx_component->version = base::Version("0.0.0.0");
    } else {
      crx_component->install_source = extension_data.install_source;
    }
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
