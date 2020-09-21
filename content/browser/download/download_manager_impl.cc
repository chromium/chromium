// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/download/database/in_progress/download_entry.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_factory.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/input_stream.h"
#include "components/download/public/common/url_download_handler_factory.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/download/network_download_pending_url_loader_factory.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"

#if defined(USE_X11)
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_features.h"
#endif

namespace content {
namespace {

void DeleteDownloadedFileOnUIThread(const base::FilePath& file_path) {
  if (!file_path.empty()) {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&download::DeleteDownloadedFile),
                       file_path));
  }
}

StoragePartitionImpl* GetStoragePartition(BrowserContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SiteInstance* site_instance = nullptr;
  if (render_process_id >= 0) {
    RenderFrameHost* render_frame_host_ =
        RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (render_frame_host_)
      site_instance = render_frame_host_->GetSiteInstance();
  }
  return static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartition(context, site_instance));
}

void OnDownloadStarted(
    download::DownloadItemImpl* download,
    download::DownloadUrlParameters::OnStartedCallback on_started) {
  if (on_started.is_null())
    return;

  if (!download || download->GetState() == download::DownloadItem::CANCELLED) {
    std::move(on_started)
        .Run(nullptr, download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  } else {
    std::move(on_started)
        .Run(download, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  }
}

// Creates an interrupted download and calls StartDownload. Can be called on
// any thread.
void CreateInterruptedDownload(
    std::unique_ptr<download::DownloadUrlParameters> params,
    download::DownloadInterruptReason reason,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  std::unique_ptr<download::DownloadCreateInfo> failed_created_info(
      new download::DownloadCreateInfo(
          base::Time::Now(), base::WrapUnique(new download::DownloadSaveInfo)));
  failed_created_info->url_chain.push_back(params->url());
  failed_created_info->result = reason;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadManagerImpl::StartDownload, download_manager,
                     std::move(failed_created_info),
                     std::make_unique<download::InputStream>(),
                     std::move(params->callback())));
}

class DownloadItemFactoryImpl : public download::DownloadItemFactory {
 public:
  DownloadItemFactoryImpl() {}
  ~DownloadItemFactoryImpl() override {}

  download::DownloadItemImpl* CreatePersistedItem(
      download::DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const base::Optional<url::Origin>& request_initiator,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<download::DownloadItem::ReceivedSlice>& received_slices)
      override {
    // For history download only as history don't have auto resumption count
    // saved.
    int auto_resume_count = download::DownloadItemImpl::kMaxAutoResumeAttempts;

    return new download::DownloadItemImpl(
        delegate, guid, download_id, current_path, target_path, url_chain,
        referrer_url, site_url, tab_url, tab_refererr_url, request_initiator,
        mime_type, original_mime_type, start_time, end_time, etag,
        last_modified, received_bytes, total_bytes, auto_resume_count, hash,
        state, danger_type, interrupt_reason, false /* paused */,
        false /* allow_metered */, opened, last_access_time, transient,
        received_slices, base::nullopt /*download_schedule*/,
        nullptr /* download_entry */);
  }

  download::DownloadItemImpl* CreateActiveItem(
      download::DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const download::DownloadCreateInfo& info) override {
    return new download::DownloadItemImpl(delegate, download_id, info);
  }

  download::DownloadItemImpl* CreateSavePageItem(
      download::DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      download::DownloadJob::CancelRequestCallback cancel_request_callback)
      override {
    return new download::DownloadItemImpl(delegate, download_id, path, url,
                                          mime_type,
                                          std::move(cancel_request_callback));
  }
};

#if defined(USE_X11)
base::FilePath GetTemporaryDownloadDirectory() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return base::nix::GetXDGDirectory(env.get(), "XDG_DATA_HOME", ".local/share");
}
#endif

std::unique_ptr<network::PendingSharedURLLoaderFactory>
CreatePendingSharedURLLoaderFactory(StoragePartitionImpl* storage_partition,
                                    RenderFrameHost* rfh,
                                    bool is_download) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_factory_remote;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxy_factory_receiver;
  if (rfh) {
    bool should_proxy = false;

    // Create an intermediate pipe that can be used to proxy the download's
    // URLLoaderFactory.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        maybe_proxy_factory_remote;
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        maybe_proxy_factory_receiver =
            maybe_proxy_factory_remote.InitWithNewPipeAndPassReceiver();

    // Allow DevTools to potentially inject itself into the proxy pipe.
    should_proxy = devtools_instrumentation::WillCreateURLLoaderFactory(
        static_cast<RenderFrameHostImpl*>(rfh), true, is_download,
        &maybe_proxy_factory_receiver, nullptr /* factory_override */);

    // Also allow the Content embedder to inject itself if it wants to.
    should_proxy |= GetContentClient()->browser()->WillCreateURLLoaderFactory(
        rfh->GetSiteInstance()->GetBrowserContext(), rfh,
        rfh->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kDownload, url::Origin(),
        base::nullopt /* navigation_id */, base::kInvalidUkmSourceId,
        &maybe_proxy_factory_receiver, nullptr /* header_client */,
        nullptr /* bypass_redirect_checks */, nullptr /* disable_secure_dns */,
        nullptr /* factory_override */);

    // If anyone above indicated that they care about proxying, pass the
    // intermediate pipe along to the NetworkDownloadPendingURLLoaderFactory.
    if (should_proxy) {
      proxy_factory_remote = std::move(maybe_proxy_factory_remote);
      proxy_factory_receiver = std::move(maybe_proxy_factory_receiver);
    }
  }

  return std::make_unique<NetworkDownloadPendingURLLoaderFactory>(
      storage_partition->url_loader_factory_getter(),
      std::move(proxy_factory_remote), std::move(proxy_factory_receiver));
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
CreatePendingSharedURLLoaderFactoryFromURLLoaderFactory(
    std::unique_ptr<network::mojom::URLLoaderFactory> factory) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  mojo::MakeSelfOwnedReceiver(std::move(factory),
                              factory_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
      std::move(factory_remote));
}

