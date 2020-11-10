// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/load_and_localize_file.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
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
  if (!needs_message_substituion)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<MessageBundle::SubstitutionMap> localization_messages(
      file_util::LoadMessageBundleSubstitutionMap(extension_path, extension_id,
                                                  extension_default_locale,
                                                  gzip_permission));

  std::string error;
  MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                       data, &error);
}

// A simple wrapper around MaybeLocalizeInBackground() that returns |data| to
// serve as an adapter for PostTaskAndReply.
std::unique_ptr<std::string> LocalizeComponentResourceInBackground(
    std::unique_ptr<std::string> data,
    const ExtensionId& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission) {
  MaybeLocalizeInBackground(extension_id, extension_path,
                            extension_default_locale, gzip_permission,
                            data.get());

  return data;
}

}  // namespace

void LoadAndLocalizeResource(const Extension& extension,
                             const ExtensionResource& resource,
                             bool localize_file,
                             LoadAndLocalizeResourceCallback callback) {
  DCHECK(!resource.extension_root().empty());
  DCHECK(!resource.relative_path().empty());

  std::string extension_default_locale;
  extension.manifest()->GetString(manifest_keys::kDefaultLocale,
                                  &extension_default_locale);
  auto gzip_permission =
      extension_l10n_util::GetGzippedMessagesPermissionForExtension(&extension);

  // Check whether the resource should be loaded as a component resource (from
  // the resource bundle) or read from disk.
  int resource_id = 0;
  const ComponentExtensionResourceManager*
      component_extension_resource_manager =
          ExtensionsBrowserClient::Get()
              ->GetComponentExtensionResourceManager();
  if (component_extension_resource_manager &&
      component_extension_resource_manager->IsComponentExtensionResource(
          resource.extension_root(), resource.relative_path(), &resource_id)) {
    auto data = std::make_unique<std::string>(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            resource_id));

    // We assume this call always succeeds.
    constexpr bool kSuccess = true;
    if (!localize_file) {
      // Even if no localization is necessary, we post the task asynchronously
      // so that |callback| is not run re-entrantly.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), kSuccess, std::move(data)));
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(&LocalizeComponentResourceInBackground,
                         std::move(data), extension.id(), extension.path(),
                         extension_default_locale, gzip_permission),
          base::BindOnce(std::move(callback), kSuccess));
    }
  } else {
    FileReader::OptionalFileSequenceTask get_file_and_l10n_callback;
    if (localize_file) {
      get_file_and_l10n_callback = base::BindOnce(
          &MaybeLocalizeInBackground, extension.id(), extension.path(),
          extension_default_locale, gzip_permission);
    }

    auto file_reader = base::MakeRefCounted<FileReader>(
        resource, std::move(get_file_and_l10n_callback), std::move(callback));
    file_reader->Start();
  }
}

}  // namespace extensions
