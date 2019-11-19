// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file_manager.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/download/save_file.h"
#include "content/browser/download/save_package.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/previews_state.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// Pointer to the singleton SaveFileManager instance.
static SaveFileManager* g_save_file_manager = nullptr;

}  // namespace

class SaveFileManager::SimpleURLLoaderHelper
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  static std::unique_ptr<SimpleURLLoaderHelper> CreateAndStartDownload(
      std::unique_ptr<network::ResourceRequest> resource_request,
      SaveItemId save_item_id,
      SavePackageId save_package_id,
      int render_process_id,
      int render_frame_routing_id,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      network::mojom::URLLoaderFactory* url_loader_factory,
      SaveFileManager* save_file_manager) {
    return std::unique_ptr<SimpleURLLoaderHelper>(new SimpleURLLoaderHelper(
        std::move(resource_request), save_item_id, save_package_id,
        render_process_id, render_frame_routing_id, annotation_tag,
        url_loader_factory, save_file_manager));
  }

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
      SaveFileManager* save_file_manager)
      : save_file_manager_(save_file_manager),
        save_item_id_(save_item_id),
        save_package_id_(save_package_id) {
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
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override {
    // TODO(jcivelli): we should make threading sane and avoid copying
    // |string_piece| bytes.
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveFileManager::UpdateSaveProgress, save_file_manager_,
                       save_item_id_, string_piece.as_string()));
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveFileManager::SaveFinished, save_file_manager_,
                       save_item_id_, save_package_id_, success));
  }

  void OnRetry(base::OnceClosure start_retry) override {
    // Retries are not enabled.
    NOTREACHED();
  }

  SaveFileManager* save_file_manager_;
  SaveItemId save_item_id_;
  SavePackageId save_package_id_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoaderHelper);
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
void SaveFileManager::SaveURL(SaveItemId save_item_id,
                              const GURL& url,
                              const Referrer& referrer,
                              int render_process_host_id,
                              int render_view_routing_id,
                              int render_frame_routing_id,
                              SaveFileCreateInfo::SaveFileSource save_source,
                              const base::FilePath& file_full_path,
                              BrowserContext* context,
                              StoragePartition* storage_partition,
                              SavePackage* save_package) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Insert started saving job to tracking list.
  DCHECK(packages_.find(save_item_id) == packages_.end());
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

    // To avoid https://crbug.com/974312, downloads initiated by Save-Page-As
    // should be treated as navigations. This definitely makes sense for the
    // top-level page (e.g. in SAVE_PAGE_TYPE_AS_ONLY_HTML mode). This is
    // probably also okay for subresources downloaded in
    // SAVE_PAGE_TYPE_AS_COMPLETE_HTML mode.
    request->mode = network::mojom::RequestMode::kNavigate;

    network::mojom::URLLoaderFactory* factory = nullptr;
    std::unique_ptr<network::mojom::URLLoaderFactory> url_loader_factory;
    RenderFrameHost* rfh = RenderFrameHost::FromID(render_process_host_id,
                                                   render_frame_routing_id);

    // TODO(qinmin): should this match the if statements in
    // DownloadManagerImpl::BeginResourceDownloadOnChecksComplete so that it
    // can handle blob, file, webui, embedder provided schemes etc?
    // https://crbug.com/953967
    if (url.SchemeIs(url::kDataScheme)) {
      url_loader_factory = std::make_unique<DataURLLoaderFactory>();
      factory = url_loader_factory.get();
    } else if (url.SchemeIsFile()) {
      url_loader_factory = std::make_unique<FileURLLoaderFactory>(
          context->GetPath(), context->GetSharedCorsOriginAccessList(),
          base::TaskPriority::USER_VISIBLE);
      factory = url_loader_factory.get();
    } else if (url.SchemeIsFileSystem() && rfh) {
      std::string storage_domain;
      auto* site_instance = rfh->GetSiteInstance();
      if (site_instance) {
        std::string partition_name;
        bool in_memory;
        GetContentClient()->browser()->GetStoragePartitionConfigForSite(
            context, site_instance->GetSiteURL(), true, &storage_domain,
            &partition_name, &in_memory);
      }
      url_loader_factory = CreateFileSystemURLLoaderFactory(
          rfh->GetProcess()->GetID(), rfh->GetFrameTreeNodeId(),
          storage_partition->GetFileSystemContext(), storage_domain);
      factory = url_loader_factory.get();
    } else if (rfh && url.SchemeIs(content::kChromeUIScheme)) {
      url_loader_factory = CreateWebUIURLLoader(rfh, url.scheme(),
                                                base::flat_set<std::string>());
      factory = url_loader_factory.get();
    } else {
      factory = storage_partition->GetURLLoaderFactoryForBrowserProcess().get();
    }

    url_loader_helpers_[save_item_id] =
        SimpleURLLoaderHelper::CreateAndStartDownload(
            std::move(request), save_item_id, save_package->id(),
            render_process_host_id, render_frame_routing_id, traffic_annotation,
            factory, this);
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

void SaveFileManager::DeleteDirectoryOrFile(const base::FilePath& full_path,
                                            bool is_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::OnDeleteDirectoryOrFile, this,
                                full_path, is_dir));
}

void SaveFileManager::SendCancelRequest(SaveItemId save_item_id) {
  // Cancel the request which has specific save id.
  DCHECK(!save_item_id.is_null());
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveFileManager::CancelSave, this, save_item_id));
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

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&SaveFileManager::OnStartSave, this,
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
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
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

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&SaveFileManager::OnSaveFinished, this,
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
      base::DeleteFile(save_file->FullPath(), false);
    } else if (save_file->save_source() ==
               SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
      base::PostTask(
          FROM_HERE, {BrowserThread::UI},
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

void SaveFileManager::OnDeleteDirectoryOrFile(const base::FilePath& full_path,
                                              bool is_dir) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!full_path.empty());

  base::DeleteFile(full_path, is_dir);
}

void SaveFileManager::RenameAllFiles(const FinalNamesMap& final_names,
                                     const base::FilePath& resource_dir,
                                     int render_process_id,
                                     int render_frame_routing_id,
                                     SavePackageId save_package_id) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  if (!resource_dir.empty() && !base::PathExists(resource_dir))
    base::CreateDirectory(resource_dir);

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

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&SaveFileManager::OnFinishSavePageJob, this,
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
      base::DeleteFile(save_file->FullPath(), false);
      save_file_map_.erase(it);
    }
  }
}

}  // namespace content