void RecordDownloadOpenerType(RenderFrameHost* current,
                              RenderFrameHost* opener) {
  DCHECK(current);
  DCHECK(opener);
  if (!opener->GetLastCommittedURL().SchemeIsHTTPOrHTTPS() ||
      !current->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    UMA_HISTOGRAM_ENUMERATION("Download.InitiatedByWindowOpener",
                              InitiatedByWindowOpenerType::kNonHTTPOrHTTPS);
    return;
  }
  if (opener->GetLastCommittedOrigin() == current->GetLastCommittedOrigin()) {
    UMA_HISTOGRAM_ENUMERATION("Download.InitiatedByWindowOpener",
                              InitiatedByWindowOpenerType::kSameOrigin);
    return;
  }
  if (net::registry_controlled_domains::SameDomainOrHost(
          opener->GetLastCommittedOrigin(), current->GetLastCommittedOrigin(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    UMA_HISTOGRAM_ENUMERATION("Download.InitiatedByWindowOpener",
                              InitiatedByWindowOpenerType::kSameSite);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Download.InitiatedByWindowOpener",
                            InitiatedByWindowOpenerType::kCrossOrigin);
}

}  // namespace

DownloadManagerImpl::DownloadManagerImpl(BrowserContext* browser_context)
    : item_factory_(new DownloadItemFactoryImpl()),
      shutdown_needed_(true),
      history_db_initialized_(false),
      in_progress_cache_initialized_(false),
      browser_context_(browser_context),
      delegate_(nullptr),
      in_progress_manager_(
          browser_context_->RetriveInProgressDownloadManager()),
      next_download_id_(download::DownloadItem::kInvalidId),
      is_history_download_id_retrieved_(false),
      should_persist_new_download_(false),
      disk_access_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK(browser_context);

  download::SetIOTaskRunner(GetIOThreadTaskRunner({}));

  if (!in_progress_manager_) {
    auto* proto_db_provider =
        BrowserContext::GetDefaultStoragePartition(browser_context)
            ->GetProtoDatabaseProvider();
    in_progress_manager_ =
        std::make_unique<download::InProgressDownloadManager>(
            this, base::FilePath(), proto_db_provider,
            base::BindRepeating(&blink::network_utils::IsOriginSecure),
            base::BindRepeating(&DownloadRequestUtils::IsURLSafe),
            /*wake_lock_provider_binder=*/base::NullCallback());
  } else {
    in_progress_manager_->SetDelegate(this);
    in_progress_manager_->set_download_start_observer(nullptr);
    in_progress_manager_->set_is_origin_secure_cb(
        base::BindRepeating(&blink::network_utils::IsOriginSecure));
  }
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

download::DownloadItemImpl* DownloadManagerImpl::CreateActiveItem(
    uint32_t id,
    const download::DownloadCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::Contains(downloads_, id))
    return nullptr;

  download::DownloadItemImpl* download =
      item_factory_->CreateActiveItem(this, id, info);

  downloads_[id] = base::WrapUnique(download);
  downloads_by_guid_[download->GetGuid()] = download;
  DownloadItemUtils::AttachInfo(
      download, GetBrowserContext(),
      WebContentsImpl::FromRenderFrameHostID(info.render_process_id,
                                             info.render_frame_id));
  return download;
}

void DownloadManagerImpl::GetNextId(GetNextIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (IsNextIdInitialized()) {
    std::move(callback).Run(next_download_id_++);
    return;
  }

  id_callbacks_.emplace_back(
      std::make_unique<GetNextIdCallback>(std::move(callback)));
  // If we are first time here, call the delegate to get the next ID from
  // history db.
  if (!is_history_download_id_retrieved_ && id_callbacks_.size() == 1u) {
    if (delegate_) {
      delegate_->GetNextId(
          base::BindOnce(&DownloadManagerImpl::OnHistoryNextIdRetrived,
                         weak_factory_.GetWeakPtr()));
    } else {
      OnHistoryNextIdRetrived(download::DownloadItem::kInvalidId);
    }
  }
}

void DownloadManagerImpl::SetNextId(uint32_t next_id) {
  if (next_id > next_download_id_)
    next_download_id_ = next_id;
  if (!IsNextIdInitialized())
    return;

  for (auto& callback : id_callbacks_)
    std::move(*callback).Run(next_download_id_++);
  id_callbacks_.clear();
}

void DownloadManagerImpl::OnHistoryNextIdRetrived(uint32_t next_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_history_download_id_retrieved_ = true;
  if (next_id == download::DownloadItem::kInvalidId)
    next_id++;
  else
    should_persist_new_download_ = true;
  SetNextId(next_id);
}

void DownloadManagerImpl::DetermineDownloadTarget(
    download::DownloadItemImpl* item,
    DownloadTargetCallback callback) {
  // Note that this next call relies on
  // DownloadItemImplDelegate::DownloadTargetCallback and
  // DownloadManagerDelegate::DownloadTargetCallback having the same
  // type.  If the types ever diverge, gasket code will need to
  // be written here.
  if (!delegate_ || !delegate_->DetermineDownloadTarget(item, &callback)) {
    base::FilePath target_path = item->GetForcedFilePath();
    // TODO(asanka): Determine a useful path if |target_path| is empty.
    std::move(callback).Run(
        target_path, download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        download::DownloadItem::MixedContentStatus::UNKNOWN, target_path,
        base::nullopt /*download_schedule*/,
        download::DOWNLOAD_INTERRUPT_REASON_NONE);
  }
}

