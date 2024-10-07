// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
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
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_target_info.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/input_stream.h"
#include "components/download/public/common/url_download_handler_factory.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/download/embedder_download_data.pb.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/url_util.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/http/http_content_disposition.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
namespace {
#if BUILDFLAG(IS_ANDROID)
// PDF MIME type.
constexpr char kPdfMimeType[] = "application/pdf";
#endif  // BUILDFLAG(IS_ANDROID)

void DeleteDownloadedFileOnUIThread(const base::FilePath& file_path) {
  if (!file_path.empty()) {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&download::DeleteDownloadedFile),
                       file_path));
  }
}

StoragePartitionImpl* GetStoragePartitionForConfig(
    BrowserContext* context,
    const StoragePartitionConfig& storage_partition_config) {
  return static_cast<StoragePartitionImpl*>(
      context->GetStoragePartition(storage_partition_config));
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
      const std::string& serialized_embedder_download_data,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::optional<url::Origin>& request_initiator,
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
        referrer_url, serialized_embedder_download_data, tab_url,
        tab_refererr_url, request_initiator, mime_type, original_mime_type,
        start_time, end_time, etag, last_modified, received_bytes, total_bytes,
        auto_resume_count, hash, state, danger_type, interrupt_reason,
        false /* paused */, false /* allow_metered */, opened, last_access_time,
        transient, received_slices, download::kInvalidRange,
        download::kInvalidRange, nullptr /* download_entry */);
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

std::unique_ptr<network::PendingSharedURLLoaderFactory>
CreatePendingSharedURLLoaderFactory(StoragePartitionImpl* storage_partition,
                                    RenderFrameHost* rfh) {
  network::URLLoaderFactoryBuilder factory_builder;

  if (rfh) {
    // Allow DevTools to potentially inject itself into `factory_builder`.
    devtools_instrumentation::WillCreateURLLoaderFactoryParams::ForFrame(
        static_cast<RenderFrameHostImpl*>(rfh))
        .Run(/*is_navigation=*/true,
             /*is_download=*/true, factory_builder,
             nullptr /* factory_override */);

    // Also allow the Content embedder to inject itself if it wants to.
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        rfh->GetSiteInstance()->GetBrowserContext(), rfh,
        rfh->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kDownload, url::Origin(),
        net::IsolationInfo(), /*navigation_id=*/std::nullopt,
        ukm::kInvalidSourceIdObj, factory_builder, /*header_client=*/nullptr,
        /*bypass_redirect_checks=*/nullptr, /*disable_secure_dns=*/nullptr,
        /*factory_override=*/nullptr,
        /*navigation_response_task_runner=*/nullptr);
  }

  return std::make_unique<network::PendingSharedURLLoaderFactoryWithBuilder>(
      std::move(factory_builder),
      storage_partition->GetURLLoaderFactoryForBrowserProcessIOThread());
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
          browser_context_->RetrieveInProgressDownloadManager()),
      next_download_id_(download::DownloadItem::kInvalidId),
      is_history_download_id_retrieved_(false),
      should_persist_new_download_(false),
      disk_access_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK(browser_context);

  download::SetIOTaskRunner(GetIOThreadTaskRunner({}));

  if (!in_progress_manager_) {
    auto* proto_db_provider = browser_context->GetDefaultStoragePartition()
                                  ->GetProtoDatabaseProvider();
    in_progress_manager_ =
        std::make_unique<download::InProgressDownloadManager>(
            this, base::FilePath(), proto_db_provider,
            base::BindRepeating(&network::IsUrlPotentiallyTrustworthy),
            base::BindRepeating(&DownloadRequestUtils::IsURLSafe),
            /*wake_lock_provider_binder=*/base::NullCallback());
  } else {
    in_progress_manager_->SetDelegate(this);
    in_progress_manager_->set_download_start_observer(nullptr);
    in_progress_manager_->set_is_origin_secure_cb(
        base::BindRepeating(&network::IsUrlPotentiallyTrustworthy));
  }
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

