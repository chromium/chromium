// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_i18n_util.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_sender.h"

namespace extensions::i18n_util {

namespace {

using ExtensionToL10nMessagesMap = std::map<ExtensionId, L10nMessagesMap>;

ExtensionToL10nMessagesMap& GetExtensionToL10nMessagesMap() {
  static base::NoDestructor<ExtensionToL10nMessagesMap> map;
  return *map;
}

}  // namespace

const L10nMessagesMap* GetRendererMessagesMap(const ExtensionId& extension_id,
                                              IPC::Sender* message_sender) {
  ExtensionToL10nMessagesMap& messages_map = GetExtensionToL10nMessagesMap();
  auto iter = messages_map.find(extension_id);
  if (iter != messages_map.end())
    return &iter->second;

  if (!message_sender)
    return nullptr;

  L10nMessagesMap& l10n_messages = messages_map[extension_id];

  // A sync call to load message catalogs for current extension.
  // TODO(devlin): Wait, what?! A synchronous call to the browser to perform
  // potentially blocking work reading files from disk? That's Bad.
  {
    SCOPED_UMA_HISTOGRAM_TIMER("Extensions.SyncGetMessageBundle");
    message_sender->Send(
        new ExtensionHostMsg_GetMessageBundle(extension_id, &l10n_messages));
  }
  // In practice, the messages map is never empty, because it contains at least
  // the @@extension_id value. But this doesn't hold in renderer unit tests.
  // DCHECK(!l10n_messages->empty());

  return &l10n_messages;
}

void EraseRendererMessagesMap(const ExtensionId& id) {
  GetExtensionToL10nMessagesMap().erase(id);
}

void SetMessagesMapForTesting(const ExtensionId& id, L10nMessagesMap map) {
  GetExtensionToL10nMessagesMap()[id] = std::move(map);
}

}  // namespace extensions::i18n_util
