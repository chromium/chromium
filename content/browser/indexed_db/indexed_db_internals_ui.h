// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace blink {
class StorageKey;
}

namespace download {
class DownloadItem;
}

namespace content {

// The implementation for the chrome://indexeddb-internals page.
class IndexedDBInternalsUI : public WebUIController {
 public:
  explicit IndexedDBInternalsUI(WebUI* web_ui);
  ~IndexedDBInternalsUI() override;

 private:
  base::WeakPtrFactory<IndexedDBInternalsUI> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(IndexedDBInternalsUI);
};

class IndexedDBInternalsHandler : public WebUIMessageHandler {
 public:
  IndexedDBInternalsHandler();
  ~IndexedDBInternalsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  void GetAllStorageKeys(const base::ListValue* args);
  void OnStorageKeysReady(const base::Value& storage_keys,
                          const base::FilePath& path);

  void DownloadStorageKeyData(const base::ListValue* args);
  void OnDownloadDataReady(const std::string& callback_id,
                           uint64_t connection_count,
                           bool success,
                           const base::FilePath& temp_path,
                           const base::FilePath& zip_path);
  void OnDownloadStarted(const base::FilePath& temp_path,
                         const std::string& callback_id,
                         size_t connection_count,
                         download::DownloadItem* item,
                         download::DownloadInterruptReason interrupt_reason);

  void ForceCloseStorageKey(const base::ListValue* args);
  void OnForcedClose(const std::string& callback_id, uint64_t connection_count);

  bool GetStorageKeyControl(const base::FilePath& path,
                            const blink::StorageKey& storage_key,
                            storage::mojom::IndexedDBControl** control);
  bool GetStorageKeyData(const base::ListValue* args,
                         std::string* callback_id,
                         base::FilePath* path,
                         blink::StorageKey* storage_key,
                         storage::mojom::IndexedDBControl** control);

  base::WeakPtrFactory<IndexedDBInternalsHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(IndexedDBInternalsHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