download::DownloadItemImpl* DownloadManagerImpl::CreateActiveItem(
    uint32_t id,
    const download::DownloadCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::Contains(downloads_by_guid_, info.guid))
    return nullptr;

  download::DownloadItemImpl* download =
      item_factory_->CreateActiveItem(this, id, info);

  downloads_[id] = base::WrapUnique(download);
  downloads_by_guid_[download->GetGuid()] = download;
  GlobalRenderFrameHostId global_id(info.render_process_id,
                                    info.render_frame_id);
  DownloadItemUtils::AttachInfo(
      download, GetBrowserContext(),
      WebContentsImpl::FromRenderFrameHostID(global_id), global_id);
  if (delegate_) {
    delegate_->AttachExtraInfo(download);
#if BUILDFLAG(IS_ANDROID)
    download->set_is_from_external_app(delegate_->IsFromExternalApp(download));
#endif  // BUILDFLAG(IS_ANDROID)
  }

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
          base::BindOnce(&DownloadManagerImpl::OnHistoryNextIdRetrieved,
                         weak_factory_.GetWeakPtr()));
    } else {
      OnHistoryNextIdRetrieved(download::DownloadItem::kInvalidId);
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

std::string
DownloadManagerImpl::StoragePartitionConfigToSerializedEmbedderDownloadData(
    const StoragePartitionConfig& storage_partition_config) {
  proto::EmbedderDownloadData embedder_download_data;
  proto::StoragePartitionConfig* config_proto =
      embedder_download_data.mutable_storage_partition_config();
  config_proto->set_partition_domain(
      storage_partition_config.partition_domain());
  config_proto->set_partition_name(storage_partition_config.partition_name());
  config_proto->set_in_memory(storage_partition_config.in_memory());

  switch (
      storage_partition_config.fallback_to_partition_domain_for_blob_urls()) {
    case StoragePartitionConfig::FallbackMode::kNone:
      config_proto->set_fallback_mode(
          proto::StoragePartitionConfig_FallbackMode_kNone);
      break;
    case StoragePartitionConfig::FallbackMode::kFallbackPartitionOnDisk:
      config_proto->set_fallback_mode(
          proto::StoragePartitionConfig_FallbackMode_kPartitionOnDisk);
      break;
    case StoragePartitionConfig::FallbackMode::kFallbackPartitionInMemory:
      config_proto->set_fallback_mode(
          proto::StoragePartitionConfig_FallbackMode_kPartitionInMemory);
      break;
    default:
      config_proto->set_fallback_mode(
          proto::StoragePartitionConfig_FallbackMode_kNone);
  }
  return embedder_download_data.SerializeAsString();
}

StoragePartitionConfig
DownloadManagerImpl::SerializedEmbedderDownloadDataToStoragePartitionConfig(
    const std::string& serialized_embedder_download_data) {
  proto::EmbedderDownloadData embedder_download_data;
  bool success =
      embedder_download_data.ParseFromString(serialized_embedder_download_data);
  DCHECK(success);

  StoragePartitionConfig::FallbackMode fallback_mode;
  switch (embedder_download_data.storage_partition_config().fallback_mode()) {
    case proto::StoragePartitionConfig_FallbackMode_kNone:
      fallback_mode = StoragePartitionConfig::FallbackMode::kNone;
      break;
    case proto::StoragePartitionConfig_FallbackMode_kPartitionOnDisk:
      fallback_mode =
          StoragePartitionConfig::FallbackMode::kFallbackPartitionOnDisk;
      break;
    case proto::StoragePartitionConfig_FallbackMode_kPartitionInMemory:
      fallback_mode =
          StoragePartitionConfig::FallbackMode::kFallbackPartitionInMemory;
      break;
    default:
      fallback_mode = StoragePartitionConfig::FallbackMode::kNone;
      break;
  }

  StoragePartitionConfig config;
  if (embedder_download_data.storage_partition_config()
          .partition_domain()
          .empty()) {
    config = StoragePartitionConfig::CreateDefault(browser_context_);
  } else {
    config = StoragePartitionConfig::Create(
        browser_context_,
        embedder_download_data.storage_partition_config().partition_domain(),
        embedder_download_data.storage_partition_config().partition_name(),
        embedder_download_data.storage_partition_config().in_memory());
  }
  config.set_fallback_to_partition_domain_for_blob_urls(fallback_mode);
  return config;
}

void DownloadManagerImpl::OnHistoryNextIdRetrieved(uint32_t next_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_history_download_id_retrieved_ = true;
  if (next_id == download::DownloadItem::kInvalidId)
    next_id++;
  else
    should_persist_new_download_ = true;
  SetNextId(next_id);
}

StoragePartitionConfig DownloadManagerImpl::GetStoragePartitionConfigForSiteUrl(
    const GURL& site_url) {
  return SiteInfo::GetStoragePartitionConfigForUrl(browser_context_, site_url);
}

void DownloadManagerImpl::DetermineDownloadTarget(
    download::DownloadItemImpl* item,
    download::DownloadTargetCallback callback) {
  if (!delegate_ || !delegate_->DetermineDownloadTarget(item, &callback)) {
    base::FilePath target_path = item->GetForcedFilePath();
    // TODO(asanka): Determine a useful path if |target_path| is empty.
    download::DownloadTargetInfo target_info;
    target_info.target_path = target_path;
    target_info.intermediate_path = target_path;

    std::move(callback).Run(std::move(target_info));
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
  for (const auto& it : downloads_by_guid_) {
    download::DownloadItemImpl* download = it.second;
    if (download != nullptr &&
        download->GetState() == download::DownloadItem::IN_PROGRESS) {
      download->Cancel(false);
      if (delegate_) {
        delegate_->OnDownloadCanceledAtShutdown(download);
      }
    }
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

      // Ensure the method is called from an active document.
      // If inactive documents start download, it can be a security risk.
      // Call ReceiveBadMessage to terminate such a renderer.
      // TODO(crbug.com/40201479): confirm if fenced frames are allowed to start
      // downloads.
      if (!RenderFrameHost::FromID(info.render_process_id, info.render_frame_id)
               ->IsActive()) {
        bad_message::ReceivedBadMessage(
            info.render_process_id,
            bad_message::RFH_INTERECEPT_DOWNLOAD_WHILE_INACTIVE);
        return false;
      }

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
    std::move(callback).Run(std::move(info), download, base::FilePath(),
                            should_persist_new_download_);
    OnDownloadStarted(download, std::move(on_started));
  } else {
    // If the download already in system, it can only be resumed.
    if (!info->guid.empty() && GetDownloadByGuid(info->guid)) {
      LOG(WARNING) << "A download with the same GUID already exists, the new "
                      "request is ignored.";
      return;
    }
    GetNextId(base::BindOnce(&DownloadManagerImpl::OnNewDownloadIdRetrieved,
                             weak_factory_.GetWeakPtr(), std::move(info),
                             std::move(on_started), std::move(callback)));
  }
}

void DownloadManagerImpl::OnNewDownloadIdRetrieved(
    std::unique_ptr<download::DownloadCreateInfo> info,
    download::DownloadUrlParameters::OnStartedCallback on_started,
    download::InProgressDownloadManager::StartDownloadItemCallback callback,
    uint32_t id) {
#if BUILDFLAG(IS_ANDROID)
  if (info->transient && !info->is_must_download &&
      delegate_->ShouldOpenPdfInline() &&
      base::EqualsCaseInsensitiveASCII(info->mime_type, kPdfMimeType)) {
    if (IsOffTheRecord()) {
      info->save_info->use_in_memory_file = true;
    } else {
      for (const auto& iter : downloads_by_guid_) {
        download::DownloadItem* item = iter.second;
        if (item->GetFileExternallyRemoved() ||
            item->GetState() != download::DownloadItem::COMPLETE) {
          continue;
        }

        if (item->GetMimeType() != kPdfMimeType ||
            item->GetUrlChain() != info->url_chain) {
          continue;
        }

        if (!item->IsTransient() || item->IsMustDownload()) {
          continue;
        }

        disk_access_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&base::PathExists, item->GetTargetFilePath()),
            base::BindOnce(&DownloadManagerImpl::CreateNewDownloadItemToStart,
                           weak_factory_.GetWeakPtr(), std::move(info),
                           std::move(on_started), std::move(callback), id,
                           item->GetTargetFilePath()));
        return;
      }
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
  CreateNewDownloadItemToStart(std::move(info), std::move(on_started),
                               std::move(callback), id, base::FilePath(),
                               false);
}

void DownloadManagerImpl::CreateNewDownloadItemToStart(
    std::unique_ptr<download::DownloadCreateInfo> info,
    download::DownloadUrlParameters::OnStartedCallback on_started,
    download::InProgressDownloadManager::StartDownloadItemCallback callback,
    uint32_t id,
    const base::FilePath& duplicate_download_file_path,
    bool duplicate_file_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  download::DownloadItemImpl* download = CreateActiveItem(id, *info);
  if (delegate_ && info->save_info) {
    info->save_info->needs_obfuscation =
        delegate_->ShouldObfuscateDownload(download);
    info->save_info->total_bytes = info->total_bytes;
  }
  content::devtools_instrumentation::WillBeginDownload(info.get(), download);
  std::move(callback).Run(
      std::move(info), download,
      duplicate_file_exists ? duplicate_download_file_path : base::FilePath(),
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

std::unique_ptr<download::DownloadItemRenameHandler>
DownloadManagerImpl::GetRenameHandlerForDownload(
    download::DownloadItemImpl* download_item) {
  if (!delegate_) {
    return nullptr;
  }

  return delegate_->GetRenameHandlerForDownload(download_item);
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
  for (const auto& it : downloads_by_guid_) {
    download::DownloadItemImpl* item = it.second;
    if (item != nullptr)
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
  if (!pending_disk_access_query_.insert(download_item->GetGuid()).second)
    return;

  disk_access_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::PathExists, download_item->GetTargetFilePath()),
      base::BindOnce(&DownloadManagerImpl::OnFileExistenceChecked,
                     weak_factory_.GetWeakPtr(), download_item->GetGuid()));
}

void DownloadManagerImpl::OnFileExistenceChecked(const std::string& guid,
                                                 bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Remove the pending check flag for this download to allow new requests.
  pending_disk_access_query_.erase(guid);

  if (!result) {  // File does not exist.
    auto it = downloads_by_guid_.find(guid);
    if (it != downloads_by_guid_.end())
      it->second->OnDownloadedFileRemoved();
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

  GlobalRenderFrameHostId global_id(render_process_id, render_frame_id);
  DownloadItemUtils::AttachInfo(
      download_item, GetBrowserContext(),
      WebContentsImpl::FromRenderFrameHostID(global_id), global_id);
  if (delegate_) {
    delegate_->AttachExtraInfo(download_item);
  }

  OnDownloadCreated(base::WrapUnique(download_item));
  if (!item_created.is_null())
    std::move(item_created).Run(download_item);
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    std::unique_ptr<download::DownloadUrlParameters> params,
    const std::string& serialized_embedder_download_data) {
  BeginDownloadInternal(std::move(params),
                        nullptr /* blob_url_loader_factory */, false,
                        serialized_embedder_download_data);
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
    FrameTreeNodeId frame_tree_node_id,
    bool from_download_cross_origin_redirect) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!delegate_) {
    DropDownload();
    return;
  }

  const GURL& url = resource_request->url;
  const std::string& method = resource_request->method;
  std::optional<url::Origin> request_initiator =
      resource_request->request_initiator;
  std::string mime_type = response_head->mime_type;
  ui::PageTransition transition_type =
      static_cast<ui::PageTransition>(resource_request->transition_type);

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
      mime_type, transition_type, std::move(on_download_checks_done));
}

int DownloadManagerImpl::RemoveDownloadsByURLAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  int count = 0;
  auto it = downloads_by_guid_.begin();
  while (it != downloads_by_guid_.end()) {
    download::DownloadItemImpl* download = it->second;

    // Increment done here to protect against invalidation below.
    ++it;

    if (download != nullptr &&
        download->GetState() != download::DownloadItem::IN_PROGRESS &&
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
  BeginDownloadInternal(std::move(params), std::move(blob_url_loader_factory),
                        /*is_new_download=*/true,
                        /*serialized_embedder_download_data=*/std::string());
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
    const StoragePartitionConfig& storage_partition_config,
    const GURL& tab_url,
    const GURL& tab_refererr_url,
    const std::optional<url::Origin>& request_initiator,
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
  // Retrieve the in-progress download if it exists. Notice that this also
  // removes it from |in_progress_downloads_|.
  auto in_progress_download = RetrieveInProgressDownload(id);

  // Return null to clear cancelled or non-resumable download.
  if (base::Contains(cleared_download_guids_on_startup_, guid)) {
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
      StoragePartitionConfigToSerializedEmbedderDownloadData(
          storage_partition_config),
      tab_url, tab_refererr_url, request_initiator, mime_type,
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
#if BUILDFLAG(IS_ANDROID)
  if (target_path.IsContentUri()) {
    base::FilePath android_display_name =
        in_progress_manager_->GetDownloadDisplayName(target_path);
    if (!android_display_name.empty())
      item->SetDisplayName(android_display_name);
    else
      return nullptr;
  }
#endif
  download::DownloadItemImpl* download = item.get();
  DownloadItemUtils::AttachInfo(download, GetBrowserContext(), nullptr,
                                GlobalRenderFrameHostId());
  if (delegate_) {
    delegate_->AttachExtraInfo(download);
  }
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
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(load_history_downloads_cb_));
      }
      break;
    case DOWNLOAD_INITIALIZATION_DEPENDENCY_NONE:
    default:
      NOTREACHED_IN_MIGRATION();
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
    DownloadItemUtils::AttachInfo(item.get(), GetBrowserContext(), nullptr,
                                  GlobalRenderFrameHostId());
    if (delegate_) {
      delegate_->AttachExtraInfo(item.get());
    }
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
}

bool DownloadManagerImpl::IsManagerInitialized() {
  return initialized_;
}

int DownloadManagerImpl::InProgressCount() {
  int count = 0;
  for (const auto& it : downloads_by_guid_) {
    if (it.second != nullptr &&
        it.second->GetState() == download::DownloadItem::IN_PROGRESS)
      ++count;
  }
  return count;
}

int DownloadManagerImpl::BlockingShutdownCount() {
  int count = 0;
  for (const auto& it : downloads_by_guid_) {
    download::DownloadItemImpl* download = it.second;
    if (download != nullptr) {
      if (download->IsTransient())
        continue;
      if (download->GetState() == download::DownloadItem::IN_PROGRESS &&
          !download->IsDangerous() && !download->IsInsecure()) {
        ++count;
      }
    }
  }
  return count;
}

download::DownloadItem* DownloadManagerImpl::GetDownload(uint32_t download_id) {
  auto it = downloads_.find(download_id);
  return it != downloads_.end() ? it->second.get() : nullptr;
}

download::DownloadItem* DownloadManagerImpl::GetDownloadByGuid(
    const std::string& guid) {
  if (!in_progress_downloads_.empty()) {
    for (const auto& it : in_progress_downloads_) {
      if (it->GetGuid() == guid)
        return it.get();
    }
  }
  auto it = downloads_by_guid_.find(guid);
  return it != downloads_by_guid_.end() ? it->second : nullptr;
}

void DownloadManagerImpl::GetAllDownloads(
    download::SimpleDownloadManager::DownloadVector* downloads) {
  for (const auto& it : downloads_by_guid_)
    if (it.second != nullptr)
      downloads->push_back(it.second);
}

void DownloadManagerImpl::GetUninitializedActiveDownloadsIfAny(
    download::SimpleDownloadManager::DownloadVector* downloads) {
  for (const auto& it : in_progress_downloads_)
    downloads->push_back(it.get());
}

void DownloadManagerImpl::OpenDownload(download::DownloadItemImpl* download) {
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
    FrameTreeNodeId frame_tree_node_id,
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
  auto storage_partition_config =
      StoragePartitionConfig::CreateDefault(browser_context_);
  auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (ftn) {
    render_frame_host = ftn->current_frame_host();
    if (render_frame_host) {
      render_process_id = render_frame_host->GetProcess()->GetID();
      render_frame_id = render_frame_host->GetRoutingID();
      storage_partition_config =
          render_frame_host->GetSiteInstance()->GetStoragePartitionConfig();
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

  bool is_transient = false;
#if BUILDFLAG(IS_ANDROID)
  if (!download::IsContentDispositionAttachmentInHead(*response_head)) {
    is_transient = delegate_->ShouldOpenPdfInline() &&
                   base::EqualsCaseInsensitiveASCII(response_head->mime_type,
                                                    kPdfMimeType);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  StoragePartitionImpl* storage_partition =
      GetStoragePartitionForConfig(browser_context_, storage_partition_config);
  in_progress_manager_->InterceptDownloadFromNavigation(
      std::move(resource_request), render_process_id, render_frame_id,
      StoragePartitionConfigToSerializedEmbedderDownloadData(
          storage_partition_config),
      tab_url, tab_referrer_url, std::move(url_chain), std::move(cert_status),
      std::move(response_head), std::move(response_body),
      std::move(url_loader_client_endpoints),
      CreatePendingSharedURLLoaderFactory(storage_partition, render_frame_host),
      is_transient);
}

void DownloadManagerImpl::BeginResourceDownloadOnChecksComplete(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool is_new_download,
    const StoragePartitionConfig& storage_partition_config,
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
    StoragePartitionImpl* storage_partition = GetStoragePartitionForConfig(
        browser_context_, storage_partition_config);
    pending_url_loader_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            CreateFileSystemURLLoaderFactory(
                rfh->GetProcess()->GetID(), rfh->GetFrameTreeNodeId(),
                storage_partition->GetFileSystemContext(),
                storage_partition->GetPartitionDomain(),
                static_cast<RenderFrameHostImpl*>(rfh)->GetStorageKey()));
  } else if (params->url().SchemeIs(url::kDataScheme)) {
    pending_url_loader_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            DataURLLoaderFactory::CreateForOneSpecificUrl(params->url()));
  } else if (rfh && !network::IsURLHandledByNetworkService(params->url())) {
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap
        non_network_url_loader_factories;
    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            params->render_process_host_id(),
            params->render_frame_host_routing_id(), params->initiator(),
            &non_network_url_loader_factories);
    auto it = non_network_url_loader_factories.find(params->url().scheme());
    if (it != non_network_url_loader_factories.end()) {
      pending_url_loader_factory =
          std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
              std::move(it->second));
    } else {
      DLOG(ERROR) << "No URLLoaderFactory found to download " << params->url();
      return;
    }
  } else {
    StoragePartitionImpl* storage_partition = GetStoragePartitionForConfig(
        browser_context_, storage_partition_config);
    pending_url_loader_factory =
        CreatePendingSharedURLLoaderFactory(storage_partition, rfh);
  }

  in_progress_manager_->BeginDownload(
      std::move(params), std::move(pending_url_loader_factory), is_new_download,
      StoragePartitionConfigToSerializedEmbedderDownloadData(
          storage_partition_config),
      tab_url, tab_referrer_url);
}