bool DownloadManagerImpl::ShouldCompleteDownload(
    download::DownloadItemImpl* item,
    base::OnceClosure complete_callback) {
  if (!delegate_ ||
      delegate_->ShouldCompleteDownload(item, std::move(complete_callback))) {
    return true;
  }
  // Otherwise, the delegate has accepted responsibility to run the
  // callback when the download is ready for completion.
  // TODO(qinmin): When returning false, the |complete_callback| should
  // be run by this class eventually. To do so we can't pass ownership
  // to |delegate_| unconditionally.
  return false;
}

bool DownloadManagerImpl::ShouldAutomaticallyOpenFile(
    const GURL& url,
    const base::FilePath& path) {
  if (!delegate_)
    return false;

  return delegate_->ShouldAutomaticallyOpenFile(url, path);
}

bool DownloadManagerImpl::ShouldAutomaticallyOpenFileByPolicy(
    const GURL& url,
    const base::FilePath& path) {
  if (!delegate_)
    return false;

  return delegate_->ShouldAutomaticallyOpenFileByPolicy(url, path);
}

bool DownloadManagerImpl::ShouldOpenDownload(
    download::DownloadItemImpl* item,
    ShouldOpenDownloadCallback callback) {
  if (!delegate_)
    return true;

  // Relies on DownloadItemImplDelegate::ShouldOpenDownloadCallback and
  // DownloadManagerDelegate::DownloadOpenDelayedCallback "just happening"
  // to have the same type :-}.
  return delegate_->ShouldOpenDownload(item, std::move(callback));
}

void DownloadManagerImpl::SetDelegate(DownloadManagerDelegate* delegate) {
  delegate_ = delegate;
}

DownloadManagerDelegate* DownloadManagerImpl::GetDelegate() {
  return delegate_;
}

void DownloadManagerImpl::Shutdown() {
  DVLOG(20) << __func__ << "() shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  for (auto& observer : observers_)
    observer.ManagerGoingDown(this);
  // TODO(benjhayden): Consider clearing observers_.

  // If there are in-progress downloads, cancel them. This also goes for
  // dangerous downloads which will remain in history if they aren't explicitly
  // accepted or discarded. Canceling will remove the intermediate download
  // file.
  for (const auto& it : downloads_) {
    download::DownloadItemImpl* download = it.second.get();
    if (download->GetState() == download::DownloadItem::IN_PROGRESS)
      download->Cancel(false);
  }
  downloads_.clear();
  downloads_by_guid_.clear();

  // We'll have nothing more to report to the observers after this point.
  observers_.Clear();

  if (in_progress_manager_)
    in_progress_manager_->ShutDown();

  if (delegate_)
    delegate_->Shutdown();
  delegate_ = nullptr;
}

bool DownloadManagerImpl::InterceptDownload(
    const download::DownloadCreateInfo& info) {
  WebContents* web_contents = WebContentsImpl::FromRenderFrameHostID(
      info.render_process_id, info.render_frame_id);
  if (info.is_new_download &&
      info.result ==
          download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT) {
    std::vector<GURL> url_chain(info.url_chain);
    GURL url = url_chain.back();
    if ((url.SchemeIsHTTPOrHTTPS() ||
         GetContentClient()->browser()->IsHandledURL(url)) &&
        web_contents) {
      url_chain.pop_back();
      NavigationController::LoadURLParams params(url);
      params.has_user_gesture = info.has_user_gesture;
      params.referrer = Referrer(
          info.referrer_url,
          blink::ReferrerUtils::NetToMojoReferrerPolicy(info.referrer_policy));
      params.redirect_chain = url_chain;
      params.frame_tree_node_id =
          RenderFrameHost::GetFrameTreeNodeIdForRoutingId(
              info.render_process_id, info.render_frame_id);
      params.from_download_cross_origin_redirect = true;
      params.initiator_origin = info.request_initiator;
      params.is_renderer_initiated = info.is_content_initiated;
      web_contents->GetController().LoadURLWithParams(params);
    }
    return true;
  }

  std::string user_agent;
  for (const auto& header : info.request_headers) {
    if (header.first == net::HttpRequestHeaders::kUserAgent) {
      user_agent = header.second;
      break;
    }
  }

  if (delegate_ && delegate_->InterceptDownloadIfApplicable(
                       info.url(), user_agent, info.content_disposition,
                       info.mime_type, info.request_origin, info.total_bytes,
                       info.transient, web_contents)) {
    DropDownload();
    return true;
  }
  return false;
}

base::FilePath DownloadManagerImpl::GetDefaultDownloadDirectory() {
  base::FilePath default_download_directory;
#if defined(USE_X11)
  // TODO(thomasanderson,crbug.com/784010): Remove this when all Linux
  // distros with versions of GTK lower than 3.14.7 are no longer
  // supported.  This should happen when support for Ubuntu Trusty and
  // Debian Jessie are removed.
  if (!features::IsUsingOzonePlatform())
    default_download_directory = GetTemporaryDownloadDirectory();
#endif

  if (delegate_ && default_download_directory.empty()) {
    base::FilePath website_save_directory;  // Unused
    delegate_->GetSaveDir(GetBrowserContext(), &website_save_directory,
                          &default_download_directory);
  }

  if (default_download_directory.empty()) {
    // |default_download_directory| can still be empty if ContentBrowserClient
    // returned an empty path for the downloads directory.
    default_download_directory =
        GetContentClient()->browser()->GetDefaultDownloadDirectory();
  }

  return default_download_directory;
}

