// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file_manager.h"

#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/download/save_file.h"
#include "content/browser/download/save_package.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/file_system/native_file_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Pointer to the singleton SaveFileManager instance.
static SaveFileManager* g_save_file_manager = nullptr;

}  // namespace

class SaveFileManager::SimpleURLLoaderHelper
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  using URLLoaderCompleteCallback = base::OnceCallback<void(bool success)>;
  static std::unique_ptr<SimpleURLLoaderHelper> CreateAndStartDownload(
      std::unique_ptr<network::ResourceRequest> resource_request,
      SaveItemId save_item_id,
      SavePackageId save_package_id,
      int render_process_id,
      int render_frame_routing_id,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      network::mojom::URLLoaderFactory* url_loader_factory,
      SaveFileManager* save_file_manager,
      URLLoaderCompleteCallback on_complete_cb) {
    return std::unique_ptr<SimpleURLLoaderHelper>(new SimpleURLLoaderHelper(
        std::move(resource_request), save_item_id, save_package_id,
        render_process_id, render_frame_routing_id, annotation_tag,
        url_loader_factory, save_file_manager, std::move(on_complete_cb)));
  }

  SimpleURLLoaderHelper(const SimpleURLLoaderHelper&) = delete;
  SimpleURLLoaderHelper& operator=(const SimpleURLLoaderHelper&) = delete;

  ~SimpleURLLoaderHelper() override = default;

 private:
  SimpleURLLoaderHelper(
      std::unique_ptr<network::ResourceRequest> resource_request,
      SaveItemId save_item_id,
      SavePackageId save_package_id,
      int render_process_id,
      int render_frame_routing_id,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      network::mojom::URLLoaderFactory* url_loader_factory,
      SaveFileManager* save_file_manager,
      URLLoaderCompleteCallback on_complete_cb)
      : save_file_manager_(save_file_manager),
        save_item_id_(save_item_id),
        save_package_id_(save_package_id),
        on_complete_cb_(std::move(on_complete_cb)) {
    GURL url = resource_request->url;
    url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   annotation_tag);
    // We can use Unretained below as |url_loader_| is owned by |this|, so the
    // callback won't be invoked if |this| gets deleted.
    url_loader_->SetOnResponseStartedCallback(base::BindOnce(
        &SimpleURLLoaderHelper::OnResponseStarted, base::Unretained(this), url,
        render_process_id, render_frame_routing_id));
    url_loader_->DownloadAsStream(url_loader_factory, this);
  }

  void OnResponseStarted(GURL url,
                         int render_process_id,
                         int render_frame_routing_id,
                         const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    std::string content_disposition;
    if (response_head.headers) {
      response_head.headers->GetNormalizedHeader("Content-Disposition",
                                                 &content_disposition);
    }

    auto info = std::make_unique<SaveFileCreateInfo>(
        url, final_url, save_item_id_, save_package_id_, render_process_id,
        render_frame_routing_id, content_disposition);
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&SaveFileManager::StartSave,
                                  save_file_manager_, std::move(info)));
  }

  // network::SimpleURLLoaderStreamConsumer implementation:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override {
    // TODO(jcivelli): we should make threading sane and avoid copying
    // |string_piece| bytes.
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveFileManager::UpdateSaveProgress, save_file_manager_,
                       save_item_id_, std::string(string_piece)));
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_complete_cb_), success));
  }

  void OnRetry(base::OnceClosure start_retry) override {
    // Retries are not enabled.
    NOTREACHED_IN_MIGRATION();
  }

  raw_ptr<SaveFileManager> save_file_manager_;
  SaveItemId save_item_id_;
  SavePackageId save_package_id_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  URLLoaderCompleteCallback on_complete_cb_;
};

SaveFileManager::SaveFileManager() {
  DCHECK(g_save_file_manager == nullptr);
  g_save_file_manager = this;
}

