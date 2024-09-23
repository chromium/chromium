// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "content/browser/indexed_db/indexed_db_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace download {
class DownloadItem;
}

namespace content::indexed_db {

class IndexedDBInternalsUI;

class IndexedDBInternalsUIConfig
    : public DefaultWebUIConfig<IndexedDBInternalsUI> {
 public:
  IndexedDBInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIIndexedDBInternalsHost) {}
};

// The implementation for the chrome://indexeddb-internals page.
class IndexedDBInternalsUI : public WebUIController,
                             public storage::mojom::IdbInternalsHandler {
 public:
  explicit IndexedDBInternalsUI(WebUI* web_ui);

  IndexedDBInternalsUI(const IndexedDBInternalsUI&) = delete;
  IndexedDBInternalsUI& operator=(const IndexedDBInternalsUI&) = delete;

  ~IndexedDBInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<storage::mojom::IdbInternalsHandler> receiver);

  // WebUIController:
  void WebUIRenderFrameCreated(RenderFrameHost* rfh) override;

  // storage::mojom::IdbInternalsHandler:
  void GetAllBucketsAcrossAllStorageKeys(
      GetAllBucketsAcrossAllStorageKeysCallback callback) override;
  void DownloadBucketData(storage::BucketId bucket_id,
                          DownloadBucketDataCallback callback) override;
  void ForceClose(storage::BucketId bucket_id,
                  ForceCloseCallback callback) override;
  void StartMetadataRecording(storage::BucketId bucket_id,
                              StartMetadataRecordingCallback callback) override;
  void StopMetadataRecording(storage::BucketId bucket_id,
                             StopMetadataRecordingCallback callback) override;
  void InspectClient(const storage::BucketClientInfo& client_info,
                     InspectClientCallback callback) override;

 private:
  void OnDownloadDataReady(DownloadBucketDataCallback callback,
                           bool success,
                           const base::FilePath& temp_path,
                           const base::FilePath& zip_path);
  void OnDownloadStarted(const base::FilePath& temp_path,
                         DownloadBucketDataCallback callback,
                         download::DownloadItem* item,
                         download::DownloadInterruptReason interrupt_reason);

  storage::mojom::IndexedDBControl* GetBucketControl(
      storage::BucketId bucket_id);

  std::map<storage::BucketId, base::FilePath> bucket_to_partition_path_map_;
  bool devtools_agent_hosts_created_ = false;

  std::unique_ptr<mojo::Receiver<storage::mojom::IdbInternalsHandler>>
      receiver_;
  base::WeakPtrFactory<IndexedDBInternalsUI> weak_factory_{this};
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