void DownloadManagerImpl::OnDownloadsInitialized() {
  in_progress_downloads_ = in_progress_manager_->TakeInProgressDownloads();
  uint32_t max_id = download::DownloadItem::kInvalidId;
  for (auto it = in_progress_downloads_.begin();
       it != in_progress_downloads_.end();) {
    download::DownloadItemImpl* download = it->get();
    uint32_t id = download->GetId();
    if (id > max_id)
      max_id = id;

    // Clean up cancelled and non resumable interrupted downloads.
    if (ShouldClearDownloadFromDB(download->GetURL(), download->GetState(),
                                  download->GetLastReason(),
                                  download->GetStartTime())) {
      cleared_download_guids_on_startup_.insert(download->GetGuid());
      DeleteDownloadedFileOnUIThread(download->GetFullPath());
      it = in_progress_downloads_.erase(it);
      continue;
    }
    ++it;
  }
  PostInitialization(DOWNLOAD_INITIALIZATION_DEPENDENCY_IN_PROGRESS_CACHE);
  SetNextId(max_id + 1);
}

void DownloadManagerImpl::StartDownloadItem(
    std::unique_ptr<download::DownloadCreateInfo> info,
    download::DownloadUrlParameters::OnStartedCallback on_started,
    download::InProgressDownloadManager::StartDownloadItemCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!info->is_new_download) {
    download::DownloadItemImpl* download = downloads_by_guid_[info->guid];
    if (!download || download->GetState() == download::DownloadItem::CANCELLED)
      download = nullptr;
    std::move(callback).Run(std::move(info), download,
                            should_persist_new_download_);
    OnDownloadStarted(download, std::move(on_started));
  } else {
    // If the download already in system, it can only be resumed.
    if (!info->guid.empty() && GetDownloadByGuid(info->guid)) {
      LOG(WARNING) << "A download with the same GUID already exists, the new "
                      "request is ignored.";
      return;
    }
    GetNextId(base::BindOnce(&DownloadManagerImpl::CreateNewDownloadItemToStart,
                             weak_factory_.GetWeakPtr(), std::move(info),
                             std::move(on_started), std::move(callback)));
  }
}

void DownloadManagerImpl::CreateNewDownloadItemToStart(
    std::unique_ptr<download::DownloadCreateInfo> info,
    download::DownloadUrlParameters::OnStartedCallback on_started,
    download::InProgressDownloadManager::StartDownloadItemCallback callback,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  download::DownloadItemImpl* download = CreateActiveItem(id, *info);
  content::devtools_instrumentation::WillBeginDownload(info.get(), download);
  std::move(callback).Run(std::move(info), download,
                          should_persist_new_download_);
  if (download) {
    // For new downloads, we notify here, rather than earlier, so that
    // the download_file is bound to download and all the usual
    // setters (e.g. Cancel) work.
    for (auto& observer : observers_)
      observer.OnDownloadCreated(this, download);
    OnNewDownloadCreated(download);
  }

  OnDownloadStarted(download, std::move(on_started));
}

void DownloadManagerImpl::BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

download::QuarantineConnectionCallback
DownloadManagerImpl::GetQuarantineConnectionCallback() {
  if (!delegate_)
    return base::NullCallback();

  return delegate_->GetQuarantineConnectionCallback();
}

void DownloadManagerImpl::StartDownload(
    std::unique_ptr<download::DownloadCreateInfo> info,
    std::unique_ptr<download::InputStream> stream,
    download::DownloadUrlParameters::OnStartedCallback on_started) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);
  in_progress_manager_->StartDownload(
      std::move(info), std::move(stream),
      download::URLLoaderFactoryProvider::GetNullPtr(), base::DoNothing(),
      std::move(on_started));
}

void DownloadManagerImpl::CheckForHistoryFilesRemoval() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& it : downloads_) {
    download::DownloadItemImpl* item = it.second.get();
    CheckForFileRemoval(item);
  }
}

void DownloadManagerImpl::OnHistoryQueryComplete(
    base::OnceClosure load_history_downloads_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!in_progress_cache_initialized_)
    load_history_downloads_cb_ = std::move(load_history_downloads_cb);
  else
    std::move(load_history_downloads_cb).Run();
}

void DownloadManagerImpl::CheckForFileRemoval(
    download::DownloadItemImpl* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((download_item->GetState() != download::DownloadItem::COMPLETE) ||
      download_item->GetFileExternallyRemoved()) {
    return;
  }

  // Check whether an task is already queued or running for the current download
  // and skip this check if it is the case.
  if (!pending_disk_access_query_.insert(download_item->GetId()).second)
    return;

  base::PostTaskAndReplyWithResult(
      disk_access_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::PathExists, download_item->GetTargetFilePath()),
      base::BindOnce(&DownloadManagerImpl::OnFileExistenceChecked,
                     weak_factory_.GetWeakPtr(), download_item->GetId()));
}

void DownloadManagerImpl::OnFileExistenceChecked(uint32_t download_id,
                                                 bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Remove the pending check flag for this download to allow new requests.
  pending_disk_access_query_.erase(download_id);

  if (!result) {  // File does not exist.
    if (base::Contains(downloads_, download_id))
      downloads_[download_id]->OnDownloadedFileRemoved();
  }
}

std::string DownloadManagerImpl::GetApplicationClientIdForFileScanning() const {
  if (delegate_)
    return delegate_->ApplicationClientIdForFileScanning();
  return std::string();
}

BrowserContext* DownloadManagerImpl::GetBrowserContext() {
  return browser_context_;
}