void DownloadManagerImpl::BeginDownloadInternal(
    std::unique_ptr<download::DownloadUrlParameters> params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool is_new_download,
    const std::string& serialized_embedder_download_data) {
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

  auto* rfh = RenderFrameHost::FromID(params->render_process_host_id(),
                                      params->render_frame_host_routing_id());

  // Untrusted network access is revoked, download request is interrupted.
  if (rfh && rfh->IsUntrustedNetworkDisabled()) {
    // TODO(crbug.com/365033308): Create a new download interrupt reason for
    // fenced frame network revocation.
    CreateInterruptedDownload(
        std::move(params), download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
        weak_factory_.GetWeakPtr());
    return;
  }

  StoragePartitionConfig storage_partition_config;
  if (rfh && serialized_embedder_download_data.empty()) {
    storage_partition_config =
        rfh->GetSiteInstance()->GetStoragePartitionConfig();
  } else {
    storage_partition_config =
        SerializedEmbedderDownloadDataToStoragePartitionConfig(
            serialized_embedder_download_data);
  }

  // Ideally everywhere a blob: URL is downloaded a URLLoaderFactory for that
  // blob URL is also passed, but since that isn't always the case, create
  // a new factory if we don't have one already.
  if (!blob_url_loader_factory && params->url().SchemeIsBlob()) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        GetStoragePartitionForConfig(browser_context_,
                                     storage_partition_config),
        params->url());
  }

  bool content_initiated = params->content_initiated();
  if (rfh && content_initiated) {
    // Cancel downloads from non-active documents (e.g prerendered, bfcached) or
    // fenced frames.
    if (rfh->IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kBeginDownload) ||
        rfh->IsNestedWithinFencedFrame()) {
      DropDownload();
      return;
    }

    // Push the throttle on the web-initiated downloads before allowing them to
    // proceed.
    if (delegate_) {
      WebContents::Getter web_contents_getter = base::BindRepeating(
          WebContents::FromFrameTreeNodeId, rfh->GetFrameTreeNodeId());
      const GURL& url = params->url();
      const std::string& method = params->method();
      std::optional<url::Origin> initiator = params->initiator();
      base::OnceCallback<void(bool /* download allowed */)>
          on_can_download_checks_done = base::BindOnce(
              &DownloadManagerImpl::BeginResourceDownloadOnChecksComplete,
              weak_factory_.GetWeakPtr(), std::move(params),
              std::move(blob_url_loader_factory), is_new_download,
              storage_partition_config);
      delegate_->CheckDownloadAllowed(
          std::move(web_contents_getter), url, method, std::move(initiator),
          false /* from_download_cross_origin_redirect */, content_initiated,
          /* mime_type= */ std::string(),
          /* page_transition= */ std::nullopt,
          std::move(on_can_download_checks_done));
      return;
    }
  }

  BeginResourceDownloadOnChecksComplete(
      std::move(params), std::move(blob_url_loader_factory), is_new_download,
      storage_partition_config, rfh ? !content_initiated : true);
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
