// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/shared_l10n_map.h"

#include "base/no_destructor.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_sender.h"

namespace extensions {

SharedL10nMap::SharedL10nMap() = default;

// static
SharedL10nMap& SharedL10nMap::GetInstance() {
  static base::NoDestructor<SharedL10nMap> g_map;
  return *g_map;
}

std::string SharedL10nMap::GetMessage(const ExtensionId& extension_id,
                                      const std::string& message_name,
                                      IPC::Sender* message_sender) {
  base::AutoLock auto_lock(lock_);

  const L10nMessagesMap* extension_map =
      GetMapForExtension(extension_id, message_sender);
  if (!extension_map)
    return std::string();

  return MessageBundle::GetL10nMessage(message_name, *extension_map);
}

bool SharedL10nMap::ReplaceMessages(const ExtensionId& extension_id,
                                    std::string* text,
                                    IPC::Sender* message_sender) {
  base::AutoLock auto_lock(lock_);

  const L10nMessagesMap* extension_map =
      GetMapForExtension(extension_id, message_sender);
  if (!extension_map)
    return false;

  std::string error_unused;
  return MessageBundle::ReplaceMessagesWithExternalDictionary(
      *extension_map, text, &error_unused);
}

void SharedL10nMap::EraseMessagesMap(const ExtensionId& id) {
  base::AutoLock auto_lock(lock_);
  map_.erase(id);
}

void SharedL10nMap::SetMessagesForTesting(const ExtensionId& id,
                                          L10nMessagesMap messages) {
  base::AutoLock auto_lock(lock_);
  map_[id] = std::move(messages);
}

const SharedL10nMap::L10nMessagesMap* SharedL10nMap::GetMapForExtension(
    const ExtensionId& extension_id,
    IPC::Sender* message_sender) {
  lock_.AssertAcquired();

  auto iter = map_.find(extension_id);
  if (iter != map_.end())
    return &iter->second;

  if (!message_sender)
    return nullptr;

  L10nMessagesMap& l10n_messages = map_[extension_id];

  // A sync call to load message catalogs for current extension.
  // TODO(devlin): Wait, what?! A synchronous call to the browser to perform
  // potentially blocking work reading files from disk? That's Bad.
  message_sender->Send(
      new ExtensionHostMsg_GetMessageBundle(extension_id, &l10n_messages));

  return &l10n_messages;
}

}  // namespace extensions