SaveFileManager::~SaveFileManager() {
  // Check for clean shutdown.
  DCHECK(save_file_map_.empty());
  DCHECK(g_save_file_manager);
  g_save_file_manager = nullptr;
}

// static
SaveFileManager* SaveFileManager::Get() {
  return g_save_file_manager;
}

// Called during the browser shutdown process to clean up any state (open files,
// timers) that live on the saving thread (file thread).
void SaveFileManager::Shutdown() {
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::OnShutdown, this));
}

// Stop file thread operations.
void SaveFileManager::OnShutdown() {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  save_file_map_.clear();
}

SaveFile* SaveFileManager::LookupSaveFile(SaveItemId save_item_id) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  auto it = save_file_map_.find(save_item_id);
  return it == save_file_map_.end() ? nullptr : it->second.get();
}

// Look up a SavePackage according to a save id.
SavePackage* SaveFileManager::LookupPackage(SaveItemId save_item_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = packages_.find(save_item_id);
  if (it != packages_.end())
    return it->second;
  return nullptr;
}

// Call from SavePackage for starting a saving job
void SaveFileManager::SaveURL(
    SaveItemId save_item_id,
    const GURL& url,
    const Referrer& referrer,
    const net::IsolationInfo& isolation_info,
    network::mojom::RequestMode request_mode,
    bool is_outermost_main_frame,
    int render_process_host_id,
    int render_view_routing_id,
    int render_frame_routing_id,
    SaveFileCreateInfo::SaveFileSource save_source,
    const base::FilePath& file_full_path,
    BrowserContext* context,
    StoragePartition* storage_partition,
    SavePackage* save_package,
    const std::string& client_guid,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Insert started saving job to tracking list.
  DCHECK(!base::Contains(packages_, save_item_id));
  packages_[save_item_id] = save_package;

  // Register a saving job.
  if (save_source == SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
    DCHECK(url.is_valid());
    // Starts the actual download.
    if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
            render_process_host_id, url)) {
      download::GetDownloadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&SaveFileManager::SaveFinished, this, save_item_id,
                         save_package->id(), /*success=*/false));
      return;
    }

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("save_file_manager", R"(
        semantics {
          sender: "Save File"
          description: "Saving url to local file."
          trigger:
            "User clicks on 'Save link as...' context menu command to save a "
            "link."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disable by settings. The request is made "
            "only if user chooses 'Save link as...' in context menu."
          policy_exception_justification: "Not implemented."
        })");

    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->referrer = referrer.url;
    request->priority = net::DEFAULT_PRIORITY;
    request->load_flags = net::LOAD_SKIP_CACHE_VALIDATION;
    request->mode = request_mode;
    if (request_mode == network::mojom::RequestMode::kNavigate) {
      request->update_first_party_url_on_redirect = true;
    }
    request->is_outermost_main_frame = is_outermost_main_frame;
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info = isolation_info;
    request->site_for_cookies = isolation_info.site_for_cookies();

    network::mojom::URLLoaderFactory* factory = nullptr;
    mojo::Remote<network::mojom::URLLoaderFactory> factory_remote;
    auto* rfh = RenderFrameHostImpl::FromID(render_process_host_id,
                                            render_frame_routing_id);

    // TODO(qinmin): should this match the if statements in
    // DownloadManagerImpl::BeginResourceDownloadOnChecksComplete so that it
    // can handle blob, file, webui, embedder provided schemes etc?
    // https://crbug.com/953967
    if (url.SchemeIs(url::kDataScheme)) {
      factory_remote.Bind(DataURLLoaderFactory::Create());
      factory = factory_remote.get();
    } else if (url.SchemeIsFile()) {
      factory_remote.Bind(FileURLLoaderFactory::Create(
          context->GetPath(), context->GetSharedCorsOriginAccessList(),
          base::TaskPriority::USER_VISIBLE));
      factory = factory_remote.get();
    } else if (url.SchemeIsFileSystem() && rfh) {
      auto* storage_partition_impl =
          static_cast<StoragePartitionImpl*>(storage_partition);
      auto partition_domain =
          rfh->GetSiteInstance()->GetPartitionDomain(storage_partition_impl);
      factory_remote.Bind(CreateFileSystemURLLoaderFactory(
          rfh->GetProcess()->GetID(), rfh->GetFrameTreeNodeId(),
          storage_partition->GetFileSystemContext(), partition_domain,
          static_cast<RenderFrameHostImpl*>(rfh)->GetStorageKey()));
      factory = factory_remote.get();
    } else if (rfh && url.SchemeIs(content::kChromeUIScheme)) {
      factory_remote.Bind(CreateWebUIURLLoaderFactory(rfh, url.scheme(), {}));
      factory = factory_remote.get();
    } else {
      factory = storage_partition->GetURLLoaderFactoryForBrowserProcess().get();
    }

    base::OnceCallback<void(bool /*success*/)> save_finished_cb =
        base::BindOnce(&SaveFileManager::OnURLLoaderComplete, this,
                       save_item_id, save_package->id(),
                       context->IsOffTheRecord() ? GURL() : url,
                       context->IsOffTheRecord() ? GURL() : referrer.url,
                       client_guid, std::move(remote_quarantine));

    url_loader_helpers_[save_item_id] =
        SimpleURLLoaderHelper::CreateAndStartDownload(
            std::move(request), save_item_id, save_package->id(),
            render_process_host_id, render_frame_routing_id, traffic_annotation,
            factory, this, std::move(save_finished_cb));
  } else {
    // We manually start the save job.
    auto info = std::make_unique<SaveFileCreateInfo>(
        file_full_path, url, save_item_id, save_package->id(),
        render_process_host_id, render_frame_routing_id, save_source);

    // Since the data will come from render process, so we need to start
    // this kind of save job by ourself.
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveFileManager::StartSave, this, std::move(info)));
  }
}

