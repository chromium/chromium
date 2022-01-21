// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_I18N_UTIL_H_
#define EXTENSIONS_RENDERER_RENDERER_I18N_UTIL_H_

#include "extensions/common/extension_id.h"
#include "extensions/common/message_bundle.h"

namespace IPC {
class Sender;
}

namespace extensions::i18n_util {

// Retrieves the L10nMessagesMap associated with the given `extension_id`. This
// may trigger a (sync!) IPC to the browser in order to fetch the map if it
// hasn't been initialized.
const L10nMessagesMap* GetRendererMessagesMap(const ExtensionId& extension_id,
                                              IPC::Sender* message_sender);

}  // namespace extensions::i18n_util

#endif  // EXTENSIONS_RENDERER_RENDERER_I18N_UTIL_H_
