// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom-forward.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/indexed_db/indexed_db_internals.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_internals.mojom.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
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

using storage::mojom::IdbPartitionMetadataPtr;

namespace content::indexed_db {

namespace {

scoped_refptr<DevToolsAgentHostImpl> GetDevToolsAgentHostForClient(
    const storage::BucketClientInfo& client_info) {
  int32_t process_id = client_info.process_id;
  const blink::ExecutionContextToken& context_token = client_info.context_token;

  if (client_info.document_token) {
    auto* rfh = RenderFrameHostImpl::FromDocumentToken(
        process_id, client_info.document_token.value());
    return rfh ? RenderFrameDevToolsAgentHost::GetFor(rfh) : nullptr;
  }

  if (context_token.Is<blink::SharedWorkerToken>()) {
    auto* rph = RenderProcessHost::FromID(process_id);
    if (!rph || !rph->IsInitializedAndNotDead()) {
      return nullptr;
    }
    auto* worker_service = static_cast<SharedWorkerServiceImpl*>(
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition())
            ->GetSharedWorkerService());
    SharedWorkerHost* shared_worker_host =
        worker_service->GetSharedWorkerHostFromToken(
            context_token.GetAs<blink::SharedWorkerToken>());
    return shared_worker_host
               ? SharedWorkerDevToolsAgentHost::GetFor(shared_worker_host)
               : nullptr;
  }

  if (context_token.Is<blink::ServiceWorkerToken>()) {
    auto* rph = RenderProcessHost::FromID(process_id);
    if (!rph || !rph->IsInitializedAndNotDead()) {
      return nullptr;
    }
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition())
            ->GetServiceWorkerContext();
    for (const auto& [version_id, info] :
         service_worker_context->GetRunningServiceWorkerInfos()) {
      if (info.token != context_token.GetAs<blink::ServiceWorkerToken>()) {
        continue;
      }
      ServiceWorkerVersion* version =
          service_worker_context->GetLiveVersion(version_id);
      return version ? ServiceWorkerDevToolsManager::GetInstance()
                           ->GetDevToolsAgentHostForWorker(
                               version->GetInfo().process_id,
                               version->GetInfo().devtools_agent_route_id)
                     : nullptr;
    }
    return nullptr;
  }

  NOTREACHED_NORETURN();
}

}  // namespace

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
      "trusted-types jstemplate static-types;");
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
            std::move(callback).Run(std::nullopt, std::move(partitions));
          },
          std::move(callback)));

  browser_context->ForEachLoadedStoragePartition(
      [&](StoragePartition* partition) {
        partition->GetIndexedDBControl().GetAllBucketsDetails(base::BindOnce(
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
    std::move(callback).Run("IndexedDB control not found");
    return;
  }

  control->ForceClose(
      bucket_id, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBInternalsUI> handler,
             storage::BucketId bucket_id,
             storage::mojom::IndexedDBControl* control,
             DownloadBucketDataCallback callback) {
            if (!handler) {
              return;
            }

            control->DownloadBucketData(
                bucket_id,
                base::BindOnce(&IndexedDBInternalsUI::OnDownloadDataReady,
                               handler, std::move(callback)));
          },
          weak_factory_.GetWeakPtr(), bucket_id, control, std::move(callback)));
}

void IndexedDBInternalsUI::ForceClose(storage::BucketId bucket_id,
                                      ForceCloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::mojom::IndexedDBControl* control = GetBucketControl(bucket_id);
  if (!control) {
    std::move(callback).Run("IndexedDB control not found");
    return;
  }

  control->ForceClose(
      bucket_id, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](ForceCloseCallback callback) {
            std::move(callback).Run(std::nullopt);
          },
          std::move(callback)));
}

void IndexedDBInternalsUI::StartMetadataRecording(
    storage::BucketId bucket_id,
    StartMetadataRecordingCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::mojom::IndexedDBControl* control = GetBucketControl(bucket_id);
  if (!control) {
    std::move(callback).Run("IndexedDB control not found");
    return;
  }

  control->StartMetadataRecording(
      bucket_id, base::BindOnce(std::move(callback), std::nullopt));
}
void IndexedDBInternalsUI::StopMetadataRecording(
    storage::BucketId bucket_id,
    StopMetadataRecordingCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::mojom::IndexedDBControl* control = GetBucketControl(bucket_id);
  if (!control) {
    std::move(callback).Run("IndexedDB control not found", {});
    return;
  }

  control->StopMetadataRecording(
      bucket_id, base::BindOnce(std::move(callback), std::nullopt));
}

void IndexedDBInternalsUI::InspectClient(
    const storage::BucketClientInfo& client_info,
    InspectClientCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!devtools_agent_hosts_created_) {
    // If a DevTools window has never been opened in this browser session,
    // DevToolsAgentHosts will not have been created for RenderFrameHosts.
    // Trigger their creation now so that the inspect call succeeds.
    DevToolsAgentHostImpl::GetOrCreateAll();
    devtools_agent_hosts_created_ = true;
  }

  scoped_refptr<DevToolsAgentHostImpl> dev_tools_agent =
      GetDevToolsAgentHostForClient(client_info);
  if (dev_tools_agent && dev_tools_agent->Inspect()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run("Client not found");
}

void IndexedDBInternalsUI::OnDownloadDataReady(
    DownloadBucketDataCallback callback,
    bool success,
    const base::FilePath& temp_path,
    const base::FilePath& zip_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    std::move(callback).Run("Error downloading database");
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
      temp_path, std::move(callback)));

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
      NOTREACHED_IN_MIGRATION();
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
    download::DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  if (interrupt_reason != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    LOG(ERROR) << "Error downloading database dump: "
               << DownloadInterruptReasonToString(interrupt_reason);
    std::move(callback).Run("Error downloading database");
    return;
  }

  item->AddObserver(new FileDeleter(temp_path));
  std::move(callback).Run(std::nullopt);
}

}  // namespace content::indexed_db