// Utility function for look up table maintenance, called on the UI thread.
// A manager may have multiple save page job (SavePackage) in progress,
// so we just look up the save id and remove it from the tracking table.
void SaveFileManager::RemoveSaveFile(SaveItemId save_item_id,
                                     SavePackage* save_package) {
  DCHECK(save_package);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // A save page job (SavePackage) can only have one manager,
  // so remove it if it exists.
  auto it = packages_.find(save_item_id);
  if (it != packages_.end())
    packages_.erase(it);
}

// Static
SavePackage* SaveFileManager::GetSavePackageFromRenderIds(
    int render_process_id,
    int render_frame_routing_id) {
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_routing_id);
  if (!render_frame_host)
    return nullptr;

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(
          render_frame_host));
  if (!web_contents)
    return nullptr;

  return web_contents->save_package();
}

void SaveFileManager::SendCancelRequest(SaveItemId save_item_id) {
  // Cancel the request which has specific save id.
  DCHECK(!save_item_id.is_null());
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveFileManager::CancelSave, this, save_item_id));
}

void SaveFileManager::OnURLLoaderComplete(
    SaveItemId save_item_id,
    SavePackageId save_package_id,
    const GURL& url,
    const GURL& referrer_url,
    const std::string& client_guid,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    bool is_success) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  SaveFile* save_file = LookupSaveFile(save_item_id);
  if (!is_success || !save_file) {
    SaveFinished(save_item_id, save_package_id, is_success);
    return;
  }

  save_file->AnnotateWithSourceInformation(
      client_guid, url, referrer_url, std::move(remote_quarantine),
      base::BindOnce(&SaveFileManager::OnQuarantineComplete, this, save_item_id,
                     save_package_id));
}

void SaveFileManager::OnQuarantineComplete(
    SaveItemId save_item_id,
    SavePackageId save_package_id,
    download::DownloadInterruptReason result) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  SaveFinished(save_item_id, save_package_id,
               result == download::DOWNLOAD_INTERRUPT_REASON_NONE);
}