void DownloadManagerImpl::CreateSavePackageDownloadItem(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    int render_process_id,
    int render_frame_id,
    download::DownloadJob::CancelRequestCallback cancel_request_callback,
    DownloadItemImplCreated item_created) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetNextId(base::BindOnce(
      &DownloadManagerImpl::CreateSavePackageDownloadItemWithId,
      weak_factory_.GetWeakPtr(), main_file_path, page_url, mime_type,
      render_process_id, render_frame_id, std::move(cancel_request_callback),
      std::move(item_created)));
}

void DownloadManagerImpl::CreateSavePackageDownloadItemWithId(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    int render_process_id,
    int render_frame_id,
    download::DownloadJob::CancelRequestCallback cancel_request_callback,
    DownloadItemImplCreated item_created,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(download::DownloadItem::kInvalidId, id);
  DCHECK(!base::Contains(downloads_, id));

  download::DownloadItemImpl* download_item = item_factory_->CreateSavePageItem(
      this, id, main_file_path, page_url, mime_type,
      std::move(cancel_request_callback));
  DownloadItemUtils::AttachInfo(download_item, GetBrowserContext(),
                                WebContentsImpl::FromRenderFrameHostID(
                                    render_process_id, render_frame_id));
  OnDownloadCreated(base::WrapUnique(download_item));
  if (!item_created.is_null())
    std::move(item_created).Run(download_item);
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    std::unique_ptr<download::DownloadUrlParameters> params,
    const GURL& site_url) {
  BeginDownloadInternal(std::move(params),
                        nullptr /* blob_url_loader_factory */, false, site_url);
}

void DownloadManagerImpl::SetDownloadItemFactoryForTesting(
    std::unique_ptr<download::DownloadItemFactory> item_factory) {
  item_factory_ = std::move(item_factory);
}

void DownloadManagerImpl::SetDownloadFileFactoryForTesting(
    std::unique_ptr<download::DownloadFileFactory> file_factory) {
  in_progress_manager_->set_file_factory(std::move(file_factory));
}

download::DownloadFileFactory*
DownloadManagerImpl::GetDownloadFileFactoryForTesting() {
  return in_progress_manager_->file_factory();
}

void DownloadManagerImpl::DownloadRemoved(
    download::DownloadItemImpl* download) {
  if (!download)
    return;

  downloads_by_guid_.erase(download->GetGuid());
  downloads_.erase(download->GetId());
}

void DownloadManagerImpl::DownloadInterrupted(
    download::DownloadItemImpl* download) {
  WebContents* web_contents = DownloadItemUtils::GetWebContents(download);
  if (!web_contents) {
    download::RecordDownloadCountWithSource(
        download::INTERRUPTED_WITHOUT_WEBCONTENTS,
        download->GetDownloadSource());
  }
}

bool DownloadManagerImpl::IsOffTheRecord() const {
  return browser_context_->IsOffTheRecord();
}

void DownloadManagerImpl::ReportBytesWasted(
    download::DownloadItemImpl* download) {
  in_progress_manager_->ReportBytesWasted(download);
}

void DownloadManagerImpl::InterceptNavigation(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::vector<GURL> url_chain,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    net::CertStatus cert_status,
    int frame_tree_node_id,
    bool from_download_cross_origin_redirect) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!delegate_) {
    DropDownload();
    return;
  }

  const GURL& url = resource_request->url;
  const std::string& method = resource_request->method;
  base::Optional<url::Origin> request_initiator =
      resource_request->request_initiator;

  WebContents::Getter web_contents_getter =
      base::BindRepeating(WebContents::FromFrameTreeNodeId, frame_tree_node_id);

  base::OnceCallback<void(bool /* download allowed */)>
      on_download_checks_done = base::BindOnce(
          &DownloadManagerImpl::InterceptNavigationOnChecksComplete,
          weak_factory_.GetWeakPtr(), frame_tree_node_id,
          std::move(resource_request), std::move(url_chain), cert_status,
          std::move(response_head), std::move(response_body),
          std::move(url_loader_client_endpoints));

  delegate_->CheckDownloadAllowed(
      std::move(web_contents_getter), url, method, std::move(request_initiator),
      from_download_cross_origin_redirect, false /*content_initiated*/,
      std::move(on_download_checks_done));
}

int DownloadManagerImpl::RemoveDownloadsByURLAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  int count = 0;
  auto it = downloads_.begin();
  while (it != downloads_.end()) {
    download::DownloadItemImpl* download = it->second.get();

    // Increment done here to protect against invalidation below.
    ++it;

    if (download->GetState() != download::DownloadItem::IN_PROGRESS &&
        url_filter.Run(download->GetURL()) &&
        download->GetStartTime() >= remove_begin &&
        (remove_end.is_null() || download->GetStartTime() < remove_end)) {
      download->Remove();
      count++;
    }
  }
  return count;
}

bool DownloadManagerImpl::CanDownload(
    download::DownloadUrlParameters* parameters) {
  return true;
}

void DownloadManagerImpl::DownloadUrl(
    std::unique_ptr<download::DownloadUrlParameters> params) {
  DownloadUrl(std::move(params), nullptr /* blob_url_loader_factory */);
}

