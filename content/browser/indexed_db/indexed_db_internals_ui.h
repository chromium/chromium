// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace download {
class DownloadItem;
}

namespace content {

class IndexedDBInternalsUI;

class IndexedDBInternalsUIConfig
    : public DefaultWebUIConfig<IndexedDBInternalsUI> {
 public:
  IndexedDBInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIIndexedDBInternalsHost) {}
};

// The implementation for the chrome://indexeddb-internals page.
class IndexedDBInternalsUI : public WebUIController {
 public:
  explicit IndexedDBInternalsUI(WebUI* web_ui);

  IndexedDBInternalsUI(const IndexedDBInternalsUI&) = delete;
  IndexedDBInternalsUI& operator=(const IndexedDBInternalsUI&) = delete;

  ~IndexedDBInternalsUI() override;

 private:
  base::WeakPtrFactory<IndexedDBInternalsUI> weak_factory_{this};
};

class IndexedDBInternalsHandler : public WebUIMessageHandler {
 public:
  IndexedDBInternalsHandler();

  IndexedDBInternalsHandler(const IndexedDBInternalsHandler&) = delete;
  IndexedDBInternalsHandler& operator=(const IndexedDBInternalsHandler&) =
      delete;

  ~IndexedDBInternalsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  void GetAllBuckets(const base::Value::List& args);
  void OnBucketsReady(const base::Value::List& storage_keys,
                      const base::FilePath& path);

  void DownloadBucketData(const base::Value::List& args);
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

  void ForceCloseBucket(const base::Value::List& args);
  void OnForcedClose(const std::string& callback_id, uint64_t connection_count);

  bool GetBucketControl(const base::FilePath& path,
                        storage::mojom::IndexedDBControl** control);
  bool GetBucketData(const base::Value::List& args,
                     std::string* callback_id,
                     base::FilePath* path,
                     storage::BucketId* bucket_id,
                     storage::mojom::IndexedDBControl** control);

  base::WeakPtrFactory<IndexedDBInternalsHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
