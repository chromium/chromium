// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/privileged/mojom/indexed_db_bucket_types.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_internals.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_internals.mojom.h"
#include "content/grit/indexed_db_resources.h"
#include "content/grit/indexed_db_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using storage::mojom::IdbPartitionMetadataPtr;

namespace content {

IndexedDBInternalsUI::IndexedDBInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIIndexedDBInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");
  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kIndexedDbResources, kIndexedDbResourcesSize));
  source->AddResourcePath("", IDR_INDEXED_DB_INDEXEDDB_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(IndexedDBInternalsUI)

IndexedDBInternalsUI::~IndexedDBInternalsUI() = default;

void IndexedDBInternalsUI::WebUIRenderFrameCreated(RenderFrameHost* rfh) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  rfh->EnableMojoJsBindings(nullptr);
}

void IndexedDBInternalsUI::BindInterface(
    mojo::PendingReceiver<storage::mojom::IdbInternalsHandler> receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<storage::mojom::IdbInternalsHandler>>(
          this, std::move(receiver));
}

void IndexedDBInternalsUI::GetAllBucketsAcrossAllStorageKeys(
    GetAllBucketsAcrossAllStorageKeysCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  auto collect_partitions = base::BarrierCallback<IdbPartitionMetadataPtr>(
      browser_context->GetLoadedStoragePartitionCount(),
      base::BindOnce(
          [](GetAllBucketsAcrossAllStorageKeysCallback callback,
             std::vector<IdbPartitionMetadataPtr> partitions) {
            std::move(callback).Run(absl::nullopt, std::move(partitions));
          },
          std::move(callback)));

  browser_context->ForEachLoadedStoragePartition(
      [&](StoragePartition* partition) {
        storage::mojom::IndexedDBControl& control =
            partition->GetIndexedDBControl();
        control.GetAllBucketsDetails(base::BindOnce(
            [](base::WeakPtr<IndexedDBInternalsUI> handler,
               base::RepeatingCallback<void(IdbPartitionMetadataPtr)>
                   collect_partitions,
               base::FilePath partition_path, bool incognito,
               std::vector<storage::mojom::IdbOriginMetadataPtr> origin_list) {
              if (!handler) {
                return;
              }
              for (const storage::mojom::IdbOriginMetadataPtr& origin :
                   origin_list) {
                for (const storage::mojom::IdbStorageKeyMetadataPtr&
                         storage_key : origin->storage_keys) {
                  for (const storage::mojom::IdbBucketMetadataPtr& bucket :
                       storage_key->buckets) {
                    handler->bucket_to_partition_path_map_
                        [bucket->bucket_locator.id] = partition_path;
                  }
                }
              }

              IdbPartitionMetadataPtr partition =
                  storage::mojom::IdbPartitionMetadata::New();
              partition->partition_path =
                  incognito ? base::FilePath() : partition_path;
              partition->origin_list = std::move(origin_list);

              collect_partitions.Run(std::move(partition));
            },
            weak_factory_.GetWeakPtr(), collect_partitions,
            partition->GetPath()));
      });
}

storage::mojom::IndexedDBControl* IndexedDBInternalsUI::GetBucketControl(
    storage::BucketId bucket_id) {
  auto partition_path_iter = bucket_to_partition_path_map_.find(bucket_id);
  if (partition_path_iter == bucket_to_partition_path_map_.end()) {
    return nullptr;
  }
  const base::FilePath& partition_path = partition_path_iter->second;

  // Search the storage partitions by path.
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  storage::mojom::IndexedDBControl* control = nullptr;
  browser_context->ForEachLoadedStoragePartition(
      [&](StoragePartition* storage_partition) {
        if (storage_partition->GetPath() == partition_path) {
          DCHECK_EQ(control, nullptr);
          control = &storage_partition->GetIndexedDBControl();
        }
      });

  return control;
}

void IndexedDBInternalsUI::DownloadBucketData(
    storage::BucketId bucket_id,
    DownloadBucketDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::mojom::IndexedDBControl* control = GetBucketControl(bucket_id);
  if (!control) {
    std::move(callback).Run("IndexedDb control not found", {});
    return;
  }

  control->ForceClose(
      bucket_id, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBInternalsUI> handler,
             storage::BucketId bucket_id,
             storage::mojom::IndexedDBControl* control,
             DownloadBucketDataCallback callback) {
            // Is the connection count always zero after closing,
            // such that this can be simplified?
            control->GetConnectionCount(
                bucket_id,
                base::BindOnce(
                    [](base::WeakPtr<IndexedDBInternalsUI> handler,
                       storage::BucketId bucket_id,
                       storage::mojom::IndexedDBControl* control,
                       DownloadBucketDataCallback callback,
                       uint64_t connection_count) {
                      if (!handler) {
                        return;
                      }

                      control->DownloadBucketData(
                          bucket_id,
                          base::BindOnce(
                              &IndexedDBInternalsUI::OnDownloadDataReady,
                              handler, std::move(callback), connection_count));
                    },
                    handler, bucket_id, control, std::move(callback)));
          },
          weak_factory_.GetWeakPtr(), bucket_id, control, std::move(callback)));
}