void DownloadManagerImpl::DownloadUrl(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  if (params->post_id() >= 0) {
    // Check this here so that the traceback is more useful.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
  }

  if (delegate_)
    delegate_->SanitizeDownloadParameters(params.get());

  download::RecordDownloadCountWithSource(
      download::DownloadCountTypes::DOWNLOAD_TRIGGERED_COUNT,
      params->download_source());
  auto* rfh = RenderFrameHost::FromID(params->render_process_host_id(),
                                      params->render_frame_host_routing_id());
  if (rfh)
    params->set_frame_tree_node_id(rfh->GetFrameTreeNodeId());
  BeginDownloadInternal(std::move(params), std::move(blob_url_loader_factory),
                        true,
                        rfh ? rfh->GetSiteInstance()->GetSiteURL() : GURL());
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

download::DownloadItem* DownloadManagerImpl::CreateDownloadItem(
    const std::string& guid,
    uint32_t id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_refererr_url,
    const base::Optional<url::Origin>& request_initiator,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    const std::string& hash,
    download::DownloadItem::DownloadState state,
    download::DownloadDangerType danger_type,
    download::DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<download::DownloadItem::ReceivedSlice>& received_slices) {
  // Retrive the in-progress download if it exists. Notice that this also
  // removes it from |in_progress_downloads_|.
  auto in_progress_download = RetrieveInProgressDownload(id);

  // Return null to clear cancelled or non-resumable download.
  if (cleared_download_guids_on_startup_.find(guid) !=
      cleared_download_guids_on_startup_.end()) {
    return nullptr;
  }

  if (url_chain.empty() ||
      ShouldClearDownloadFromDB(url_chain.back(), state, interrupt_reason,
                                start_time)) {
    DeleteDownloadedFileOnUIThread(current_path);
    return nullptr;
  }

  auto item = base::WrapUnique(item_factory_->CreatePersistedItem(
      this, guid, id, current_path, target_path, url_chain, referrer_url,
      site_url, tab_url, tab_refererr_url, request_initiator, mime_type,
      original_mime_type, start_time, end_time, etag, last_modified,
      received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
      opened, last_access_time, transient, received_slices));
  if (in_progress_download) {
    // If a download is in both history DB and in-progress DB, we should
    // be able to remove the in-progress entry if the following 2 conditions
    // are both met:
    // 1. The download state in the history DB is a terminal state.
    // 2. The download is not currently in progress.
    // The situation could happen when browser crashes when download just
    // reaches a terminal state. If the download is already in progress, we
    // should wait for it to complete so that both DBs will be updated
    // afterwards.
    if (item->IsDone() && in_progress_download->GetState() !=
                              download::DownloadItem::IN_PROGRESS) {
      in_progress_manager_->RemoveInProgressDownload(guid);
    } else {
      // If one of the conditions are not met, use the in-progress download
      // entry.
      // TODO(qinmin): return nullptr so that the history DB will delete
      // the download.
      item = std::move(in_progress_download);
      item->SetDelegate(this);
    }
  }
#if defined(OS_ANDROID)
  if (target_path.IsContentUri()) {
    base::FilePath display_name =
        in_progress_manager_->GetDownloadDisplayName(target_path);
    if (!display_name.empty())
      item->SetDisplayName(display_name);
    else
      return nullptr;
  }
#endif
  download::DownloadItemImpl* download = item.get();
  DownloadItemUtils::AttachInfo(download, GetBrowserContext(), nullptr);
  OnDownloadCreated(std::move(item));
  return download;
}

void DownloadManagerImpl::OnDownloadCreated(
    std::unique_ptr<download::DownloadItemImpl> download) {
  DCHECK(!base::Contains(downloads_, download->GetId()));
  DCHECK(!base::Contains(downloads_by_guid_, download->GetGuid()));
  download::DownloadItemImpl* item = download.get();
  downloads_[item->GetId()] = std::move(download);
  downloads_by_guid_[item->GetGuid()] = item;
  for (auto& observer : observers_)
    observer.OnDownloadCreated(this, item);
  OnNewDownloadCreated(item);
  DVLOG(20) << __func__ << "() download = " << item->DebugString(true);
}

void DownloadManagerImpl::PostInitialization(
    DownloadInitializationDependency dependency) {
  // If initialization has occurred (ie. in tests), skip post init steps.
  if (initialized_)
    return;

  switch (dependency) {
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB:
      history_db_initialized_ = true;
      break;
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_IN_PROGRESS_CACHE:
      in_progress_cache_initialized_ = true;
      // Post a task to load downloads from history db.
      if (load_history_downloads_cb_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, std::move(load_history_downloads_cb_));
      }
      break;
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_NONE:
    default:
      NOTREACHED();
      break;
  }

  // Download manager is only initialized if both history db and in progress
  // cache are initialized.
  bool history_loaded = history_db_initialized_ || IsOffTheRecord();
  if (!history_loaded || !in_progress_cache_initialized_)
    return;

  for (const auto& guid : cleared_download_guids_on_startup_) {
    in_progress_manager_->RemoveInProgressDownload(guid);
  }

  if (in_progress_downloads_.empty()) {
    OnDownloadManagerInitialized();
  } else {
    GetNextId(base::BindOnce(&DownloadManagerImpl::ImportInProgressDownloads,
                             weak_factory_.GetWeakPtr()));
  }
}

void DownloadManagerImpl::ImportInProgressDownloads(uint32_t id) {
  auto download = in_progress_downloads_.begin();
  while (download != in_progress_downloads_.end()) {
    auto item = std::move(*download);
    // If the in-progress download doesn't have an ID, generate new IDs for it.
    if (item->GetId() == download::DownloadItem::kInvalidId) {
      item->SetDownloadId(id++);
      next_download_id_++;
      if (!should_persist_new_download_)
        in_progress_manager_->RemoveInProgressDownload(item->GetGuid());
    }
    item->SetDelegate(this);
    DownloadItemUtils::AttachInfo(item.get(), GetBrowserContext(), nullptr);
    download = in_progress_downloads_.erase(download);
    OnDownloadCreated(std::move(item));
  }
  OnDownloadManagerInitialized();
}

