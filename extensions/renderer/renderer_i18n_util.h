// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_I18N_UTIL_H_
#define EXTENSIONS_RENDERER_RENDERER_I18N_UTIL_H_

#include <map>
#include <string>

#include "extensions/common/extension_id.h"

namespace IPC {
class Sender;
}

namespace extensions::i18n_util {

// A map of message name to message.
using L10nMessagesMap = std::map<std::string, std::string>;

// Retrieves the L10nMessagesMap associated with the given `extension_id`. This
// may trigger a (sync!) IPC to the browser in order to fetch the map if it
// hasn't been initialized.
const L10nMessagesMap* GetRendererMessagesMap(const ExtensionId& extension_id,
                                              IPC::Sender* message_sender);

// Erases the L10nMessagesMap for the given `id`.
void EraseRendererMessagesMap(const ExtensionId& id);

// Sets the L10nMessagesMap for the extension for testing purposes.
void SetMessagesMapForTesting(const ExtensionId& id, L10nMessagesMap map);

}  // namespace extensions::i18n_util

#endif  // EXTENSIONS_RENDERER_RENDERER_I18N_UTIL_H_