// Notifications sent from the IO thread and run on the file thread:

// The IO thread created |info|, but the file thread (this method) uses it
// to create a SaveFile which will hold and finally destroy |info|. It will
// then passes |info| to the UI thread for reporting saving status.
void SaveFileManager::StartSave(std::unique_ptr<SaveFileCreateInfo> info) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(info);
  // No need to calculate hash.
  auto save_file =
      std::make_unique<SaveFile>(std::move(info), /*calculate_hash=*/false);

  // TODO(phajdan.jr): We should check the return value and handle errors here.
  save_file->Initialize();

  const SaveFileCreateInfo& save_file_create_info = save_file->create_info();
  DCHECK(!LookupSaveFile(save_file->save_item_id()));
  save_file_map_[save_file->save_item_id()] = std::move(save_file);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::OnStartSave, this,
                                save_file_create_info));
}

// We do forward an update to the UI thread here, since we do not use timer to
// update the UI. If the user has canceled the saving action (in the UI
// thread). We may receive a few more updates before the IO thread gets the
// cancel message. We just delete the data since the SaveFile has been deleted.
void SaveFileManager::UpdateSaveProgress(SaveItemId save_item_id,
                                         const std::string& data) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  SaveFile* save_file = LookupSaveFile(save_item_id);
  if (save_file) {
    DCHECK(save_file->InProgress());

    download::DownloadInterruptReason reason =
        save_file->AppendDataToFile(data.data(), data.size());
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveFileManager::OnUpdateSaveProgress, this,
                       save_file->save_item_id(), save_file->BytesSoFar(),
                       reason == download::DOWNLOAD_INTERRUPT_REASON_NONE));
  }
}

// The IO thread will call this when saving is completed or it got error when
// fetching data. We forward the message to OnSaveFinished in UI thread.
void SaveFileManager::SaveFinished(SaveItemId save_item_id,
                                   SavePackageId save_package_id,
                                   bool is_success) {
  DVLOG(20) << __func__ << "() save_item_id = " << save_item_id
            << " save_package_id = " << save_package_id
            << " is_success = " << is_success;
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  int64_t bytes_so_far = 0;
  SaveFile* save_file = LookupSaveFile(save_item_id);
  // Note that we might not have a save_file: canceling starts on the download
  // thread but the load is canceled on the UI thread. The request might finish
  // while thread hoping.
  if (save_file) {
    DCHECK(save_file->InProgress());
    DVLOG(20) << __func__ << "() save_file = " << save_file->DebugString();
    bytes_so_far = save_file->BytesSoFar();
    save_file->Finish();
    save_file->Detach();
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::OnSaveFinished, this,
                                save_item_id, bytes_so_far, is_success));
}

// Notifications sent from the file thread and run on the UI thread.

void SaveFileManager::OnStartSave(const SaveFileCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SavePackage* save_package = GetSavePackageFromRenderIds(
      info.render_process_id, info.render_frame_routing_id);
  if (!save_package) {
    // Cancel this request.
    SendCancelRequest(info.save_item_id);
    return;
  }

  // Forward this message to SavePackage.
  save_package->StartSave(&info);
}

void SaveFileManager::OnUpdateSaveProgress(SaveItemId save_item_id,
                                           int64_t bytes_so_far,
                                           bool write_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SavePackage* package = LookupPackage(save_item_id);
  if (package)
    package->UpdateSaveProgress(save_item_id, bytes_so_far, write_success);
  else
    SendCancelRequest(save_item_id);
}

void SaveFileManager::OnSaveFinished(SaveItemId save_item_id,
                                     int64_t bytes_so_far,
                                     bool is_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClearURLLoader(save_item_id);
  SavePackage* package = LookupPackage(save_item_id);
  if (package)
    package->SaveFinished(save_item_id, bytes_so_far, is_success);
}

// Notifications sent from the UI thread and run on the file thread.

