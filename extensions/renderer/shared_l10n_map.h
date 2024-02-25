// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SHARED_L10N_MAP_H_
#define EXTENSIONS_RENDERER_SHARED_L10N_MAP_H_

#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace mojom {
class RendererHost;
}

// A helper class to retrieve l10n data for extensions. Since renderers are
// always tied to a specific profile, this class is safe as a singleton (we
// don't need to worry about extensions in other profiles).
//
// This class is thread-safe.
//
// NOTA BENE: ALL METHODS THAT RETRIEVE OR REPLACE MESSAGES MAY RESULT IN A
// SYNC IPC TO THE BROWSER PROCESS AND/OR WAITING FOR A THREAD LOCK.
//
// In practice, the above is not as *quite* bad as it sounds (though still not
// ideal): we cache the retrieved data for extensions, so a maximum of a single
// IPC will be sent per extension per process. Additionally, extensions will
// usually only be accessing this data on a single thread at a time.
class SharedL10nMap {
 public:
  // A map of message name to message.
  using L10nMessagesMap = std::map<std::string, std::string>;

  using IPCTarget = mojom::RendererHost;

  SharedL10nMap();
  SharedL10nMap(const SharedL10nMap&) = delete;
  SharedL10nMap& operator=(const SharedL10nMap&) = delete;
  ~SharedL10nMap() = delete;  // Only the global instance should be used.

  static SharedL10nMap& GetInstance();

  // Retrieves the localized message for the given `extension_id` and
  // `message_name`.
  std::string GetMessage(const ExtensionId& extension_id,
                         const std::string& message_name,
                         IPCTarget* ipc_target);

  // Replaces all messages in `text` with the messages for the given
  // `extension_id`. Returns false if any messages were unmatched.
  bool ReplaceMessages(const ExtensionId& extension_id,
                       std::string* text,
                       IPCTarget* ipc_target);

  // Erases the L10nMessagesMap for the given `id`.
  void EraseMessagesMap(const ExtensionId& extension_id);

  // Sets the L10nMessagesMap for the extension for testing purposes.
  void SetMessagesForTesting(const ExtensionId& id, L10nMessagesMap messages);

 private:
  const L10nMessagesMap* GetMapForExtension(const ExtensionId& extension_id,
                                            IPCTarget* ipc_target);

  using ExtensionToL10nMessagesMap = std::map<ExtensionId, L10nMessagesMap>;
  ExtensionToL10nMessagesMap map_;

  base::Lock lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SHARED_L10N_MAP_H_