void IndexedDBInternalsUI::ForceClose(storage::BucketId bucket_id,
                                      ForceCloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::mojom::IndexedDBControl* control = GetBucketControl(bucket_id);
  if (!control) {
    std::move(callback).Run("IndexedDb control not found", {});
    return;
  }

  control->ForceClose(
      bucket_id, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](storage::BucketId bucket_id,
             storage::mojom::IndexedDBControl* control,
             ForceCloseCallback callback) {
            control->GetConnectionCount(
                bucket_id,
                base::BindOnce(
                    [](ForceCloseCallback callback, uint64_t connection_count) {
                      std::move(callback).Run(absl::nullopt, connection_count);
                    },
                    std::move(callback)));
          },
          bucket_id, control, std::move(callback)));
}

void IndexedDBInternalsUI::OnDownloadDataReady(
    DownloadBucketDataCallback callback,
    uint64_t connection_count,
    bool success,
    const base::FilePath& temp_path,
    const base::FilePath& zip_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    std::move(callback).Run("Error downloading database", {});
    return;
  }

  const GURL url = GURL("file://" + zip_path.AsUTF8Unsafe());
  WebContents* web_contents = web_ui()->GetWebContents();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("indexed_db_internals_handler", R"(
        semantics {
          sender: "Indexed DB Internals"
          description:
            "This is an internal Chrome webpage that displays debug "
            "information about IndexedDB usage and data, used by developers."
          trigger: "When a user navigates to chrome://indexeddb-internals/."
          data: "None."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings, but it's only "
            "triggered by navigating to the specified URL."
          policy_exception_justification:
            "Not implemented. Indexed DB is Chrome's internal local data "
            "storage."
        })");
  std::unique_ptr<download::DownloadUrlParameters> dl_params(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url, traffic_annotation));
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url, content::Referrer(web_contents->GetLastCommittedURL(),
                             network::mojom::ReferrerPolicy::kDefault));
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      Referrer::ReferrerPolicyForUrlRequest(referrer.policy));

  // This is how to watch for the download to finish: first wait for it
  // to start, then attach a download::DownloadItem::Observer to observe the
  // state change to the finished state.
  dl_params->set_callback(base::BindOnce(
      &IndexedDBInternalsUI::OnDownloadStarted, weak_factory_.GetWeakPtr(),
      temp_path, std::move(callback), connection_count));

  BrowserContext* context = web_contents->GetBrowserContext();
  context->GetDownloadManager()->DownloadUrl(std::move(dl_params));
}

// The entire purpose of this class is to delete the temp file after
// the download is complete.
class FileDeleter : public download::DownloadItem::Observer {
 public:
  explicit FileDeleter(const base::FilePath& temp_dir) : temp_dir_(temp_dir) {}

  FileDeleter(const FileDeleter&) = delete;
  FileDeleter& operator=(const FileDeleter&) = delete;

  ~FileDeleter() override;

  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadOpened(download::DownloadItem* item) override {}
  void OnDownloadRemoved(download::DownloadItem* item) override {}
  void OnDownloadDestroyed(download::DownloadItem* item) override {}

 private:
  const base::FilePath temp_dir_;
};

void FileDeleter::OnDownloadUpdated(download::DownloadItem* item) {
  switch (item->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      break;
    case download::DownloadItem::COMPLETE:
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED: {
      item->RemoveObserver(this);
      delete this;
      break;
    }
    default:
      NOTREACHED();
  }
}

FileDeleter::~FileDeleter() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::GetDeletePathRecursivelyCallback(std::move(temp_dir_)));
}

void IndexedDBInternalsUI::OnDownloadStarted(
    const base::FilePath& temp_path,
    DownloadBucketDataCallback callback,
    size_t connection_count,
    download::DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  if (interrupt_reason != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    LOG(ERROR) << "Error downloading database dump: "
               << DownloadInterruptReasonToString(interrupt_reason);
    std::move(callback).Run("Error downloading database", {});
    return;
  }

  item->AddObserver(new FileDeleter(temp_path));
  std::move(callback).Run(absl::nullopt, connection_count);
}

}  // namespace content
