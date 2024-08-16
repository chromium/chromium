// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/load_and_localize_file.h"

#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/l10n_file_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace {

void MaybeLocalizeInBackground(
    const ExtensionId& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission,
    std::string* data) {
  bool needs_message_substituion =
      data->find(extensions::MessageBundle::kMessageBegin) != std::string::npos;
  if (!needs_message_substituion) {
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<MessageBundle::SubstitutionMap> localization_messages =
      l10n_file_util::LoadMessageBundleSubstitutionMap(
          extension_path, extension_id, extension_default_locale,
          gzip_permission);

  std::string error;
  MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                       data, &error);
}

// A simple wrapper around MaybeLocalizeInBackground() that returns |data| to
// serve as an adapter for PostTaskAndReply.
std::vector<std::unique_ptr<std::string>>
LocalizeComponentResourcesInBackground(
    std::vector<std::unique_ptr<std::string>> data,
    const ExtensionId& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission) {
  for (auto& resource : data) {
    MaybeLocalizeInBackground(extension_id, extension_path,
                              extension_default_locale, gzip_permission,
                              resource.get());
  }

  return data;
}

// A helper function to load component resources.
std::vector<std::unique_ptr<std::string>> LoadComponentResources(
    const ComponentExtensionResourceManager& resource_manager,
    const std::vector<ExtensionResource>& resources) {
  std::vector<std::unique_ptr<std::string>> data;
  data.reserve(resources.size());
  for (const auto& resource : resources) {
    int resource_id = 0;
    bool is_component_resource = resource_manager.IsComponentExtensionResource(
        resource.extension_root(), resource.relative_path(), &resource_id);
    DCHECK(is_component_resource)
        << "If any resources passed to LoadAndLocalizeResources() "
           "are component resources, they all must be.";
    auto resource_data = std::make_unique<std::string>(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            resource_id));
    data.push_back(std::move(resource_data));
  }

  return data;
}

}  // namespace

void LoadAndLocalizeResources(const Extension& extension,
                              std::vector<ExtensionResource> resources,
                              bool localize_file,
                              size_t max_script_length,
                              LoadAndLocalizeResourcesCallback callback) {
  DCHECK(!resources.empty());
  DCHECK(base::ranges::all_of(resources, [](const ExtensionResource& resource) {
    return !resource.extension_root().empty() &&
           !resource.relative_path().empty();
  }));

  std::string extension_default_locale;
  if (const std::string* temp =
          extension.manifest()->FindStringPath(manifest_keys::kDefaultLocale)) {
    extension_default_locale = *temp;
  }
  auto gzip_permission =
      extension_l10n_util::GetGzippedMessagesPermissionForExtension(&extension);

  // Check whether the resource should be loaded as a component resource (from
  // the resource bundle) or read from disk.
  // We assume (and assert) that if any resource is a component extension
  // resource, they all must be. Read the first resource passed to check if it
  // is a component resource, and treat them all as such if it is.
  const ComponentExtensionResourceManager*
      component_extension_resource_manager =
          ExtensionsBrowserClient::Get()
              ->GetComponentExtensionResourceManager();
  int unused_resource_id = 0;
  bool are_component_resources =
      component_extension_resource_manager &&
      component_extension_resource_manager->IsComponentExtensionResource(
          resources.front().extension_root(), resources.front().relative_path(),
          &unused_resource_id);
  if (are_component_resources) {
    std::vector<std::unique_ptr<std::string>> data = LoadComponentResources(
        *component_extension_resource_manager, resources);
    // Even if no localization is necessary, we post the task asynchronously
    // so that |callback| is not run re-entrantly.
    if (!localize_file) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(data), std::nullopt));
    } else {
      auto callback_adapter =
          [](LoadAndLocalizeResourcesCallback callback,
             std::vector<std::unique_ptr<std::string>> data) {
            std::move(callback).Run(std::move(data), std::nullopt);
          };
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(&LocalizeComponentResourcesInBackground,
                         std::move(data), extension.id(), extension.path(),
                         extension_default_locale, gzip_permission),
          base::BindOnce(callback_adapter, std::move(callback)));
    }

    return;
  }

  // Otherwise, it's not a set of component resources, and we need to load them
  // from disk.

  FileReader::OptionalFileSequenceTask get_file_and_l10n_callback;
  if (localize_file) {
    get_file_and_l10n_callback = base::BindRepeating(
        &MaybeLocalizeInBackground, extension.id(), extension.path(),
        extension_default_locale, gzip_permission);
  }

  auto file_reader = base::MakeRefCounted<FileReader>(
      std::move(resources), max_script_length,
      std::move(get_file_and_l10n_callback), std::move(callback));
  file_reader->Start();
}

}  // namespace extensions