// This method will be sent via a user action, or shutdown on the UI thread,
// and run on the file thread. We don't post a message back for cancels,
// but we do forward the cancel to the IO thread. Since this message has been
// sent from the UI thread, the saving job may have already completed and
// won't exist in our map.
void SaveFileManager::CancelSave(SaveItemId save_item_id) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  auto it = save_file_map_.find(save_item_id);
  if (it != save_file_map_.end()) {
    std::unique_ptr<SaveFile> save_file = std::move(it->second);

    if (!save_file->InProgress()) {
      // We've won a race with the UI thread--we finished the file before
      // the UI thread cancelled it on us.  Unfortunately, in this situation
      // the cancel wins, so we need to delete the now detached file.
      base::DeleteFile(save_file->FullPath());
    } else if (save_file->save_source() ==
               SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&SaveFileManager::ClearURLLoader, this, save_item_id));
    }

    // Whatever the save file is complete or not, just delete it.  This
    // will delete the underlying file if InProgress() is true.
    save_file_map_.erase(it);
  }
}

void SaveFileManager::ClearURLLoader(SaveItemId save_item_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto url_loader_iter = url_loader_helpers_.find(save_item_id);
  if (url_loader_iter != url_loader_helpers_.end())
    url_loader_helpers_.erase(url_loader_iter);
}

void SaveFileManager::RenameAllFiles(const FinalNamesMap& final_names,
                                     const base::FilePath& resource_dir,
                                     int render_process_id,
                                     int render_frame_routing_id,
                                     SavePackageId save_package_id) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  if (!resource_dir.empty() && !base::PathExists(resource_dir)) {
    // Use `NativeFileUtil::CreateDirectory` instead of `base::CreateDirectory`
    // to set the correct permissions on ChromeOS.
    storage::NativeFileUtil::CreateDirectory(resource_dir, /*exclusive=*/false,
                                             /*recursive=*/true);
  }

  for (const auto& i : final_names) {
    SaveItemId save_item_id = i.first;
    const base::FilePath& final_name = i.second;

    auto it = save_file_map_.find(save_item_id);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second.get();
      DCHECK(!save_file->InProgress());
      save_file->Rename(final_name);
      save_file_map_.erase(it);
    }
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::OnFinishSavePageJob, this,
                                render_process_id, render_frame_routing_id,
                                save_package_id));
}

void SaveFileManager::OnFinishSavePageJob(int render_process_id,
                                          int render_frame_routing_id,
                                          SavePackageId save_package_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SavePackage* save_package =
      GetSavePackageFromRenderIds(render_process_id, render_frame_routing_id);

  if (save_package && save_package->id() == save_package_id)
    save_package->Finish();
}

void SaveFileManager::RemoveSavedFileFromFileMap(
    const std::vector<SaveItemId>& save_item_ids) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  for (const SaveItemId save_item_id : save_item_ids) {
    auto it = save_file_map_.find(save_item_id);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second.get();
      DCHECK(!save_file->InProgress());
      base::DeleteFile(save_file->FullPath());
      save_file_map_.erase(it);
    }
  }
}

void SaveFileManager::GetSaveFilePaths(
    const std::vector<std::pair<SaveItemId, base::FilePath>>&
        ids_and_final_paths,
    base::OnceCallback<void(base::flat_map<base::FilePath, base::FilePath>)>
        callback) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  base::flat_map<base::FilePath, base::FilePath> tmp_paths_and_final_paths;

  for (const auto& id_and_final_path : ids_and_final_paths) {
    auto it = save_file_map_.find(id_and_final_path.first);
    if (it != save_file_map_.end() && !it->second->FullPath().empty() &&
        !id_and_final_path.second.empty()) {
      tmp_paths_and_final_paths.insert(
          {it->second->FullPath(), id_and_final_path.second});
    }
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::move(tmp_paths_and_final_paths)));
}

}  // namespace content