void DownloadManagerImpl::OnDownloadManagerInitialized() {
  OnInitialized();
  in_progress_manager_->OnAllInprogressDownloadsLoaded();
  for (auto& observer : observers_)
    observer.OnManagerInitialized();
  size_t size = 0;
  for (const auto& it : downloads_)
    size += it.second->GetApproximateMemoryUsage();
  if (!IsOffTheRecord() && size > 0)
    download::RecordDownloadManagerMemoryUsage(size);
}

bool DownloadManagerImpl::IsManagerInitialized() {
  return initialized_;
}

int DownloadManagerImpl::InProgressCount() {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == download::DownloadItem::IN_PROGRESS)
      ++count;
  }
  return count;
}

int DownloadManagerImpl::NonMaliciousInProgressCount() {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == download::DownloadItem::IN_PROGRESS &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED &&
        it.second->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS) {
      ++count;
    }
  }
  return count;
}

download::DownloadItem* DownloadManagerImpl::GetDownload(uint32_t download_id) {
  return base::Contains(downloads_, download_id) ? downloads_[download_id].get()
                                                 : nullptr;
}

download::DownloadItem* DownloadManagerImpl::GetDownloadByGuid(
    const std::string& guid) {
  if (!in_progress_downloads_.empty()) {
    for (const auto& it : in_progress_downloads_) {
      if (it->GetGuid() == guid)
        return it.get();
    }
  }
  return base::Contains(downloads_by_guid_, guid) ? downloads_by_guid_[guid]
                                                  : nullptr;
}

void DownloadManagerImpl::GetAllDownloads(
    download::SimpleDownloadManager::DownloadVector* downloads) {
  for (const auto& it : downloads_)
    downloads->push_back(it.second.get());
}

void DownloadManagerImpl::GetUninitializedActiveDownloadsIfAny(
    download::SimpleDownloadManager::DownloadVector* downloads) {
  for (const auto& it : in_progress_downloads_)
    downloads->push_back(it.get());
}

void DownloadManagerImpl::OpenDownload(download::DownloadItemImpl* download) {
  int num_unopened = 0;
  for (const auto& it : downloads_) {
    download::DownloadItemImpl* item = it.second.get();
    if ((item->GetState() == download::DownloadItem::COMPLETE) &&
        !item->GetOpened())
      ++num_unopened;
  }
  download::RecordOpensOutstanding(num_unopened);

  if (delegate_)
    delegate_->OpenDownload(download);
}

void DownloadManagerImpl::ShowDownloadInShell(
    download::DownloadItemImpl* download) {
  if (delegate_)
    delegate_->ShowDownloadInShell(download);
}

void DownloadManagerImpl::DropDownload() {
  download::RecordDownloadCount(download::DOWNLOAD_DROPPED_COUNT);
  for (auto& observer : observers_)
    observer.OnDownloadDropped(this);
}

void DownloadManagerImpl::InterceptNavigationOnChecksComplete(
    int frame_tree_node_id,
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::vector<GURL> url_chain,
    net::CertStatus cert_status,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_download_allowed) {
  if (!is_download_allowed) {
    DropDownload();
    return;
  }

  int render_process_id = -1;
  int render_frame_id = -1;
  GURL site_url, tab_url, tab_referrer_url;
  RenderFrameHost* render_frame_host = nullptr;
  auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (ftn) {
    render_frame_host = ftn->current_frame_host();
    if (render_frame_host) {
      render_process_id = render_frame_host->GetProcess()->GetID();
      render_frame_id = render_frame_host->GetRoutingID();
    }
    auto* web_contents = WebContentsImpl::FromFrameTreeNode(ftn);
    DCHECK(web_contents);
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    if (entry) {
      tab_url = entry->GetURL();
      tab_referrer_url = entry->GetReferrer().url;
    }
    RenderFrameHost* opener = web_contents->GetOpener();
    if (opener) {
      RecordDownloadOpenerType(render_frame_host, opener);
    }
  }
  StoragePartitionImpl* storage_partition =
      GetStoragePartition(browser_context_, render_process_id, render_frame_id);
  in_progress_manager_->InterceptDownloadFromNavigation(
      std::move(resource_request), render_process_id, render_frame_id, site_url,
      tab_url, tab_referrer_url, std::move(url_chain), std::move(cert_status),
      std::move(response_head), std::move(response_body),
      std::move(url_loader_client_endpoints),
      CreatePendingSharedURLLoaderFactory(storage_partition, render_frame_host,
                                          false));
}

void DownloadManagerImpl::BeginResourceDownloadOnChecksComplete(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool is_new_download,
    const GURL& site_url,
    bool is_download_allowed) {
  if (!is_download_allowed) {
    DropDownload();
    return;
  }

  GURL tab_url, tab_referrer_url;
  auto* rfh = RenderFrameHost::FromID(params->render_process_host_id(),
                                      params->render_frame_host_routing_id());
  if (rfh) {
    auto* web_contents = WebContents::FromRenderFrameHost(rfh);
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    if (entry) {
      tab_url = entry->GetURL();
      tab_referrer_url = entry->GetReferrer().url;
    }
  }

  DCHECK_EQ(params->url().SchemeIsBlob(), bool{blob_url_loader_factory});
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory;
  if (blob_url_loader_factory) {
    DCHECK(params->url().SchemeIsBlob());
    pending_url_loader_factory = blob_url_loader_factory->Clone();
  } else if (params->url().SchemeIsFile()) {
    pending_url_loader_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            FileURLLoaderFactory::Create(
                browser_context_->GetPath(),
                browser_context_->GetSharedCorsOriginAccessList(),
                // USER_VISIBLE because download should progress
                // even when there is high priority work to do.
                base::TaskPriority::USER_VISIBLE));
  } else if (rfh && params->url().SchemeIs(content::kChromeUIScheme)) {
    pending_url_loader_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            CreateWebUIURLLoaderFactory(rfh, params->url().scheme(),
                                        base::flat_set<std::string>()));
  } else if (rfh && params->url().SchemeIsFileSystem()) {
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                       site_url));

    pending_url_loader_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            CreateFileSystemURLLoaderFactory(
                rfh->GetProcess()->GetID(), rfh->GetFrameTreeNodeId(),
                storage_partition->GetFileSystemContext(),
                storage_partition->GetPartitionDomain()));
  } else if (params->url().SchemeIs(url::kDataScheme)) {
    pending_url_loader_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            DataURLLoaderFactory::CreateForOneSpecificUrl(params->url()));
  } else if (rfh && !blink::network_utils::IsURLHandledByNetworkService(
                        params->url())) {
    ContentBrowserClient::NonNetworkURLLoaderFactoryDeprecatedMap
        non_network_uniquely_owned_factories;
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap
        non_network_url_loader_factories;

    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            params->render_process_host_id(),
            params->render_frame_host_routing_id(),
            &non_network_uniquely_owned_factories,
            &non_network_url_loader_factories);
    auto it = non_network_uniquely_owned_factories.find(params->url().scheme());
    if (it != non_network_uniquely_owned_factories.end()) {
      pending_url_loader_factory =
          CreatePendingSharedURLLoaderFactoryFromURLLoaderFactory(
              std::move(it->second));
    } else {
      auto it2 = non_network_url_loader_factories.find(params->url().scheme());
      if (it2 != non_network_url_loader_factories.end()) {
        pending_url_loader_factory =
            std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
                std::move(it2->second));
      } else {
        DLOG(ERROR) << "No URLLoaderFactory found to download "
                    << params->url();
        return;
      }
    }
  } else {
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                       site_url));
    pending_url_loader_factory =
        CreatePendingSharedURLLoaderFactory(storage_partition, rfh, true);
  }

  in_progress_manager_->BeginDownload(
      std::move(params), std::move(pending_url_loader_factory), is_new_download,
      site_url, tab_url, tab_referrer_url);
}

void DownloadManagerImpl::BeginDownloadInternal(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool is_new_download,
    const GURL& site_url) {
  // Check if the renderer is permitted to request the requested URL.
  if (params->render_process_host_id() >= 0 &&
      !DownloadRequestUtils::IsURLSafe(params->render_process_host_id(),
                                       params->url())) {
    CreateInterruptedDownload(
        std::move(params),
        download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
        weak_factory_.GetWeakPtr());
    return;
  }

  // Ideally everywhere a blob: URL is downloaded a URLLoaderFactory for that
  // blob URL is also passed, but since that isn't always the case, create
  // a new factory if we don't have one already.
  if (!blob_url_loader_factory && params->url().SchemeIsBlob()) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        BrowserContext::GetStoragePartitionForSite(browser_context_, site_url),
        params->url());
  }

  auto* rfh = RenderFrameHost::FromID(params->render_process_host_id(),
                                      params->render_frame_host_routing_id());
  bool content_initiated = params->content_initiated();
  // If it's from the web, we don't trust it, so we push the throttle on.
  if (rfh && content_initiated) {
    WebContents::Getter web_contents_getter = base::BindRepeating(
        WebContents::FromFrameTreeNodeId, rfh->GetFrameTreeNodeId());
    const GURL& url = params->url();
    const std::string& method = params->method();
    base::Optional<url::Origin> initiator = params->initiator();
    base::OnceCallback<void(bool /* download allowed */)>
        on_can_download_checks_done = base::BindOnce(
            &DownloadManagerImpl::BeginResourceDownloadOnChecksComplete,
            weak_factory_.GetWeakPtr(), std::move(params),
            std::move(blob_url_loader_factory), is_new_download, site_url);
    if (delegate_) {
      delegate_->CheckDownloadAllowed(
          std::move(web_contents_getter), url, method, std::move(initiator),
          false /* from_download_cross_origin_redirect */, content_initiated,
          std::move(on_can_download_checks_done));
    }
    return;
  }

  BeginResourceDownloadOnChecksComplete(
      std::move(params), std::move(blob_url_loader_factory), is_new_download,
      site_url, rfh ? !content_initiated : true);
}

bool DownloadManagerImpl::IsNextIdInitialized() const {
  return is_history_download_id_retrieved_ && in_progress_cache_initialized_;
}

bool DownloadManagerImpl::ShouldClearDownloadFromDB(
    const GURL& url,
    download::DownloadItem::DownloadState state,
    download::DownloadInterruptReason reason,
    const base::Time& start_time) {
  if (!base::FeatureList::IsEnabled(
          download::features::kDeleteExpiredDownloads)) {
    return false;
  }

  // Use system time to determine if the download is expired. Manually setting
  // the system time can affect this.
  bool expired = base::Time::Now() - start_time >=
                 download::GetExpiredDownloadDeleteTime();
  if (state == download::DownloadItem::CANCELLED && expired)
    return true;

  if (reason != download::DOWNLOAD_INTERRUPT_REASON_NONE &&
      state == download::DownloadItem::INTERRUPTED && expired) {
    return true;
  }

  return false;
}

std::unique_ptr<download::DownloadItemImpl>
DownloadManagerImpl::RetrieveInProgressDownload(uint32_t id) {
  // In case the history DB has some invalid IDs, skip them.
  if (id == download::DownloadItem::kInvalidId)
    return nullptr;

  for (auto it = in_progress_downloads_.begin();
       it != in_progress_downloads_.end(); ++it) {
    if ((*it)->GetId() == id) {
      auto download = std::move(*it);
      in_progress_downloads_.erase(it);
      return download;
    }
  }

  return nullptr;
}

}  // namespace content
