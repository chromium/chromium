// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/attribution_reporting/features.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "components/services/storage/storage_service_impl.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/browsing_data/storage_partition_code_cache_data_remover.h"
#include "content/browser/browsing_topics/browsing_topics_site_data_manager_impl.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/cache_storage/cache_storage_control_wrapper.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_manager_impl.h"
#include "content/browser/cookie_store/cookie_store_manager.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/font_access/font_access_manager.h"
#include "content/browser/gpu/gpu_disk_cache_factory.h"
#include "content/browser/host_zoom_level_context.h"
#include "content/browser/indexed_db/indexed_db_control_wrapper.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/loader/reconnectable_url_loader_factory.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/network/shared_dictionary_util.h"
#include "content/browser/network_context_client_base_impl.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/push_messaging/push_messaging_context.h"
#include "content/browser/quota/quota_context.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_state_keep_alive.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/shared_storage/shared_storage_header_observer.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl_private_key_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/private_network_device_delegate.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_notification_service.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/ssl/client_cert_store.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-shared.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "net/android/http_auth_negotiate_android.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/cdm_storage_manager.h"
#include "content/browser/media/media_license_manager.h"
#include "content/public/browser/cdm_storage_data_model.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

using CookieDeletionFilter = network::mojom::CookieDeletionFilter;
using CookieDeletionFilterPtr = network::mojom::CookieDeletionFilterPtr;

namespace content {

namespace {

using Type = StoragePartitionImpl::ContextType;

const storage::QuotaSettings* g_test_quota_settings;

// Timeout after which the
// History.ClearBrowsingData.Duration.SlowTasks180sStoragePartition histogram is
// recorded.
const base::TimeDelta kSlowTaskTimeout = base::Seconds(180);

// If true, Storage Service instances will always be started in-process.
bool g_force_in_process_storage_service = false;

mojo::Remote<storage::mojom::StorageService>& GetStorageServiceRemoteStorage() {
  // NOTE: This use of sequence-local storage is only to ensure that the Remote
  // only lives as long as the UI-thread sequence, since the UI-thread sequence
  // may be torn down and reinitialized e.g. between unit tests.
  static base::SequenceLocalStorageSlot<
      mojo::Remote<storage::mojom::StorageService>>
      remote_slot;
  return remote_slot.GetOrCreateValue();
}

void RunInProcessStorageService(
    mojo::PendingReceiver<storage::mojom::StorageService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  static base::SequenceLocalStorageSlot<
      std::unique_ptr<storage::StorageServiceImpl>>
      service_storage_slot;
  service_storage_slot.GetOrCreateValue() =
      std::make_unique<storage::StorageServiceImpl>(std::move(receiver),
                                                    /*io_task_runner=*/nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
void BindStorageServiceFilesystemImpl(
    const base::FilePath& directory_path,
    mojo::PendingReceiver<storage::mojom::Directory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<storage::FilesystemImpl>(
          directory_path, storage::FilesystemImpl::ClientType::kUntrusted),
      std::move(receiver));
}
#endif

mojo::Remote<storage::mojom::StorageService>& GetStorageServiceRemote() {
  mojo::Remote<storage::mojom::StorageService>& remote =
      GetStorageServiceRemoteStorage();
  if (!remote) {
#if !BUILDFLAG(IS_ANDROID)
    const base::FilePath sandboxed_data_dir =
        GetContentClient()
            ->browser()
            ->GetSandboxedStorageServiceDataDirectory();
    const bool single_process_mode =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess);
    const bool oop_storage_enabled = !sandboxed_data_dir.empty() &&
                                     !single_process_mode &&
                                     !g_force_in_process_storage_service;
    if (oop_storage_enabled) {
      DCHECK(sandboxed_data_dir.IsAbsolute())
          << "Storage Service data directory must be an absolute path, but \""
          << sandboxed_data_dir << "\" is not an absolute path.";
      remote = ServiceProcessHost::Launch<storage::mojom::StorageService>(
          ServiceProcessHost::Options()
              .WithDisplayName("Storage Service")
              .Pass());
      remote.reset_on_disconnect();

      // Provide the service with an API it can use to access filesystem
      // contents *only* within the embedder's specified data directory.
      mojo::PendingRemote<storage::mojom::Directory> directory;
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
          ->PostTask(FROM_HERE,
                     base::BindOnce(
                         &BindStorageServiceFilesystemImpl, sandboxed_data_dir,
                         directory.InitWithNewPipeAndPassReceiver()));
      remote->SetDataDirectory(sandboxed_data_dir, std::move(directory));
    } else
#endif  // !BUILDFLAG(IS_ANDROID)
    {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&RunInProcessStorageService,
                                    remote.BindNewPipeAndPassReceiver()));
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableAggressiveDOMStorageFlushing)) {
      remote->EnableAggressiveDomStorageFlushing();
    }
  }
  return remote;
}

void OnClearedCookies(base::OnceClosure callback, uint32_t num_deleted) {
  // The final callback needs to happen from UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&OnClearedCookies, std::move(callback), num_deleted));
    return;
  }

  std::move(callback).Run();
}

void CheckQuotaManagedDataDeletionStatus(size_t* deletion_task_count,
                                         base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (*deletion_task_count == 0) {
    delete deletion_task_count;
    std::move(callback).Run();
  }
}

void OnQuotaManagedBucketDeleted(const storage::BucketLocator& bucket,
                                 size_t* deletion_task_count,
                                 base::OnceClosure callback,
                                 blink::mojom::QuotaStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(*deletion_task_count, 0u);
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    DLOG(ERROR) << "Couldn't remove data type " << static_cast<int>(bucket.type)
                << " for bucket with storage key "
                << bucket.storage_key.GetDebugString() << " is_default "
                << bucket.is_default << " and bucket id " << bucket.id
                << ". Status: " << static_cast<int>(status);
  }

  (*deletion_task_count)--;
  CheckQuotaManagedDataDeletionStatus(deletion_task_count, std::move(callback));
}

void PerformQuotaManagerStorageCleanup(
    const scoped_refptr<storage::QuotaManager>& quota_manager,
    blink::mojom::StorageType quota_storage_type,
    storage::QuotaClientTypes quota_client_types,
    base::OnceClosure callback) {
  quota_manager->PerformStorageCleanup(
      quota_storage_type, std::move(quota_client_types), std::move(callback));
}

void ClearedGpuCache(base::OnceClosure callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClearedGpuCache, std::move(callback)));
    return;
  }
  std::move(callback).Run();
}

void OnLocalStorageUsageInfo(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    bool perform_storage_cleanup,
    const base::Time delete_begin,
    const base::Time delete_end,
    base::OnceClosure callback,
    const std::vector<StorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::OnceClosure done_callback =
      perform_storage_cleanup
          ? base::BindOnce(
                &DOMStorageContextWrapper::PerformLocalStorageCleanup,
                dom_storage_context, std::move(callback))
          : std::move(callback);

  base::ConcurrentClosures concurrent;
  for (const StorageUsageInfo& info : infos) {
    if (storage_key_matcher &&
        !storage_key_matcher.Run(info.storage_key,
                                 special_storage_policy.get())) {
      continue;
    }

    if (info.last_modified >= delete_begin &&
        info.last_modified <= delete_end) {
      dom_storage_context->DeleteLocalStorage(info.storage_key,
                                              concurrent.CreateClosure());
    }
  }
  std::move(concurrent).Done(std::move(done_callback));
}

void OnSessionStorageUsageInfo(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    bool perform_storage_cleanup,
    base::OnceClosure callback,
    const std::vector<SessionStorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::OnceClosure done_callback =
      perform_storage_cleanup
          ? base::BindOnce(
                &DOMStorageContextWrapper::PerformSessionStorageCleanup,
                dom_storage_context, std::move(callback))
          : std::move(callback);

  base::ConcurrentClosures concurrent;
  for (const SessionStorageUsageInfo& info : infos) {
    if (storage_key_matcher &&
        !storage_key_matcher.Run(info.storage_key,
                                 special_storage_policy.get())) {
      continue;
    }
    dom_storage_context->DeleteSessionStorage(info, concurrent.CreateClosure());
  }
  std::move(concurrent).Done(std::move(done_callback));
}

void ClearLocalStorageOnUIThread(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    const blink::StorageKey& storage_key,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!storage_key.origin().opaque()) {
    bool can_delete =
        !storage_key_matcher ||
        storage_key_matcher.Run(storage_key, special_storage_policy.get());
    if (can_delete) {
      dom_storage_context->DeleteLocalStorage(storage_key, std::move(callback));
    } else {
      std::move(callback).Run();
    }
    return;
  }

  dom_storage_context->GetLocalStorageUsage(
      base::BindOnce(&OnLocalStorageUsageInfo, dom_storage_context,
                     special_storage_policy, std::move(storage_key_matcher),
                     perform_storage_cleanup, begin, end, std::move(callback)));
}

void ClearSessionStorageOnUIThread(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    bool perform_storage_cleanup,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  dom_storage_context->GetSessionStorageUsage(
      base::BindOnce(&OnSessionStorageUsageInfo, dom_storage_context,
                     special_storage_policy, std::move(storage_key_matcher),
                     perform_storage_cleanup, std::move(callback)));
}

// LoginHandlerDelegate manages HTTP auth. It is self-owning and deletes itself
// when the credentials are resolved or the AuthChallengeResponder is cancelled.
class LoginHandlerDelegate {
 public:
  LoginHandlerDelegate(
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder,
      WebContents* web_contents,
      content::BrowserContext* browser_context,
      const net::AuthChallengeInfo& auth_info,
      bool is_request_for_primary_main_frame,
      bool is_request_for_navigation,
      base::StrictNumeric<int32_t> process_id,
      base::StrictNumeric<int32_t> request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt)
      : auth_challenge_responder_(std::move(auth_challenge_responder)),
        auth_info_(auth_info),
        request_id_(process_id, request_id),
        is_request_for_primary_main_frame_(is_request_for_primary_main_frame),
        is_request_for_navigation_(is_request_for_navigation),
        creating_login_delegate_(false),
        url_(url),
        response_headers_(std::move(response_headers)),
        first_auth_attempt_(first_auth_attempt),
        web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
        browser_context_(browser_context->GetWeakPtr()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auth_challenge_responder_.set_disconnect_handler(base::BindOnce(
        &LoginHandlerDelegate::OnRequestCancelled, base::Unretained(this)));

    DevToolsURLLoaderInterceptor::HandleAuthRequest(
        request_id_, auth_info_,
        base::BindOnce(&LoginHandlerDelegate::ContinueAfterInterceptor,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnRequestCancelled() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // This will destroy `login_handler_io_` on the IO thread and, if needed,
    // inform the delegate.
    delete this;
  }

  void ContinueAfterInterceptor(
      bool use_fallback,
      const std::optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!(use_fallback && auth_credentials.has_value()));
    if (!use_fallback) {
      OnAuthCredentials(auth_credentials);
      return;
    }

    if (!browser_context_) {
      OnAuthCredentials(std::nullopt);
      return;
    }

    if (web_contents_) {
      CHECK_EQ(web_contents_->GetBrowserContext(), browser_context_.get());
    }

    // WeakPtr is not strictly necessary here due to OnRequestCancelled.
    creating_login_delegate_ = true;
    login_delegate_ = GetContentClient()->browser()->CreateLoginDelegate(
        auth_info_, web_contents_.get(), browser_context_.get(), request_id_,
        is_request_for_primary_main_frame_, is_request_for_navigation_, url_,
        response_headers_, first_auth_attempt_,
        base::BindOnce(&LoginHandlerDelegate::OnAuthCredentials,
                       weak_factory_.GetWeakPtr()));
    creating_login_delegate_ = false;
    if (!login_delegate_) {
      OnAuthCredentials(std::nullopt);
      return;
    }
  }

  void OnAuthCredentials(
      const std::optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // CreateLoginDelegate must not call the callback reentrantly. For
    // robustness, detect this mistake.
    CHECK(!creating_login_delegate_);
    auth_challenge_responder_->OnAuthCredentials(auth_credentials);
    delete this;
  }

  mojo::Remote<network::mojom::AuthChallengeResponder>
      auth_challenge_responder_;
  net::AuthChallengeInfo auth_info_;
  const content::GlobalRequestID request_id_;
  bool is_request_for_primary_main_frame_;
  bool is_request_for_navigation_;
  bool creating_login_delegate_;
  GURL url_;
  const scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool first_auth_attempt_;
  base::WeakPtr<WebContents> web_contents_;
  base::WeakPtr<BrowserContext> browser_context_;
  std::unique_ptr<LoginDelegate> login_delegate_;
  base::WeakPtrFactory<LoginHandlerDelegate> weak_factory_{this};
};

// This class lives on the UI thread. It is self-owned and will delete itself
// after any of the SSLClientAuthHandler::Delegate methods are invoked (or when
// a mojo connection error occurs).
class SSLClientAuthDelegate : public SSLClientAuthHandler::Delegate {
 public:
  SSLClientAuthDelegate(
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          client_cert_responder_remote,
      BrowserContext* browser_context,
      int process_id,
      base::WeakPtr<WebContents> web_contents,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info)
      : client_cert_responder_(std::move(client_cert_responder_remote)),
        ssl_client_auth_handler_(std::make_unique<SSLClientAuthHandler>(
            GetContentClient()->browser()->CreateClientCertStore(
                browser_context),
            browser_context->GetWeakPtr(),
            process_id,
            web_contents,
            std::move(cert_info.get()),
            this)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(client_cert_responder_);
    client_cert_responder_.set_disconnect_handler(base::BindOnce(
        &SSLClientAuthDelegate::DeleteSelf, base::Unretained(this)));
    ssl_client_auth_handler_->SelectCertificate();
  }

  ~SSLClientAuthDelegate() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  void DeleteSelf() { delete this; }

  // SSLClientAuthHandler::Delegate:
  void CancelCertificateSelection() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    client_cert_responder_->CancelRequest();
    DeleteSelf();
  }

  // SSLClientAuthHandler::Delegate:
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (cert && private_key) {
      mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key;

      mojo::MakeSelfOwnedReceiver(
          std::make_unique<SSLPrivateKeyImpl>(private_key),
          ssl_private_key.InitWithNewPipeAndPassReceiver());

      client_cert_responder_->ContinueWithCertificate(
          cert, private_key->GetProviderName(),
          private_key->GetAlgorithmPreferences(), std::move(ssl_private_key));
    } else {
      client_cert_responder_->ContinueWithoutCertificate();
    }

    DeleteSelf();
  }

 private:
  mojo::Remote<network::mojom::ClientCertificateResponder>
      client_cert_responder_;
  std::unique_ptr<SSLClientAuthHandler> ssl_client_auth_handler_;
};

void CallCancelRequest(
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        client_cert_responder_remote) {
  DCHECK(client_cert_responder_remote);
  mojo::Remote<network::mojom::ClientCertificateResponder>
      client_cert_responder(std::move(client_cert_responder_remote));
  client_cert_responder->CancelRequest();
}

// Cancels prerendering if `navigation_or_document` is in a prerendered frame
// tree, using `final_status` as the cancellation reason. Returns true if
// cancelled.
bool CancelIfPrerendering(NavigationOrDocumentHandle* navigation_or_document,
                          PrerenderFinalStatus final_status) {
  FrameTreeNode* frame_tree_node = nullptr;
  // `navigation_or_document` can be null for `kServiceWorkerContext`.
  if (!navigation_or_document) {
    return false;
  }
  auto* navigation_request = navigation_or_document->GetNavigationRequest();
  if (navigation_request) {
    frame_tree_node = navigation_request->frame_tree_node();
  }
  auto* render_frame_host = navigation_or_document->GetDocument();
  if (render_frame_host) {
    frame_tree_node = FrameTreeNode::From(render_frame_host);
  }
  if (!frame_tree_node) {
    return false;
  }

  auto* web_contents = WebContentsImpl::FromFrameTreeNode(frame_tree_node);
  return web_contents->CancelPrerendering(frame_tree_node, final_status);
}

class SSLErrorDelegate : public SSLErrorHandler::Delegate {
 public:
  explicit SSLErrorDelegate(network::mojom::URLLoaderNetworkServiceObserver::
                                OnSSLCertificateErrorCallback response)
      : response_(std::move(response)) {}
  ~SSLErrorDelegate() override = default;
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override {
    std::move(response_).Run(error);
    delete this;
  }
  void ContinueSSLRequest() override {
    std::move(response_).Run(net::OK);
    delete this;
  }
  base::WeakPtr<SSLErrorDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  network::mojom::URLLoaderNetworkServiceObserver::OnSSLCertificateErrorCallback
      response_;
  base::WeakPtrFactory<SSLErrorDelegate> weak_factory_{this};
};

#if BUILDFLAG(IS_ANDROID)
void FinishGenerateNegotiateAuthToken(
    std::unique_ptr<net::android::HttpAuthNegotiateAndroid> auth_negotiate,
    std::unique_ptr<std::string> auth_token,
    std::unique_ptr<net::HttpAuthPreferences> prefs,
    network::mojom::NetworkContextClient::
        OnGenerateHttpNegotiateAuthTokenCallback callback,
    int result) {
  std::move(callback).Run(result, *auth_token);
}
#endif

// If both `storage_key_matcher` and `storage_key_policy_matcher` are null, this
// should return a null callback that indicates all StorageKeys should match.
// Otherwise, returns true if StorageKey matches both `storage_key_matcher` and
// `storage_key_policy_matcher` if not null.
StoragePartition::StorageKeyPolicyMatcherFunction
CombineStorageKeyMatcherFunctions(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    StoragePartition::StorageKeyPolicyMatcherFunction
        storage_key_policy_matcher) {
  if (storage_key_matcher.is_null() && storage_key_policy_matcher.is_null()) {
    return base::NullCallback();
  }

  return base::BindRepeating(
      [](StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
         StoragePartition::StorageKeyPolicyMatcherFunction
             storage_key_policy_matcher,
         const blink::StorageKey& storage_key,
         storage::SpecialStoragePolicy* policy) -> bool {
        return (!storage_key_matcher || storage_key_matcher.Run(storage_key)) &&
               (!storage_key_policy_matcher ||
                storage_key_policy_matcher.Run(storage_key, policy));
      },
      std::move(storage_key_matcher), std::move(storage_key_policy_matcher));
}

// Conceptually, many downstream interfaces don't need to know about the
// complexity of callers into StoragePartition, so this function reduces the API
// surface to something simple and generic. It is designed to be used by
// callsites in ClearDataImpl.
//
// Precondition: `storage_key_matcher`/`storage_key_policy_matcher` and
// `storage_key` cannot both be set. If all of `storage_key_matcher`,
// `storage_key_policy_matcher` and `storage_key` are null/empty, this should
// return a null callback that indicates all StorageKeys should match. This is
// an optimization for backends to efficiently clear all data.
//
// TODO(csharrison, mek): Right now, this is only used by a small set of storage
// backends, e.g. aggregation service. We should consider moving some of the
// backends to use this if they can, and additionally we should consider
// rethinking this approach if / when storage backends move out of process
// (see crbug.com/1016065 for initial work here).
StoragePartition::StorageKeyMatcherFunction CreateGenericStorageKeyMatcher(
    const blink::StorageKey& storage_key,
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    StoragePartition::StorageKeyPolicyMatcherFunction
        storage_key_policy_matcher,
    scoped_refptr<storage::SpecialStoragePolicy> policy) {
  const bool storage_key_origin_empty = storage_key.origin().opaque();
  DCHECK(storage_key_origin_empty || storage_key_matcher.is_null());
  DCHECK(storage_key_origin_empty || storage_key_policy_matcher.is_null());

  if (storage_key_origin_empty && storage_key_matcher.is_null() &&
      storage_key_policy_matcher.is_null()) {
    return base::NullCallback();
  }

  if (storage_key_matcher || storage_key_policy_matcher) {
    return base::BindRepeating(
        [](StoragePartition::StorageKeyPolicyMatcherFunction
               storage_key_policy_matcher,
           scoped_refptr<storage::SpecialStoragePolicy> policy,
           const blink::StorageKey& storage_key) -> bool {
          DCHECK(!storage_key_policy_matcher.is_null());
          return storage_key_policy_matcher.Run(storage_key, policy.get());
        },
        CombineStorageKeyMatcherFunctions(
            std::move(storage_key_matcher),
            std::move(storage_key_policy_matcher)),
        std::move(policy));
  }
  DCHECK(!storage_key_origin_empty);
  return base::BindRepeating(std::equal_to<const blink::StorageKey&>(),
                             storage_key);
}

}  // namespace

// Static.
storage::QuotaClientTypes StoragePartitionImpl::GenerateQuotaClientTypes(
    uint32_t remove_mask) {
  storage::QuotaClientTypes quota_client_types;

  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS) {
    quota_client_types.insert(storage::QuotaClientType::kFileSystem);
  }
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_WEBSQL) {
    quota_client_types.insert(storage::QuotaClientType::kDatabase);
  }
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_INDEXEDDB) {
    quota_client_types.insert(storage::QuotaClientType::kIndexedDatabase);
  }
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS) {
    quota_client_types.insert(storage::QuotaClientType::kServiceWorker);
  }
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE) {
    quota_client_types.insert(storage::QuotaClientType::kServiceWorkerCache);
  }
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_BACKGROUND_FETCH) {
    quota_client_types.insert(storage::QuotaClientType::kBackgroundFetch);
  }
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_MEDIA_LICENSES) {
    quota_client_types.insert(storage::QuotaClientType::kMediaLicense);
  }
  return quota_client_types;
}

// static
void StoragePartitionImpl::ForceInProcessStorageServiceForTesting() {
  g_force_in_process_storage_service = true;
}

// Helper for deleting quota managed data from a partition.
//
// Most of the operations in this class are done on IO thread.
class StoragePartitionImpl::QuotaManagedDataDeletionHelper {
 public:
  QuotaManagedDataDeletionHelper(
      uint32_t remove_mask,
      uint32_t quota_storage_remove_mask,
      const std::optional<blink::StorageKey>& storage_key,
      base::OnceClosure callback)
      : remove_mask_(remove_mask),
        quota_storage_remove_mask_(quota_storage_remove_mask),
        storage_key_(storage_key),
        callback_(std::move(callback)),
        task_count_(0) {
    DCHECK(!storage_key_.has_value() || !storage_key_->origin().opaque());
  }

  QuotaManagedDataDeletionHelper(const QuotaManagedDataDeletionHelper&) =
      delete;
  QuotaManagedDataDeletionHelper& operator=(
      const QuotaManagedDataDeletionHelper&) = delete;

  void IncrementTaskCountOnIO();
  void DecrementTaskCountOnIO();

  void ClearDataOnIOThread(
      const scoped_refptr<storage::QuotaManager>& quota_manager,
      const base::Time begin,
      const base::Time end,
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
      bool perform_storage_cleanup);

  void ClearBucketsOnIOThread(
      storage::QuotaManager* quota_manager,
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
      bool perform_storage_cleanup,
      base::OnceClosure callback,
      blink::mojom::StorageType quota_storage_type,
      const std::set<storage::BucketLocator>& buckets);

 private:
  // All of these data are accessed on IO thread.
  uint32_t remove_mask_;
  uint32_t quota_storage_remove_mask_;
  std::optional<blink::StorageKey> storage_key_;
  base::OnceClosure callback_;
  int task_count_;
};

// Helper for deleting all sorts of data from a partition, keeps track of
// deletion status.
//
// StoragePartitionImpl creates an instance of this class to keep track of
// data deletion progress. Deletion requires deleting multiple bits of data
// (e.g. cookies, local storage, session storage etc.) and hopping between UI
// and IO thread. An instance of this class is created in the beginning of
// deletion process (StoragePartitionImpl::ClearDataImpl) and the instance is
// forwarded and updated on each (sub) deletion's callback. The instance is
// finally destroyed when deletion completes (and `callback` is invoked).
class StoragePartitionImpl::DataDeletionHelper {
 public:
  DataDeletionHelper(uint32_t remove_mask,
                     uint32_t quota_storage_remove_mask,
                     base::OnceClosure callback)
      : remove_mask_(remove_mask),
        quota_storage_remove_mask_(quota_storage_remove_mask),
        callback_(std::move(callback)) {}

  DataDeletionHelper(const DataDeletionHelper&) = delete;
  DataDeletionHelper& operator=(const DataDeletionHelper&) = delete;

  ~DataDeletionHelper() = default;

  void ClearDataOnUIThread(
      const blink::StorageKey& storage_key,
      BrowsingDataFilterBuilder* filter_builder,
      StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
      CookieDeletionFilterPtr cookie_deletion_filter,
      const base::FilePath& path,
      DOMStorageContextWrapper* dom_storage_context,
      storage::QuotaManager* quota_manager,
      storage::SpecialStoragePolicy* special_storage_policy,
      storage::FileSystemContext* filesystem_context,
      network::mojom::CookieManager* cookie_manager,
      InterestGroupManagerImpl* interest_group_manager,
      AttributionManager* attribution_manager,
      AggregationService* aggregation_service,
      PrivateAggregationManagerImpl* private_aggregation_manager,
      storage::SharedStorageManager* shared_storage_manager,
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
      CdmStorageManager* cdm_storage_manager,
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
      bool perform_storage_cleanup,
      const base::Time begin,
      const base::Time end);

  void ClearQuotaManagedDataOnIOThread(
      const scoped_refptr<storage::QuotaManager>& quota_manager,
      const base::Time begin,
      const base::Time end,
      const blink::StorageKey& storage_key,
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
      bool perform_storage_cleanup,
      base::OnceClosure callback);

 private:
  // For debugging purposes. Please add new deletion tasks at the end.
  // This enum is recorded in a histogram, so don't change or reuse ids.
  // LINT.IfChange(TracingDataType)
  enum class TracingDataType {
    kSynchronous = 1,
    kCookies = 2,
    kQuota = 3,
    kLocalStorage = 4,
    kSessionStorage = 5,
    kShaderCache = 6,    // Deprecated in favor of using kGpuCache.
    kPluginPrivate = 7,  // Deprecated.
    kConversions = 8,
    kAggregationService = 9,
    kSharedStorage = 10,
    kGpuCache = 11,
    kPrivateAggregation = 12,
    kInterestGroups = 13,
    kCdmStorage = 14,
    kMaxValue = kCdmStorage,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/history/enums.xml:StoragePartitionRemoverTasks)

  base::OnceClosure CreateTaskCompletionClosure(TracingDataType data_type);

  void OnTaskComplete(TracingDataType data_type,
                      int tracing_id);  // Callable on any thread.
  void RecordUnfinishedSubTasks();

  uint32_t remove_mask_;
  uint32_t quota_storage_remove_mask_;

  // Accessed on UI thread.
  base::OnceClosure callback_;
  // Accessed on UI thread.
  std::set<TracingDataType> pending_tasks_;

  base::WeakPtrFactory<StoragePartitionImpl::DataDeletionHelper> weak_factory_{
      this};
};

void StoragePartitionImpl::DataDeletionHelper::ClearQuotaManagedDataOnIOThread(
    const scoped_refptr<storage::QuotaManager>& quota_manager,
    const base::Time begin,
    const base::Time end,
    const blink::StorageKey& storage_key,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    bool perform_storage_cleanup,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoragePartitionImpl::QuotaManagedDataDeletionHelper* helper =
      new StoragePartitionImpl::QuotaManagedDataDeletionHelper(
          remove_mask_, quota_storage_remove_mask_,
          storage_key.origin().opaque() ? std::nullopt
                                        : std::make_optional(storage_key),
          std::move(callback));
  helper->ClearDataOnIOThread(quota_manager, begin, end, special_storage_policy,
                              std::move(storage_key_matcher),
                              perform_storage_cleanup);
}

class StoragePartitionImpl::ServiceWorkerCookieAccessObserver
    : public network::mojom::CookieAccessObserver {
 public:
  explicit ServiceWorkerCookieAccessObserver(
      StoragePartitionImpl* storage_partition)
      : storage_partition_(storage_partition) {}

 private:
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override {
    storage_partition_->service_worker_cookie_observers_.Add(
        std::make_unique<ServiceWorkerCookieAccessObserver>(storage_partition_),
        std::move(observer));
  }

  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (auto& details : details_vector) {
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
          storage_partition_->GetServiceWorkerContext();
      std::vector<GlobalRenderFrameHostId> destinations =
          *service_worker_context->GetWindowClientFrameRoutingIds(
              blink::StorageKey::CreateFirstParty(
                  url::Origin::Create(details->url)));
      if (destinations.empty()) {
        return;
      }

      for (GlobalRenderFrameHostId frame_id : destinations) {
        if (RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(frame_id)) {
          rfh->OnCookiesAccessed(mojo::Clone(details_vector));
        }
      }
    }
  }

  // `storage_partition_` owns this object via UniqueReceiverSet
  // (service_worker_cookie_observers_).
  raw_ptr<StoragePartitionImpl> storage_partition_;
};

class StoragePartitionImpl::ServiceWorkerTrustTokenAccessObserver
    : public network::mojom::TrustTokenAccessObserver {
 public:
  explicit ServiceWorkerTrustTokenAccessObserver(
      StoragePartitionImpl* storage_partition)
      : storage_partition_(storage_partition) {}

 private:
  void Clone(mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver>
                 observer) override {
    storage_partition_->service_worker_trust_token_observers_.Add(
        std::make_unique<ServiceWorkerTrustTokenAccessObserver>(
            storage_partition_),
        std::move(observer));
  }

  void OnTrustTokensAccessed(
      network::mojom::TrustTokenAccessDetailsPtr details) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
        storage_partition_->GetServiceWorkerContext();

    url::Origin origin;
    switch (details->which()) {
      case network::mojom::TrustTokenAccessDetails::Tag::kIssuance:
        origin = details->get_issuance()->origin;
        break;
      case network::mojom::TrustTokenAccessDetails::Tag::kRedemption:
        origin = details->get_redemption()->origin;
        break;
      case network::mojom::TrustTokenAccessDetails::Tag::kSigning:
        origin = details->get_signing()->origin;
        break;
    }

    std::vector<GlobalRenderFrameHostId> destinations =
        *service_worker_context->GetWindowClientFrameRoutingIds(
            blink::StorageKey::CreateFirstParty(origin));
    if (destinations.empty()) {
      return;
    }

    for (GlobalRenderFrameHostId frame_id : destinations) {
      if (RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(frame_id)) {
        rfh->OnTrustTokensAccessed(mojo::Clone(details));
      }
    }
  }

  // `storage_partition_` owns this object via UniqueReceiverSet
  // (service_worker_trust_token_observers_).
  raw_ptr<StoragePartitionImpl> storage_partition_;
};

class StoragePartitionImpl::ServiceWorkerSharedDictionaryAccessObserver
    : public network::mojom::SharedDictionaryAccessObserver {
 public:
  explicit ServiceWorkerSharedDictionaryAccessObserver(
      StoragePartitionImpl* storage_partition)
      : storage_partition_(storage_partition) {}

 private:
  void Clone(
      mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>
          observer) override {
    storage_partition_->service_worker_shared_dictionary_observers_.Add(
        std::make_unique<ServiceWorkerSharedDictionaryAccessObserver>(
            storage_partition_),
        std::move(observer));
  }

  void OnSharedDictionaryAccessed(
      network::mojom::SharedDictionaryAccessDetailsPtr details) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
        storage_partition_->GetServiceWorkerContext();

    std::vector<GlobalRenderFrameHostId> destinations =
        *service_worker_context->GetWindowClientFrameRoutingIds(
            blink::StorageKey::CreateFirstParty(
                details->isolation_key.frame_origin()));
    if (destinations.empty()) {
      return;
    }

    for (GlobalRenderFrameHostId frame_id : destinations) {
      if (RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(frame_id)) {
        rfh->OnSharedDictionaryAccessed(mojo::Clone(details));
      }
    }
  }

  // `storage_partition_` owns this object via UniqueReceiverSet
  // (service_worker_shared_dictionary_observers_).
  raw_ptr<StoragePartitionImpl> storage_partition_;
};

struct StoragePartitionImpl::NetworkContextOwner {
  mojo::Remote<network::mojom::NetworkContext> network_context;
};

StoragePartitionImpl::StoragePartitionImpl(
    BrowserContext* browser_context,
    const StoragePartitionConfig& config,
    const base::FilePath& partition_path,
    const base::FilePath& relative_partition_path,
    storage::SpecialStoragePolicy* special_storage_policy)
    : browser_context_(browser_context),
      partition_path_(partition_path),
      config_(config),
      relative_partition_path_(relative_partition_path),
      special_storage_policy_(special_storage_policy),
      network_context_owner_(std::make_unique<NetworkContextOwner>()),
      deletion_helpers_running_(0) {}

StoragePartitionImpl::~StoragePartitionImpl() {
#if DCHECK_IS_ON()
  DCHECK(on_browser_context_will_be_destroyed_called_);
#endif
  browser_context_ = nullptr;

  scoped_refptr<storage::DatabaseTracker> database_tracker(
      GetDatabaseTracker());
  if (database_tracker) {
    storage::DatabaseTracker* database_tracker_ptr = database_tracker.get();
    database_tracker_ptr->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&storage::DatabaseTracker::Shutdown,
                                  std::move(database_tracker)));
  }

  if (GetFileSystemContext()) {
    GetFileSystemContext()->Shutdown();
  }

  if (GetDOMStorageContext()) {
    GetDOMStorageContext()->Shutdown();
  }

  if (GetServiceWorkerContext()) {
    GetServiceWorkerContext()->Shutdown();
  }

  if (GetBackgroundSyncContext()) {
    GetBackgroundSyncContext()->Shutdown();
  }

  if (GetBackgroundFetchContext()) {
    GetBackgroundFetchContext()->Shutdown();
  }

  if (GetGeneratedCodeCacheContext()) {
    GetGeneratedCodeCacheContext()->Shutdown();
  }
}

void StoragePartitionImpl::OnBrowserContextWillBeDestroyed() {
#if DCHECK_IS_ON()
  on_browser_context_will_be_destroyed_called_ = true;
#endif

  // Shut down service worker and shared worker machinery because these can keep
  // RenderProcessHosts and SiteInstances alive, and the codebase assumes these
  // are destroyed before the BrowserContext is destroyed.
  GetServiceWorkerContext()->Shutdown();
  GetSharedWorkerService()->Shutdown();

  // These hold raw pointers to objects that are about to be destroyed, before
  // this object is destroyed. Shut them down now to avoid dangling pointers.
  if (GetFileSystemAccessManager()) {
    GetFileSystemAccessManager()->Shutdown();
  }

  if (GetContentIndexContext()) {
    GetContentIndexContext()->Shutdown();
  }

  if (GetPlatformNotificationContext()) {
    GetPlatformNotificationContext()->Shutdown();
  }

  if (keep_alive_url_loader_service_) {
    keep_alive_url_loader_service_->Shutdown();
  }
}

void StoragePartitionImpl::RegisterKeepAliveHandle(
    mojo::PendingReceiver<blink::mojom::NavigationStateKeepAliveHandle>
        receiver,
    std::unique_ptr<NavigationStateKeepAlive> handle) {
  navigation_state_keep_alive_map_.erase(handle->frame_token());
  navigation_state_keep_alive_map_.insert(
      std::make_pair(handle->frame_token(), handle.get()));

  keep_alive_handles_receiver_set_.Add(std::move(handle), std::move(receiver));
}

void StoragePartitionImpl::RevokeNetworkForNoncesInNetworkContext(
    const std::vector<base::UnguessableToken>& nonces,
    network::mojom::NetworkContext::RevokeNetworkForNoncesCallback callback) {
  GetNetworkContext()->RevokeNetworkForNonces(nonces, std::move(callback));

  // Save nonces in `StoragePartitionImpl`. When there is a crash of
  // `NetworkService`, the network revocation nonces of `NetworkContext` will be
  // restored using this.
  network_revocation_nonces_.insert(std::begin(nonces), std::end(nonces));
}

void StoragePartitionImpl::ClearNoncesInNetworkContextAfterDelay(
    const std::vector<base::UnguessableToken>& nonces) {
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &StoragePartitionImpl::ClearNoncesInNetworkContextAfterDelayCallback,
          weak_factory_.GetWeakPtr(), nonces),
      clear_nonces_in_network_context_delay_);
}

void StoragePartitionImpl::ClearNoncesInNetworkContextAfterDelayCallback(
    const std::vector<base::UnguessableToken>& nonces) {
  GetNetworkContext()->ClearNonces(nonces);

  for (const auto& nonce : nonces) {
    network_revocation_nonces_.erase(nonce);
  }

  clear_nonces_in_network_context_callback_for_testing_.Run();
}

void StoragePartitionImpl::RemoveKeepAliveHandleFromMap(
    blink::LocalFrameToken frame_token,
    NavigationStateKeepAlive* keep_alive) {
  // The NavigationStateKeepAlive associated with `frame_token` may have
  // changed. Make sure the specified one is removed from the map.
  auto it = navigation_state_keep_alive_map_.find(frame_token);
  if (it != navigation_state_keep_alive_map_.end() &&
      it->second == keep_alive) {
    navigation_state_keep_alive_map_.erase(frame_token);
  }
}

NavigationStateKeepAlive* StoragePartitionImpl::GetNavigationStateKeepAlive(
    blink::LocalFrameToken frame_token) {
  auto it = navigation_state_keep_alive_map_.find(frame_token);
  if (it == navigation_state_keep_alive_map_.end()) {
    return nullptr;
  }
  return it->second;
}

// static
std::unique_ptr<StoragePartitionImpl> StoragePartitionImpl::Create(
    BrowserContext* context,
    const StoragePartitionConfig& config,
    const base::FilePath& relative_partition_path) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  base::FilePath partition_path =
      context->GetPath().Append(relative_partition_path);

  return base::WrapUnique(new StoragePartitionImpl(
      context, config, partition_path, relative_partition_path,
      context->GetSpecialStoragePolicy()));
}

void StoragePartitionImpl::Initialize(
    StoragePartitionImpl* fallback_for_blob_urls) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  DCHECK(!initialized_);
  initialized_ = true;

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManager being used. We do them
  // all together here prior to handing out a reference to anything
  // that utilizes the QuotaManager.
  quota_context_ = base::MakeRefCounted<QuotaContext>(
      is_in_memory(), partition_path_,
      browser_context_->GetSpecialStoragePolicy(),
      base::BindRepeating(&StoragePartitionImpl::GetQuotaSettings,
                          weak_factory_.GetWeakPtr()));
  quota_manager_ = quota_context_->quota_manager();
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy =
      quota_manager_->proxy();

  StorageNotificationService* storage_notification_service =
      browser_context_->GetStorageNotificationService();
  if (storage_notification_service) {
    // The weak ptr associated with the pressure notification callback will be
    // created and evaluated by a task runner on the UI thread, as confirmed by
    // the DCHECK's above, ensuring that the task runner does not attempt to run
    // the callback in the case that the storage notification service is already
    // destructed.
    quota_manager_->SetStoragePressureCallback(
        storage_notification_service
            ->CreateThreadSafePressureNotificationCallback());
  }

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  filesystem_context_ = CreateFileSystemContext(
      browser_context_, partition_path_, is_in_memory(), quota_manager_proxy);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
  // WebView.
  database_tracker_ = storage::DatabaseTracker::Create(
      partition_path_, is_in_memory(), quota_manager_proxy);
#endif  // BUILDFLAG(IS_ANDROID)

  dom_storage_context_ = DOMStorageContextWrapper::Create(
      this, browser_context_->GetSpecialStoragePolicy());

  lock_manager_ = std::make_unique<LockManager<storage::BucketId>>();

  shared_storage_worklet_host_manager_ =
      std::make_unique<SharedStorageWorkletHostManager>();

  scoped_refptr<ChromeBlobStorageContext> blob_context =
      ChromeBlobStorageContext::GetFor(browser_context_);

  file_system_access_manager_ =
      base::MakeRefCounted<FileSystemAccessManagerImpl>(
          filesystem_context_, blob_context,
          browser_context_->GetFileSystemAccessPermissionContext(),
          browser_context_->IsOffTheRecord());

  mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
      file_system_access_context;
  file_system_access_manager_->BindInternalsReceiver(
      file_system_access_context.InitWithNewPipeAndPassReceiver());
  base::FilePath path = is_in_memory() ? base::FilePath() : partition_path_;
  indexed_db_control_wrapper_ =
      std::make_unique<indexed_db::IndexedDBControlWrapper>(
          path, browser_context_->GetSpecialStoragePolicy(),
          quota_manager_proxy,
          ChromeBlobStorageContext::GetRemoteFor(browser_context_),
          std::move(file_system_access_context), GetIOThreadTaskRunner({}));

  cache_storage_control_wrapper_ = std::make_unique<CacheStorageControlWrapper>(
      GetIOThreadTaskRunner({}), path,
      browser_context_->GetSpecialStoragePolicy(), quota_manager_proxy,
      ChromeBlobStorageContext::GetRemoteFor(browser_context_));

  service_worker_context_ = new ServiceWorkerContextWrapper(browser_context_);
  service_worker_context_->set_storage_partition(this);

  dedicated_worker_service_ = std::make_unique<DedicatedWorkerServiceImpl>();

  shared_worker_service_ =
      std::make_unique<SharedWorkerServiceImpl>(this, service_worker_context_);

  push_messaging_context_ = std::make_unique<PushMessagingContext>(
      browser_context_, service_worker_context_);

  host_zoom_level_context_.reset(new HostZoomLevelContext(
      browser_context_->CreateZoomLevelDelegate(partition_path_)));

  platform_notification_context_ = new PlatformNotificationContextImpl(
      path, browser_context_, service_worker_context_);
  platform_notification_context_->Initialize();

  devtools_background_services_context_ =
      std::make_unique<DevToolsBackgroundServicesContextImpl>(
          browser_context_, service_worker_context_);

  content_index_context_ = base::MakeRefCounted<ContentIndexContextImpl>(
      browser_context_, service_worker_context_);

  background_fetch_context_ = base::MakeRefCounted<BackgroundFetchContext>(
      weak_factory_.GetWeakPtr(), service_worker_context_, quota_manager_proxy,
      *devtools_background_services_context_.get());

  background_sync_context_ = base::MakeRefCounted<BackgroundSyncContextImpl>();
  background_sync_context_->Init(service_worker_context_,
                                 *devtools_background_services_context_.get());

  payment_app_context_ = new PaymentAppContextImpl();
  payment_app_context_->Init(service_worker_context_);

  broadcast_channel_service_ = std::make_unique<BroadcastChannelService>();

  bluetooth_allowed_devices_map_ =
      std::make_unique<BluetoothAllowedDevicesMap>();

  // Must be initialized before the
  // `shared_url_loader_factory_for_browser_process_`. Cookie deprecation
  // traffic labels should not be sent for off-the-record profiles, unless the
  // "enable_otr_profiles" feature parameter is true.
  if (base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting) &&
      (!is_in_memory() ||
       features::kCookieDeprecationFacilitatedTestingEnableOTRProfiles.Get())) {
    cookie_deprecation_label_manager_ =
        std::make_unique<CookieDeprecationLabelManagerImpl>(browser_context_);
  }

  shared_url_loader_factory_for_browser_process_ = std::make_unique<
      ReconnectableURLLoaderFactoryForIOThreadWrapper>(base::BindRepeating(
      &StoragePartitionImpl::CreateURLLoaderFactoryForBrowserProcessInternal,
      GetWeakPtr()));
  shared_url_loader_factory_for_browser_process_->factory_for_io_thread()
      ->Initialize();

  service_worker_context_->Init(path, quota_manager_proxy.get(),
                                browser_context_->GetSpecialStoragePolicy(),
                                blob_context.get());

  blob_url_registry_ = std::make_unique<storage::BlobUrlRegistry>(
      fallback_for_blob_urls
          ? fallback_for_blob_urls->GetBlobUrlRegistry()->AsWeakPtr()
          : nullptr);

  blob_registry_ = BlobRegistryWrapper::Create(blob_context);

  subresource_proxying_url_loader_service_ =
      std::make_unique<SubresourceProxyingURLLoaderService>(browser_context_);

  if (blink::features::IsKeepAliveURLLoaderServiceEnabled()) {
    keep_alive_url_loader_service_ =
        std::make_unique<KeepAliveURLLoaderService>(browser_context_);
  }

  cookie_store_manager_ =
      std::make_unique<CookieStoreManager>(service_worker_context_);
  // Unit tests use the LoadAllSubscriptions() callback to crash early if
  // restoring the CookieManagerStore's state from ServiceWorkerStorage fails.
  // Production and browser tests rely on CookieStoreManager's well-defined
  // behavior when restoring the state fails.
  cookie_store_manager_->LoadAllSubscriptions(base::DoNothing());

  bucket_manager_ = std::make_unique<BucketManager>(this);

  if (base::FeatureList::IsEnabled(
          attribution_reporting::features::kConversionMeasurement)) {
    // The Conversion Measurement API is not available in Incognito mode, but
    // this is enforced by the `AttributionManagerImpl` itself for better error
    // reporting and metrics.
    attribution_manager_ = std::make_unique<AttributionManagerImpl>(
        this, path, special_storage_policy_);
  }

  if (base::FeatureList::IsEnabled(blink::features::kInterestGroupStorage)) {
    // Auction worklets on non-Android use dedicated processes; on Android due
    // to high cost of process launch they try to reuse renderers.
    interest_group_manager_ = std::make_unique<InterestGroupManagerImpl>(
        path, is_in_memory(),
#if BUILDFLAG(IS_ANDROID)
        InterestGroupManagerImpl::ProcessMode::kInRenderer,
#else
        InterestGroupManagerImpl::ProcessMode::kDedicated,
#endif
        GetURLLoaderFactoryForBrowserProcess(),
        base::BindRepeating(&BrowserContext::GetKAnonymityServiceDelegate,
                            // This use of Unretained is safe since the browser
                            // context owns this storage partition.
                            base::Unretained(browser_context_)));
  }

  // The Topics API is not available in Incognito mode.
  if (!is_in_memory() &&
      base::FeatureList::IsEnabled(blink::features::kBrowsingTopics)) {
    browsing_topics_site_data_manager_ =
        std::make_unique<BrowsingTopicsSiteDataManagerImpl>(path);
  }

  GeneratedCodeCacheSettings settings =
      GetContentClient()->browser()->GetGeneratedCodeCacheSettings(
          browser_context_);

  // For Incognito mode, we should not persist anything on the disk so
  // we do not create a code cache. Caching the generated code in memory
  // is not useful, since V8 already maintains one copy in memory.
  if (!is_in_memory() && settings.enabled()) {
    generated_code_cache_context_ =
        base::MakeRefCounted<GeneratedCodeCacheContext>();

    base::FilePath code_cache_path;
    if (config_.partition_domain().empty()) {
      code_cache_path = settings.path().AppendASCII("Code Cache");
    } else {
      // For site isolated partitions use the config directory.
      code_cache_path = settings.path()
                            .Append(relative_partition_path_)
                            .AppendASCII("Code Cache");
    }
    DCHECK_GE(settings.size_in_bytes(), 0);
    GetGeneratedCodeCacheContext()->Initialize(code_cache_path,
                                               settings.size_in_bytes());
  }

  font_access_manager_ = FontAccessManager::Create();

  if (base::FeatureList::IsEnabled(kPrivacySandboxAggregationService)) {
    aggregation_service_ =
        std::make_unique<AggregationServiceImpl>(is_in_memory(), path, this);
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (is_in_memory()) {
    // Pass an empty path if in_memory so that CdmStorage.db is not stored on
    // disk.
    cdm_storage_manager_ =
        std::make_unique<CdmStorageManager>(base::FilePath());
  } else {
    cdm_storage_manager_ = std::make_unique<CdmStorageManager>(
        partition_path_.Append(kCdmStorageDatabaseFileName));
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    base::FilePath shared_storage_path =
        is_in_memory() ? base::FilePath()
                       : path.Append(storage::kSharedStoragePath);
    shared_storage_manager_ = std::make_unique<storage::SharedStorageManager>(
        shared_storage_path, special_storage_policy_);
    if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM118)) {
      shared_storage_header_observer_ =
          std::make_unique<SharedStorageHeaderObserver>(this);
    }
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi)) {
    private_aggregation_manager_ =
        std::make_unique<PrivateAggregationManagerImpl>(is_in_memory(), path,
                                                        this);
  }
}

void StoragePartitionImpl::OnStorageServiceDisconnected() {
  // This will be lazily re-bound on next use.
  remote_partition_.reset();

  dom_storage_context_->RecoverFromStorageServiceCrash();
  for (const auto& client : dom_storage_clients_) {
    client.second->ResetStorageAreaAndNamespaceConnections();
  }
}

const StoragePartitionConfig& StoragePartitionImpl::GetConfig() const {
  return config_;
}

const base::FilePath& StoragePartitionImpl::GetPath() const {
  return partition_path_;
}

const std::string& StoragePartitionImpl::GetPartitionDomain() const {
  return config_.partition_domain();
}

network::mojom::NetworkContext* StoragePartitionImpl::GetNetworkContext() {
  DCHECK(initialized_);
  if (!network_context_owner_->network_context.is_bound()) {
    InitNetworkContext();
  }
  return network_context_owner_->network_context.get();
}

cert_verifier::mojom::CertVerifierServiceUpdater*
StoragePartitionImpl::GetCertVerifierServiceUpdater() {
  DCHECK(initialized_);
  if (!cert_verifier_service_updater_.is_bound()) {
    InitNetworkContext();
  }
  return cert_verifier_service_updater_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcess() {
  CHECK(shared_url_loader_factory_for_browser_process_);
  return shared_url_loader_factory_for_browser_process_->factory();
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcessIOThread() {
  CHECK(shared_url_loader_factory_for_browser_process_);
  return shared_url_loader_factory_for_browser_process_->factory_for_io_thread()
      ->CloneForIOThread();
}

network::mojom::CookieManager*
StoragePartitionImpl::GetCookieManagerForBrowserProcess() {
  DCHECK(initialized_);
  // Create the CookieManager as needed.
  if (!cookie_manager_for_browser_process_ ||
      !cookie_manager_for_browser_process_.is_connected()) {
    // Reset `cookie_manager_for_browser_process_` before binding it again.
    cookie_manager_for_browser_process_.reset();
    GetNetworkContext()->GetCookieManager(
        cookie_manager_for_browser_process_.BindNewPipeAndPassReceiver());
  }
  return cookie_manager_for_browser_process_.get();
}

void StoragePartitionImpl::CreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    bool is_service_worker,
    int process_id,
    int routing_id,
    net::CookieSettingOverrides cookie_setting_overrides,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer) {
  DCHECK(initialized_);
  if (!GetContentClient()->browser()->WillCreateRestrictedCookieManager(
          role, browser_context_, origin, isolation_info, is_service_worker,
          process_id, routing_id, &receiver)) {
    GetNetworkContext()->GetRestrictedCookieManager(
        std::move(receiver), role, origin, isolation_info,
        cookie_setting_overrides, std::move(cookie_observer));
  }
}

void StoragePartitionImpl::CreateTrustTokenQueryAnswerer(
    mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver,
    const url::Origin& top_frame_origin) {
  DCHECK(initialized_);
  GetNetworkContext()->GetTrustTokenQueryAnswerer(std::move(receiver),
                                                  top_frame_origin);
}

storage::QuotaManager* StoragePartitionImpl::GetQuotaManager() {
  DCHECK(initialized_);
  return quota_manager_.get();
}

storage::QuotaManagerProxy* StoragePartitionImpl::GetQuotaManagerProxy() {
  DCHECK(initialized_);
  return quota_manager_->proxy();
}

BackgroundSyncContextImpl* StoragePartitionImpl::GetBackgroundSyncContext() {
  DCHECK(initialized_);
  return background_sync_context_.get();
}

storage::FileSystemContext* StoragePartitionImpl::GetFileSystemContext() {
  DCHECK(initialized_);
  return filesystem_context_.get();
}

storage::DatabaseTracker* StoragePartitionImpl::GetDatabaseTracker() {
  DCHECK(initialized_);
  return database_tracker_.get();
}

DOMStorageContextWrapper* StoragePartitionImpl::GetDOMStorageContext() {
  DCHECK(initialized_);
  return dom_storage_context_.get();
}

storage::mojom::LocalStorageControl*
StoragePartitionImpl::GetLocalStorageControl() {
  DCHECK(initialized_);
  return GetDOMStorageContext()->GetLocalStorageControl();
}

LockManager<storage::BucketId>* StoragePartitionImpl::GetLockManager() {
  DCHECK(initialized_);
  return lock_manager_.get();
}

SharedStorageWorkletHostManager*
StoragePartitionImpl::GetSharedStorageWorkletHostManager() {
  DCHECK(initialized_);
  return shared_storage_worklet_host_manager_.get();
}

storage::mojom::IndexedDBControl& StoragePartitionImpl::GetIndexedDBControl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return indexed_db_control_wrapper_->GetIndexedDBControl();
}

FileSystemAccessEntryFactory*
StoragePartitionImpl::GetFileSystemAccessEntryFactory() {
  DCHECK(initialized_);
  return file_system_access_manager_.get();
}

QuotaContext* StoragePartitionImpl::GetQuotaContext() {
  DCHECK(initialized_);
  return quota_context_.get();
}

storage::mojom::CacheStorageControl*
StoragePartitionImpl::GetCacheStorageControl() {
  DCHECK(initialized_);
  return cache_storage_control_wrapper_.get();
}

ServiceWorkerContextWrapper* StoragePartitionImpl::GetServiceWorkerContext() {
  DCHECK(initialized_);
  return service_worker_context_.get();
}

DedicatedWorkerServiceImpl* StoragePartitionImpl::GetDedicatedWorkerService() {
  DCHECK(initialized_);
  return dedicated_worker_service_.get();
}

SharedWorkerService* StoragePartitionImpl::GetSharedWorkerService() {
  DCHECK(initialized_);
  return shared_worker_service_.get();
}

HostZoomMap* StoragePartitionImpl::GetHostZoomMap() {
  DCHECK(initialized_);
  DCHECK(host_zoom_level_context_.get());
  return host_zoom_level_context_->GetHostZoomMap();
}

HostZoomLevelContext* StoragePartitionImpl::GetHostZoomLevelContext() {
  DCHECK(initialized_);
  return host_zoom_level_context_.get();
}

ZoomLevelDelegate* StoragePartitionImpl::GetZoomLevelDelegate() {
  DCHECK(initialized_);
  DCHECK(host_zoom_level_context_.get());
  return host_zoom_level_context_->GetZoomLevelDelegate();
}

PlatformNotificationContextImpl*
StoragePartitionImpl::GetPlatformNotificationContext() {
  DCHECK(initialized_);
  return platform_notification_context_.get();
}

BackgroundFetchContext* StoragePartitionImpl::GetBackgroundFetchContext() {
  DCHECK(initialized_);
  return background_fetch_context_.get();
}

PaymentAppContextImpl* StoragePartitionImpl::GetPaymentAppContext() {
  DCHECK(initialized_);
  return payment_app_context_.get();
}

BroadcastChannelService* StoragePartitionImpl::GetBroadcastChannelService() {
  DCHECK(initialized_);
  return broadcast_channel_service_.get();
}

BluetoothAllowedDevicesMap*
StoragePartitionImpl::GetBluetoothAllowedDevicesMap() {
  DCHECK(initialized_);
  return bluetooth_allowed_devices_map_.get();
}

BlobRegistryWrapper* StoragePartitionImpl::GetBlobRegistry() {
  DCHECK(initialized_);
  return blob_registry_.get();
}

storage::BlobUrlRegistry* StoragePartitionImpl::GetBlobUrlRegistry() {
  DCHECK(initialized_);
  return blob_url_registry_.get();
}

SubresourceProxyingURLLoaderService*
StoragePartitionImpl::GetSubresourceProxyingURLLoaderService() {
  DCHECK(initialized_);
  return subresource_proxying_url_loader_service_.get();
}

KeepAliveURLLoaderService*
StoragePartitionImpl::GetKeepAliveURLLoaderService() {
  DCHECK(initialized_);
  return keep_alive_url_loader_service_.get();
}

CookieStoreManager* StoragePartitionImpl::GetCookieStoreManager() {
  DCHECK(initialized_);
  return cookie_store_manager_.get();
}

BucketManager* StoragePartitionImpl::GetBucketManager() {
  DCHECK(initialized_);
  return bucket_manager_.get();
}

GeneratedCodeCacheContext*
StoragePartitionImpl::GetGeneratedCodeCacheContext() {
  DCHECK(initialized_);
  return generated_code_cache_context_.get();
}

DevToolsBackgroundServicesContext*
StoragePartitionImpl::GetDevToolsBackgroundServicesContext() {
  DCHECK(initialized_);
  return devtools_background_services_context_.get();
}

FileSystemAccessManagerImpl*
StoragePartitionImpl::GetFileSystemAccessManager() {
  DCHECK(initialized_);
  return file_system_access_manager_.get();
}

AttributionManager* StoragePartitionImpl::GetAttributionManager() {
  DCHECK(initialized_);
  return attribution_manager_.get();
}

AttributionDataModel* StoragePartitionImpl::GetAttributionDataModel() {
  DCHECK(initialized_);
  return attribution_manager_.get();
}

FontAccessManager* StoragePartitionImpl::GetFontAccessManager() {
  DCHECK(initialized_);
  return font_access_manager_.get();
}

void StoragePartitionImpl::SetFontAccessManagerForTesting(
    std::unique_ptr<FontAccessManager> font_access_manager) {
  DCHECK(initialized_);
  DCHECK(font_access_manager);
  font_access_manager_ = std::move(font_access_manager);
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
MediaLicenseManager* StoragePartitionImpl::GetMediaLicenseManager() {
  DCHECK(initialized_);
  return media_license_manager_.get();
}

CdmStorageDataModel* StoragePartitionImpl::GetCdmStorageDataModel() {
  DCHECK(initialized_);
  return cdm_storage_manager_.get();
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

InterestGroupManager* StoragePartitionImpl::GetInterestGroupManager() {
  DCHECK(initialized_);
  return interest_group_manager_.get();
}

BrowsingTopicsSiteDataManager*
StoragePartitionImpl::GetBrowsingTopicsSiteDataManager() {
  DCHECK(initialized_);
  return browsing_topics_site_data_manager_.get();
}

ContentIndexContextImpl* StoragePartitionImpl::GetContentIndexContext() {
  DCHECK(initialized_);
  return content_index_context_.get();
}

AggregationService* StoragePartitionImpl::GetAggregationService() {
  DCHECK(initialized_);
  return aggregation_service_.get();
}

leveldb_proto::ProtoDatabaseProvider*
StoragePartitionImpl::GetProtoDatabaseProvider() {
  if (!proto_database_provider_) {
    proto_database_provider_ =
        std::make_unique<leveldb_proto::ProtoDatabaseProvider>(partition_path_,
                                                               is_in_memory());
  }
  return proto_database_provider_.get();
}

void StoragePartitionImpl::SetProtoDatabaseProvider(
    std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider) {
  DCHECK(!proto_database_provider_);
  proto_database_provider_ = std::move(proto_db_provider);
}

leveldb_proto::ProtoDatabaseProvider*
StoragePartitionImpl::GetProtoDatabaseProviderForTesting() {
  return proto_database_provider_.get();
}

storage::SharedStorageManager* StoragePartitionImpl::GetSharedStorageManager() {
  return shared_storage_manager_.get();
}

PrivateAggregationManager*
StoragePartitionImpl::GetPrivateAggregationManager() {
  DCHECK(initialized_);
  return private_aggregation_manager_.get();
}

PrivateAggregationDataModel*
StoragePartitionImpl::GetPrivateAggregationDataModel() {
  DCHECK(initialized_);
  return private_aggregation_manager_.get();
}

CookieDeprecationLabelManager*
StoragePartitionImpl::GetCookieDeprecationLabelManager() {
  CHECK(initialized_);
  return cookie_deprecation_label_manager_.get();
}

void StoragePartitionImpl::DeleteStaleSessionOnlyCookiesAfterDelay() {
  // We need to delay deleting stale session cookies until after the cookie db
  // has initialized, otherwise we will bypass lazy loading and block.
  // See crbug.com/40285083 for more info.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &StoragePartitionImpl::DeleteStaleSessionOnlyCookiesAfterDelayCallback,
          weak_factory_.GetWeakPtr()),
      delete_stale_session_only_cookies_delay_);
}

void StoragePartitionImpl::DeleteStaleSessionOnlyCookiesAfterDelayCallback() {
  GetCookieManagerForBrowserProcess()->DeleteStaleSessionOnlyCookies(
      base::BindOnce([](const uint32_t num_deleted) {
        base::UmaHistogramCounts10M(
            "Cookie.StaleSessionCookiesDeletedOnStartup", num_deleted);
      }));
}

void StoragePartitionImpl::OpenLocalStorage(
    const blink::StorageKey& storage_key,
    const blink::LocalFrameToken& local_frame_token,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(initialized_);
  ChildProcessSecurityPolicyImpl::Handle security_policy_handle =
      dom_storage_receivers_.current_context()->Duplicate();
  dom_storage_context_->OpenLocalStorage(
      storage_key, local_frame_token, std::move(receiver),
      std::move(security_policy_handle),
      dom_storage_receivers_.GetBadMessageCallback());
}

void StoragePartitionImpl::BindSessionStorageNamespace(
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  DCHECK(initialized_);
  dom_storage_context_->BindNamespace(
      namespace_id, dom_storage_receivers_.GetBadMessageCallback(),
      std::move(receiver));
}

void StoragePartitionImpl::BindSessionStorageArea(
    const blink::StorageKey& storage_key,
    const blink::LocalFrameToken& local_frame_token,
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(initialized_);
  ChildProcessSecurityPolicyImpl::Handle security_policy_handle =
      dom_storage_receivers_.current_context()->Duplicate();
  dom_storage_context_->BindStorageArea(
      storage_key, local_frame_token, namespace_id, std::move(receiver),
      std::move(security_policy_handle),
      dom_storage_receivers_.GetBadMessageCallback());
}

void StoragePartitionImpl::OnAuthRequired(
    const std::optional<base::UnguessableToken>& window_id,
    int32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  URLLoaderNetworkContext context =
      url_loader_network_observers_.current_context();
  std::optional<bool> is_primary_main_frame;
  std::optional<bool> is_navigation_request;

  if (window_id) {
    // Use `window_id` if it is provided, because this request was sent by a
    // service worker; service workers use `window_id` to identify the frame
    // that sends the request since a worker is shared among multiple frames.
    // TODO(crbug.com/40194275): Add a DCHECK here that process_id and
    // routing_id are invalid. It can't be added yet because somehow routing_id
    // is valid here.
    if (service_worker_context_->context()) {
      auto* container_host = service_worker_context_->context()
                                 ->service_worker_client_owner()
                                 .GetServiceWorkerClientByWindowId(*window_id);
      if (container_host) {
        if (container_host->GetRenderFrameHostId()) {
          // Use ServiceWorkerContainerHost's GlobalRenderFrameHostId when
          // the navigation commit has already started.
          GlobalRenderFrameHostId render_frame_host_id =
              container_host->GetRenderFrameHostId();
          context = URLLoaderNetworkContext::CreateForRenderFrameHost(
              render_frame_host_id);

          // Only the request for a sub resource intercepted by a service worker
          // reaches here.
          is_primary_main_frame = false;
          is_navigation_request = false;
        } else if (NavigationRequest* ongoing_navigation =
                       container_host->GetOngoingNavigationRequestBeforeCommit(
                           base::PassKey<StoragePartitionImpl>())) {
          // This auth request is for an ongoing navigation controlled
          // by service worker. The navigation request can be nullptr if user
          // has closed the WebContents.
          // Overwrite the context; set `type` to kNavigationRequestContext.
          // TODO(crbug.com/40784852): Optimize locating logic.
          context =
              URLLoaderNetworkContext::CreateForNavigation(*ongoing_navigation);
        }
      }
    }
  }

  // If the request is for a prerendering page, prerendering should be cancelled
  // because the embedder may show UI for auth requests, and it's unsuitable for
  // a hidden page.
  if (CancelIfPrerendering(context.navigation_or_document(),
                           PrerenderFinalStatus::kLoginAuthRequested)) {
    return;
  }

  if (!is_primary_main_frame.has_value()) {
    is_primary_main_frame = context.IsPrimaryMainFrameRequest();
  }
  if (!is_navigation_request.has_value()) {
    is_navigation_request = context.IsNavigationRequestContext();
  }
  int process_id = network::mojom::kBrowserProcessId;
  if (context.type() == ContextType::kRenderFrameHostContext) {
    // Set `process_id` to `kInvalidProcessId` considering `render_frame_host`
    // can be null when it's destroyed already. `process_id` is updated only if
    // `render_frame_host` is not null. If `render_frame_host` is null,
    // later logic will call OnAuthCredentials() with a nullopt that triggers
    // CancelAuth().
    process_id = network::mojom::kInvalidProcessId;

    // `navigation_or_document_` can be null when `context` is created with
    // an invalid RenderFrameHost after a page is destroyed.
    // It is currently possible for the ServiceWorker case above to use
    // kRenderFrameHostContext for the auth request, after the RenderFrameHost
    // has been deleted. Treating this as an invalid process ID will cancel the
    // auth, which is the same outcome as if the ServiceWorker's process were
    // used.
    // TODO(crbug.com/40224422): Update the ServiceWorker code to
    // recognize when the RenderFrameHost goes away and not use
    // CreateForRenderFrameHost above.
    if (context.navigation_or_document()) {
      auto* render_frame_host = context.navigation_or_document()->GetDocument();
      if (render_frame_host) {
        process_id = render_frame_host->GetGlobalId().child_id;
      }
    }
  } else if (context.type() == ContextType::kServiceWorkerContext) {
    process_id = context.process_id();
  }

  WebContents* current_web_contents = context.GetWebContents();
  if (current_web_contents) {
    // Evict all the BFCache entries that
    // 1): are stored in the same BrowserContext
    // 2): were loaded with the "Cache-control: no-store" header
    // 3): match the challenger information of the page that requires HTTP
    // authentication.
    for (WebContentsImpl* web_contents : WebContentsImpl::GetAllWebContents()) {
      if (web_contents->GetBrowserContext()->UniqueId() ==
          current_web_contents->GetBrowserContext()->UniqueId()) {
        for (const std::unique_ptr<BackForwardCacheImpl::Entry>& entry :
             web_contents->GetController().GetBackForwardCache().GetEntries()) {
          RenderFrameHostImpl* rfh = entry->render_frame_host();
          const GURL& last_committed_url = rfh->GetLastCommittedURL();
          if (rfh->LoadedWithCacheControlNoStoreHeader() &&
              auth_info.challenger ==
                  url::SchemeHostPort(last_committed_url.scheme(),
                                      last_committed_url.host(),
                                      last_committed_url.IntPort())) {
            BackForwardCacheCanStoreDocumentResult flattened_reasons;
            flattened_reasons.No(BackForwardCacheMetrics::NotRestoredReason::
                                     kCacheControlNoStore);
            flattened_reasons.No(
                BackForwardCacheMetrics::NotRestoredReason::kHTTPAuthRequired);
            rfh->EvictFromBackForwardCacheWithFlattenedReasons(
                flattened_reasons);
          }
        }
      }
    }
  }

  new LoginHandlerDelegate(std::move(auth_challenge_responder),
                           current_web_contents, browser_context_, auth_info,
                           *is_primary_main_frame, *is_navigation_request,
                           process_id, request_id, url, head_headers,
                           first_auth_attempt);  // deletes self
}

void StoragePartitionImpl::OnPrivateNetworkAccessPermissionRequired(
    const GURL& url,
    const net::IPAddress& ip_address,
    const std::optional<std::string>& private_network_device_id,
    const std::optional<std::string>& private_network_device_name,
    OnPrivateNetworkAccessPermissionRequiredCallback callback) {
  if (!base::FeatureList::IsEnabled(
          network::features::kPrivateNetworkAccessPermissionPrompt)) {
    std::move(callback).Run(false);
    return;
  }

  if (url_loader_network_observers_.empty()) {
    std::move(callback).Run(false);
    return;
  }
  const URLLoaderNetworkContext& context =
      url_loader_network_observers_.current_context();

  if (context.type() != ContextType::kRenderFrameHostContext ||
      !context.navigation_or_document()) {
    std::move(callback).Run(false);
    return;
  }
  RenderFrameHost* render_frame_host =
      context.navigation_or_document()->GetDocument();
  if (!render_frame_host) {
    std::move(callback).Run(false);
    return;
  }
  auto device = blink::mojom::PrivateNetworkDevice::New(
      private_network_device_id, private_network_device_name, ip_address);

  PrivateNetworkDeviceDelegate* delegate =
      GetContentClient()->browser()->GetPrivateNetworkDeviceDelegate();
  if (!delegate) {
    std::move(callback).Run(false);
    return;
  }

  delegate->RequestPermission(*render_frame_host, std::move(device),
                              std::move(callback));
}

void StoragePartitionImpl::OnCertificateRequested(
    const std::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder) {
  URLLoaderNetworkContext context =
      url_loader_network_observers_.current_context();

  if (window_id) {
    // Use `window_id` if it is provided, because this request was sent by a
    // service worker; service workers use `window_id` to identify the frame
    // that sends the request since a worker is shared among multiple frames.
    // TODO(crbug.com/40194275): Add a DCHECK here that process_id and
    // routing_id are invalid. It can't be added yet because somehow routing_id
    // is valid here.
    if (service_worker_context_->context()) {
      auto* container_host = service_worker_context_->context()
                                 ->service_worker_client_owner()
                                 .GetServiceWorkerClientByWindowId(*window_id);
      if (container_host) {
        if (container_host->GetRenderFrameHostId()) {
          // Use ServiceWorkerContainerHost's GlobalRenderFrameHostId when
          // the navigation commit has already started.
          GlobalRenderFrameHostId render_frame_host_id =
              container_host->GetRenderFrameHostId();
          context = URLLoaderNetworkContext::CreateForRenderFrameHost(
              render_frame_host_id);
        } else if (NavigationRequest* ongoing_navigation =
                       container_host->GetOngoingNavigationRequestBeforeCommit(
                           base::PassKey<StoragePartitionImpl>())) {
          // This certification request is for an ongoing navigation.
          // Overwrite the context; set `type` to kNavigationRequestContext.
          // TODO(crbug.com/40784852): Optimize locating logic.
          context =
              URLLoaderNetworkContext::CreateForNavigation(*ongoing_navigation);
        } else {
          // The navigation request was canceled since the WebContents was
          // discarded, so it is meaningless to continue the certification
          // request.
          CallCancelRequest(std::move(cert_responder));
          return;
        }
      }
    }
  }

  // If the request is for a prerendering page, prerendering should be cancelled
  // because the embedder may show a dialog and ask users to select client
  // certificates, and it's unsuitable for a hidden page.
  if (CancelIfPrerendering(context.navigation_or_document(),
                           PrerenderFinalStatus::kClientCertRequested)) {
    CallCancelRequest(std::move(cert_responder));
    return;
  }

  base::WeakPtr<WebContents> web_contents_weak;
  int process_id = network::mojom::kInvalidProcessId;
  if (context.type() == ContextType::kServiceWorkerContext) {
    process_id = context.process_id();
  } else {
    WebContents* web_contents = context.GetWebContents();
    // The WebContents is already invalid. Bail.
    if (!web_contents) {
      CallCancelRequest(std::move(cert_responder));
      return;
    }
    CHECK_EQ(web_contents->GetBrowserContext(), browser_context_.get());
    web_contents_weak = web_contents->GetWeakPtr();

    if (context.navigation_or_document()) {
      auto* render_frame_host = context.navigation_or_document()->GetDocument();
      if (render_frame_host) {
        process_id = render_frame_host->GetProcess()->GetID();
      }
    }
  }

  // SSLClientAuthDelegate handles its own lifetime.
  new SSLClientAuthDelegate(std::move(cert_responder), browser_context(),
                            process_id, web_contents_weak, cert_info);
}

void StoragePartitionImpl::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  URLLoaderNetworkContext context =
      url_loader_network_observers_.current_context();

  // Cancel this request and the prerendering if the request is for a
  // prerendering page, because prerendering pages are invisible and browser
  // cannot show errors on invisible pages.
  if (CancelIfPrerendering(context.navigation_or_document(),
                           PrerenderFinalStatus::kSslCertificateError)) {
    std::move(response).Run(net_error);
    return;
  }

  SSLErrorDelegate* delegate =
      new SSLErrorDelegate(std::move(response));  // deletes self
  bool is_primary_main_frame_request = context.IsPrimaryMainFrameRequest();
  SSLManager::OnSSLCertificateError(
      delegate->GetWeakPtr(), is_primary_main_frame_request, url,
      context.navigation_or_document(), net_error, ssl_info, fatal);
}

void StoragePartitionImpl::OnLoadingStateUpdate(
    network::mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  auto* web_contents =
      url_loader_network_observers_.current_context().GetWebContents();
  if (web_contents) {
    static_cast<WebContentsImpl*>(web_contents)
        ->LoadStateChanged(std::move(info));
  }
  std::move(callback).Run();
}

void StoragePartitionImpl::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  GlobalRenderFrameHostId render_frame_host_id =
      GetRenderFrameHostIdFromNetworkContext();
  GetContentClient()->browser()->OnNetworkServiceDataUseUpdate(
      render_frame_host_id, network_traffic_annotation_id_hash, recv_bytes,
      sent_bytes);
}

void StoragePartitionImpl::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<network::mojom::SharedStorageOperationPtr> operations,
    OnSharedStorageHeaderReceivedCallback callback) {
  if (!shared_storage_header_observer_) {
    std::move(callback).Run();
    return;
  }

  // Currently, shared-storage-writable headers aren't available for requests
  // initiated by service workers, so `navigation_or_document` should be
  // non-null.
  //
  // TODO(cammie): If we handle the service worker case by allowing service
  // workers to initiate shared-storage-writable requests, the assumption that
  // `navigation_or_document` must be non-null may become incorrect.
  auto* navigation_or_document =
      url_loader_network_observers_.current_context().navigation_or_document();
  DCHECK(navigation_or_document);

  shared_storage_header_observer_->HeaderReceived(
      request_origin, url_loader_network_observers_.current_context().type(),
      navigation_or_document, std::move(operations), std::move(callback),
      mojo::GetBadMessageCallback(), /*can_defer=*/true);
}

void StoragePartitionImpl::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        observer) {
  url_loader_network_observers_.Add(
      this, std::move(observer),
      url_loader_network_observers_.current_context());
}

void StoragePartitionImpl::OnWebSocketConnectedToPrivateNetwork(
    network::mojom::IPAddressSpace ip_address_space) {
  RenderFrameHostImpl* render_frame_host_impl =
      RenderFrameHostImpl::FromID(GetRenderFrameHostIdFromNetworkContext());

  if (render_frame_host_impl &&
      network::IsLessPublicAddressSpace(
          ip_address_space, render_frame_host_impl->BuildClientSecurityState()
                                ->ip_address_space)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        render_frame_host_impl,
        blink::mojom::WebFeature::kPrivateNetworkAccessWebSocketConnected);
  }
}

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
StoragePartitionImpl::CreateURLLoaderNetworkObserverForFrame(int process_id,
                                                             int routing_id) {
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver> remote;
  url_loader_network_observers_.Add(
      this, remote.InitWithNewPipeAndPassReceiver(),
      URLLoaderNetworkContext::CreateForRenderFrameHost(
          GlobalRenderFrameHostId(process_id, routing_id)));
  return remote;
}

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
StoragePartitionImpl::CreateURLLoaderNetworkObserverForNavigationRequest(
    NavigationRequest& navigation_request) {
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver> remote;
  url_loader_network_observers_.Add(
      this, remote.InitWithNewPipeAndPassReceiver(),
      URLLoaderNetworkContext::CreateForNavigation(navigation_request));
  return remote;
}

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
StoragePartitionImpl::CreateAuthCertObserverForServiceWorker(int process_id) {
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver> remote;
  url_loader_network_observers_.Add(this,
                                    remote.InitWithNewPipeAndPassReceiver(),
                                    URLLoaderNetworkContext(process_id));
  return remote;
}

void StoragePartitionImpl::OnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    const GURL& destination_url,
    OnFileUploadRequestedCallback callback) {
  NetworkContextOnFileUploadRequested(process_id, async, file_paths,
                                      destination_url, std::move(callback));
}

void StoragePartitionImpl::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  DCHECK(initialized_);
  PermissionController* permission_controller =
      browser_context_->GetPermissionController();
  DCHECK(permission_controller);

  std::vector<url::Origin> origins_out;
  for (auto& origin : origins) {
    bool allowed = permission_controller
                       ->GetPermissionResultForOriginWithoutContext(
                           blink::PermissionType::BACKGROUND_SYNC, origin)
                       .status == blink::mojom::PermissionStatus::GRANTED;
    if (allowed) {
      origins_out.push_back(origin);
    }
  }

  std::move(callback).Run(origins_out);
}

void StoragePartitionImpl::OnCanSendDomainReliabilityUpload(
    const url::Origin& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  DCHECK(initialized_);
  PermissionController* permission_controller =
      browser_context_->GetPermissionController();
  std::move(callback).Run(
      permission_controller
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::BACKGROUND_SYNC, origin)
          .status == blink::mojom::PermissionStatus::GRANTED);
}

void StoragePartitionImpl::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  DCHECK(initialized_);

  base::WeakPtr<WebContents> weak_web_contents;
  WebContents* web_contents =
      url_loader_network_observers_.current_context().GetWebContents();
  if (web_contents) {
    weak_web_contents = web_contents->GetWeakPtr();
  }

  std::optional<blink::StorageKey> storage_key = CalculateStorageKey(
      url::Origin::Create(url),
      cookie_partition_key.has_value()
          ? base::OptionalToPtr(cookie_partition_key.value().nonce())
          : nullptr);

  ClearSiteDataHandler::HandleHeader(
      browser_context()->GetWeakPtr(), weak_web_contents, GetConfig(), url,
      header_value, load_flags, cookie_partition_key, storage_key,
      partitioned_state_allowed_only, std::move(callback));
}

#if BUILDFLAG(IS_ANDROID)
void StoragePartitionImpl::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  // The callback takes ownership of these unique_ptrs and destroys them when
  // run.
  auto prefs = std::make_unique<net::HttpAuthPreferences>();
  prefs->set_auth_android_negotiate_account_type(
      auth_negotiate_android_account_type);

  auto auth_negotiate =
      std::make_unique<net::android::HttpAuthNegotiateAndroid>(prefs.get());
  net::android::HttpAuthNegotiateAndroid* auth_negotiate_raw =
      auth_negotiate.get();
  auth_negotiate->set_server_auth_token(server_auth_token);
  auth_negotiate->set_can_delegate(can_delegate);

  auto auth_token = std::make_unique<std::string>();
  auth_negotiate_raw->GenerateAuthTokenAndroid(
      nullptr, spn, std::string(), auth_token.get(),
      base::BindOnce(&FinishGenerateNegotiateAuthToken,
                     std::move(auth_negotiate), std::move(auth_token),
                     std::move(prefs), std::move(callback)));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void StoragePartitionImpl::OnTrustAnchorUsed() {
  GetContentClient()->browser()->OnTrustAnchorUsed(browser_context_);
}
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
void StoragePartitionImpl::OnCanSendSCTAuditingReport(
    OnCanSendSCTAuditingReportCallback callback) {
  bool allowed =
      GetContentClient()->browser()->CanSendSCTAuditingReport(browser_context_);
  std::move(callback).Run(allowed);
}

void StoragePartitionImpl::OnNewSCTAuditingReportSent() {
  GetContentClient()->browser()->OnNewSCTAuditingReportSent(browser_context_);
}
#endif

void StoragePartitionImpl::ClearDataImpl(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const blink::StorageKey& storage_key,
    BrowsingDataFilterBuilder* filter_builder,
    StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
    CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool storage_key_origin_empty = storage_key.origin().opaque();
  DCHECK(storage_key_origin_empty || !filter_builder);
  DCHECK(storage_key_origin_empty || storage_key_policy_matcher.is_null());

  StorageKeyMatcherFunction storage_key_matcher =
      filter_builder ? filter_builder->BuildStorageKeyFilter()
                     : StorageKeyMatcherFunction();

  for (auto& observer : data_removal_observers_) {
    auto filter = CreateGenericStorageKeyMatcher(
        storage_key, storage_key_matcher, storage_key_policy_matcher,
        special_storage_policy_);
    observer.OnStorageKeyDataCleared(remove_mask, std::move(filter), begin,
                                     end);
  }

  DataDeletionHelper* helper = new DataDeletionHelper(
      remove_mask, quota_storage_remove_mask,
      base::BindOnce(&StoragePartitionImpl::DeletionHelperDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  // `helper` deletes itself when done in
  // DataDeletionHelper::DecrementTaskCount().
  deletion_helpers_running_++;
  helper->ClearDataOnUIThread(
      storage_key, filter_builder, std::move(storage_key_policy_matcher),
      std::move(cookie_deletion_filter), GetPath(), dom_storage_context_.get(),
      quota_manager_.get(), special_storage_policy_.get(),
      filesystem_context_.get(), GetCookieManagerForBrowserProcess(),
      interest_group_manager_.get(), attribution_manager_.get(),
      aggregation_service_.get(), private_aggregation_manager_.get(),
      shared_storage_manager_.get(),
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
      cdm_storage_manager_.get(),
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
      perform_storage_cleanup, begin, end);
}

void StoragePartitionImpl::DeletionHelperDone(base::OnceClosure callback) {
  std::move(callback).Run();
  deletion_helpers_running_--;
  if (on_deletion_helpers_done_callback_ && deletion_helpers_running_ == 0) {
    // Notify tests that storage partition is done with all deletion tasks.
    std::move(on_deletion_helpers_done_callback_).Run();
  }
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::
    IncrementTaskCountOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ++task_count_;
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::
    DecrementTaskCountOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(task_count_, 0);
  --task_count_;
  if (task_count_) {
    return;
  }

  std::move(callback_).Run();
  delete this;
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::ClearDataOnIOThread(
    const scoped_refptr<storage::QuotaManager>& quota_manager,
    const base::Time begin,
    const base::Time end,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    bool perform_storage_cleanup) {
  IncrementTaskCountOnIO();
  base::RepeatingClosure decrement_callback = base::BindRepeating(
      &QuotaManagedDataDeletionHelper::DecrementTaskCountOnIO,
      base::Unretained(this));

  // Ask the QuotaManager for all buckets with temporary quota modified
  // within the user-specified timeframe, and deal with the resulting set in
  // ClearBucketsOnIOThread().
  if (quota_storage_remove_mask_ & QUOTA_MANAGED_STORAGE_MASK_TEMPORARY) {
    IncrementTaskCountOnIO();
    quota_manager->GetBucketsModifiedBetween(
        blink::mojom::StorageType::kTemporary, begin, end,
        base::BindOnce(&QuotaManagedDataDeletionHelper::ClearBucketsOnIOThread,
                       base::Unretained(this), base::RetainedRef(quota_manager),
                       special_storage_policy, storage_key_matcher,
                       perform_storage_cleanup, decrement_callback,
                       blink::mojom::StorageType::kTemporary));
  }

  // Do the same for syncable quota.
  if (quota_storage_remove_mask_ & QUOTA_MANAGED_STORAGE_MASK_SYNCABLE) {
    IncrementTaskCountOnIO();
    quota_manager->GetBucketsModifiedBetween(
        blink::mojom::StorageType::kSyncable, begin, end,
        base::BindOnce(&QuotaManagedDataDeletionHelper::ClearBucketsOnIOThread,
                       base::Unretained(this), base::RetainedRef(quota_manager),
                       special_storage_policy, std::move(storage_key_matcher),
                       perform_storage_cleanup, decrement_callback,
                       blink::mojom::StorageType::kSyncable));
  }

  DecrementTaskCountOnIO();
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::
    ClearBucketsOnIOThread(
        storage::QuotaManager* quota_manager,
        const scoped_refptr<storage::SpecialStoragePolicy>&
            special_storage_policy,
        StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
        bool perform_storage_cleanup,
        base::OnceClosure callback,
        blink::mojom::StorageType quota_storage_type,
        const std::set<storage::BucketLocator>& buckets) {
  // The QuotaManager manages all storage other than cookies, LocalStorage,
  // and SessionStorage. This loop wipes out most HTML5 storage for the given
  // storage keys.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (buckets.empty()) {
    std::move(callback).Run();
    return;
  }

  storage::QuotaClientTypes quota_client_types =
      StoragePartitionImpl::GenerateQuotaClientTypes(remove_mask_);

  // The logic below (via CheckQuotaManagedDataDeletionStatus) only
  // invokes the callback when all processing is complete.
  base::OnceClosure done_callback =
      perform_storage_cleanup
          ? base::BindOnce(&PerformQuotaManagerStorageCleanup,
                           base::WrapRefCounted(quota_manager),
                           quota_storage_type, quota_client_types,
                           std::move(callback))
          : std::move(callback);

  size_t* deletion_task_count = new size_t(0u);
  (*deletion_task_count)++;
  for (const auto& bucket : buckets) {
    // TODO(mkwst): Clean this up, it's slow. http://crbug.com/130746
    if (storage_key_.has_value() && bucket.storage_key != *storage_key_) {
      continue;
    }

    if (storage_key_matcher &&
        !storage_key_matcher.Run(bucket.storage_key,
                                 special_storage_policy.get())) {
      continue;
    }

    auto split_callback = base::SplitOnceCallback(std::move(done_callback));
    done_callback = std::move(split_callback.first);

    (*deletion_task_count)++;
    quota_manager->DeleteBucketData(
        bucket, quota_client_types,
        base::BindOnce(&OnQuotaManagedBucketDeleted, bucket,
                       deletion_task_count, std::move(split_callback.second)));
  }
  (*deletion_task_count)--;

  CheckQuotaManagedDataDeletionStatus(deletion_task_count,
                                      std::move(done_callback));
}

base::OnceClosure
StoragePartitionImpl::DataDeletionHelper::CreateTaskCompletionClosure(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto result = pending_tasks_.insert(data_type);
  DCHECK(result.second) << "Task already started: "
                        << static_cast<int>(data_type);

  static int tracing_id = 0;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "browsing_data", "StoragePartitionImpl",
      TRACE_ID_WITH_SCOPE("StoragePartitionImpl", ++tracing_id), "data_type",
      static_cast<int>(data_type));
  return base::BindOnce(
      &StoragePartitionImpl::DataDeletionHelper::OnTaskComplete,
      base::Unretained(this), data_type, tracing_id);
}

void StoragePartitionImpl::DataDeletionHelper::OnTaskComplete(
    TracingDataType data_type,
    int tracing_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DataDeletionHelper::OnTaskComplete,
                       base::Unretained(this), data_type, tracing_id));
    return;
  }
  size_t num_erased = pending_tasks_.erase(data_type);
  DCHECK_EQ(num_erased, 1U) << static_cast<int>(data_type);
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "browsing_data", "StoragePartitionImpl",
      TRACE_ID_WITH_SCOPE("StoragePartitionImpl", tracing_id));

  if (pending_tasks_.empty()) {
    std::move(callback_).Run();
    delete this;
  }
}

void StoragePartitionImpl::DataDeletionHelper::RecordUnfinishedSubTasks() {
  DCHECK(!pending_tasks_.empty());
  for (TracingDataType task : pending_tasks_) {
    base::UmaHistogramEnumeration(
        "History.ClearBrowsingData.Duration.SlowTasks180sStoragePartition",
        task);
  }
}

void StoragePartitionImpl::DataDeletionHelper::ClearDataOnUIThread(
    const blink::StorageKey& storage_key,
    BrowsingDataFilterBuilder* filter_builder,
    StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
    CookieDeletionFilterPtr cookie_deletion_filter,
    const base::FilePath& path,
    DOMStorageContextWrapper* dom_storage_context,
    storage::QuotaManager* quota_manager,
    storage::SpecialStoragePolicy* special_storage_policy,
    storage::FileSystemContext* filesystem_context,
    network::mojom::CookieManager* cookie_manager,
    InterestGroupManagerImpl* interest_group_manager,
    AttributionManager* attribution_manager,
    AggregationService* aggregation_service,
    PrivateAggregationManagerImpl* private_aggregation_manager,
    storage::SharedStorageManager* shared_storage_manager,
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    CdmStorageManager* cdm_storage_manager,
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end) {
  DCHECK_NE(remove_mask_, 0u);
  DCHECK(callback_);

  // Only one of `storage_key`'s origin and
  // `filter_builder`/`storage_key_policy_matcher` can be set.
  const bool storage_key_origin_empty = storage_key.origin().opaque();
  DCHECK(storage_key_origin_empty || !filter_builder);
  DCHECK(storage_key_origin_empty || storage_key_policy_matcher.is_null());

  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &StoragePartitionImpl::DataDeletionHelper::RecordUnfinishedSubTasks,
          weak_factory_.GetWeakPtr()),
      kSlowTaskTimeout);

  base::ScopedClosureRunner synchronous_clear_operations(
      CreateTaskCompletionClosure(TracingDataType::kSynchronous));

  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_ref =
      base::WrapRefCounted(special_storage_policy);

  StorageKeyMatcherFunction storage_key_matcher =
      filter_builder ? filter_builder->BuildStorageKeyFilter()
                     : StorageKeyMatcherFunction();

  // This is preferred for new storage APIs to reduce the complexity.
  auto generic_filter = CreateGenericStorageKeyMatcher(
      storage_key, storage_key_matcher, storage_key_policy_matcher,
      storage_policy_ref);

  auto combined_storage_key_matcher = CombineStorageKeyMatcherFunctions(
      storage_key_matcher, storage_key_policy_matcher);

  if (remove_mask_ & REMOVE_DATA_MASK_COOKIES) {
    // The CookieDeletionFilter has a redundant time interval to `begin` and
    // `end`. Ensure that the filter has no time interval specified to help
    // callers detect when they are using the wrong interval values.
    DCHECK(!cookie_deletion_filter->created_after_time.has_value());
    DCHECK(!cookie_deletion_filter->created_before_time.has_value());

    if (!begin.is_null()) {
      cookie_deletion_filter->created_after_time = begin;
    }
    if (!end.is_null()) {
      cookie_deletion_filter->created_before_time = end;
    }

    cookie_manager->DeleteCookies(
        std::move(cookie_deletion_filter),
        base::BindOnce(
            &OnClearedCookies,
            // Handle the cookie store being destroyed and the callback thus not
            // being called.
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                CreateTaskCompletionClosure(TracingDataType::kCookies))));
  }

  // It is not expected to only delete internal interest group data.
  DCHECK(!(remove_mask_ & REMOVE_DATA_MASK_INTEREST_GROUPS_INTERNAL) ||
         remove_mask_ & REMOVE_DATA_MASK_INTEREST_GROUPS);
  if (remove_mask_ & REMOVE_DATA_MASK_INTEREST_GROUPS) {
    if (interest_group_manager) {
      // The internal interest group data is not specific to a site so it only
      // makes sense to delete it for all sites (i.e. when
      // generic_filter.is_null()).
      if ((remove_mask_ & REMOVE_DATA_MASK_INTEREST_GROUPS_INTERNAL) &&
          generic_filter.is_null()) {
        interest_group_manager->DeleteAllInterestGroupData(
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                CreateTaskCompletionClosure(TracingDataType::kInterestGroups)));
      } else {
        interest_group_manager->DeleteInterestGroupData(
            generic_filter,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                CreateTaskCompletionClosure(TracingDataType::kInterestGroups)));
      }
    }
  }

  if (remove_mask_ & REMOVE_DATA_MASK_INTEREST_GROUP_PERMISSIONS_CACHE) {
    if (interest_group_manager) {
      interest_group_manager->ClearPermissionsCache();
    }
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if ((remove_mask_ & REMOVE_DATA_MASK_MEDIA_LICENSES) && cdm_storage_manager) {
    auto cdm_deletion_callback = base::BindOnce(
        base::IgnoreArgs<bool>(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            CreateTaskCompletionClosure(TracingDataType::kCdmStorage))));

    cdm_storage_manager->DeleteData(generic_filter, storage_key, begin, end,
                                    std::move(cdm_deletion_callback));
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // TODO(crbug.com/40272342): Remove REMOVE_DATA_MASK_MEDIA_LICENSES from here
  // when MediaLicense is removed from Quota types.
  if (remove_mask_ & REMOVE_DATA_MASK_INDEXEDDB ||
      remove_mask_ & REMOVE_DATA_MASK_WEBSQL ||
      remove_mask_ & REMOVE_DATA_MASK_FILE_SYSTEMS ||
      remove_mask_ & REMOVE_DATA_MASK_SERVICE_WORKERS ||
      remove_mask_ & REMOVE_DATA_MASK_CACHE_STORAGE ||
      remove_mask_ & REMOVE_DATA_MASK_MEDIA_LICENSES) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DataDeletionHelper::ClearQuotaManagedDataOnIOThread,
                       base::Unretained(this),
                       base::WrapRefCounted(quota_manager), begin, end,
                       storage_key, storage_policy_ref,
                       combined_storage_key_matcher, perform_storage_cleanup,
                       CreateTaskCompletionClosure(TracingDataType::kQuota)));
  }

  if (remove_mask_ & REMOVE_DATA_MASK_LOCAL_STORAGE) {
    ClearLocalStorageOnUIThread(
        base::WrapRefCounted(dom_storage_context), storage_policy_ref,
        combined_storage_key_matcher, storage_key, perform_storage_cleanup,
        begin, end,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            CreateTaskCompletionClosure(TracingDataType::kLocalStorage)));

    // ClearDataImpl cannot clear session storage data when a particular origin
    // is specified. Therefore we ignore clearing session storage in this case.
    // TODO(lazyboy): Fix.
    if (storage_key_origin_empty) {
      // TODO(crbug.com/41457196): Sometimes SessionStorage fails to call its
      // callback. Figure out why.
      ClearSessionStorageOnUIThread(
          base::WrapRefCounted(dom_storage_context), storage_policy_ref,
          combined_storage_key_matcher, perform_storage_cleanup,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              CreateTaskCompletionClosure(TracingDataType::kSessionStorage)));
    }
  }

  if (remove_mask_ & REMOVE_DATA_MASK_SHADER_CACHE) {
    gpu::GpuDiskCacheFactory* gpu_cache_factory =
        GetGpuDiskCacheFactorySingleton();
    // May be null in tests where it is difficult to plumb through a test
    // storage partition.
    if (!path.empty() && gpu_cache_factory) {
      // Clear the path for all the different GPU cache sub-types.
      base::RepeatingClosure barrier = base::BarrierClosure(
          gpu::kGpuDiskCacheTypes.size(),
          CreateTaskCompletionClosure(TracingDataType::kGpuCache));
      for (gpu::GpuDiskCacheType type : gpu::kGpuDiskCacheTypes) {
        gpu_cache_factory->ClearByPath(
            path.Append(gpu::GetGpuDiskCacheSubdir(type)), begin, end,
            base::BindOnce(&ClearedGpuCache, barrier));
      }
    }
  }

  // It is not expected to only delete internal attribution reporting data.
  DCHECK(!(remove_mask_ & REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_INTERNAL) ||
         remove_mask_ & REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_SITE_CREATED);
  if (attribution_manager &&
      (remove_mask_ & REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_SITE_CREATED)) {
    if (storage_key_origin_empty) {
      attribution_manager->ClearData(
          begin, end, generic_filter, filter_builder,
          remove_mask_ & REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_INTERNAL,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              CreateTaskCompletionClosure(TracingDataType::kConversions)));
    } else if (storage_key.IsFirstPartyContext()) {
      // Attribution Reporting API doesn't support cross-site data deletion.
      std::unique_ptr<BrowsingDataFilterBuilder> effective_filter_builder =
          BrowsingDataFilterBuilder::Create(
              BrowsingDataFilterBuilder::Mode::kDelete);
      effective_filter_builder->AddOrigin(storage_key.origin());
      attribution_manager->ClearData(
          begin, end, generic_filter, effective_filter_builder.get(),
          remove_mask_ & REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_INTERNAL,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              CreateTaskCompletionClosure(TracingDataType::kConversions)));
    }
  }

  if (aggregation_service &&
      (remove_mask_ & REMOVE_DATA_MASK_AGGREGATION_SERVICE)) {
    // Currently the aggregation service only stores public keys and we don't
    // have information on the page/context that uses the public key origin,
    // therefore we don't check origins and instead just delete all rows in the
    // given time range.
    // TODO(crbug.com/40210305): Consider fine-grained deletion of public keys.
    // TODO(crbug.com/40815455): Consider adding aggregation service origins to
    // `CookiesTreeModel`.
    aggregation_service->ClearData(
        begin, end, generic_filter,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            CreateTaskCompletionClosure(TracingDataType::kAggregationService)));
  }

  if (private_aggregation_manager &&
      (remove_mask_ & REMOVE_DATA_MASK_PRIVATE_AGGREGATION_INTERNAL)) {
    private_aggregation_manager->ClearBudgetData(
        begin, end, generic_filter,

        // Wrapping the callback ensures that the callback is still run in the
        // case that the storage partition is deleted before the task is posted.
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            CreateTaskCompletionClosure(TracingDataType::kPrivateAggregation)));
  }

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI) &&
      shared_storage_manager &&
      (remove_mask_ & REMOVE_DATA_MASK_SHARED_STORAGE)) {
    auto shared_storage_purge_callback = base::BindOnce(
        [](base::WeakPtr<storage::SharedStorageManager> manager,
           base::OnceClosure callback,
           storage::SharedStorageDatabase::OperationResult result) {
          if (manager) {
            manager->OnOperationResult(result);
          }
          std::move(callback).Run();
        },
        shared_storage_manager->GetWeakPtr(),
        CreateTaskCompletionClosure(TracingDataType::kSharedStorage));

    shared_storage_manager->PurgeMatchingOrigins(
        combined_storage_key_matcher, begin, end,
        std::move(shared_storage_purge_callback), perform_storage_cleanup);
  }
}

void StoragePartitionImpl::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(initialized_);
  CookieDeletionFilterPtr deletion_filter = CookieDeletionFilter::New();
  if (!storage_origin.host().empty()) {
    deletion_filter->host_name = storage_origin.host();
  }
  // Construct a |BrowsingDataFilterBuilder| instead of just passing a storage
  // key based on the origin directly. This is needed to be able to delete the
  // associated 3P data embedded on the origin.
  auto filter_builder = BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(url::Origin::Create(storage_origin));
  ClearDataImpl(remove_mask, quota_storage_remove_mask, blink::StorageKey(),
                filter_builder.get(), StorageKeyPolicyMatcherFunction(),
                std::move(deletion_filter), false, base::Time(),
                base::Time::Max(), std::move(callback));
}

void StoragePartitionImpl::ClearDataForBuckets(
    const blink::StorageKey& storage_key,
    const std::set<std::string>& storage_buckets,
    base::OnceClosure callback) {
  DCHECK(initialized_);

  const auto remove_buckets_done =
      base::BarrierCallback<blink::mojom::QuotaStatusCode>(
          storage_buckets.size(),
          BindPostTaskToCurrentDefault(
              base::BindOnce(&StoragePartitionImpl::ClearDataForBucketsDone,
                             base::Unretained(this), storage_key,
                             storage_buckets, std::move(callback))));

  storage::QuotaManagerProxy* quota_manager_proxy = GetQuotaManagerProxy();

  for (const auto& bucket : storage_buckets) {
    quota_manager_proxy->DeleteBucket(
        storage_key, bucket, base::SequencedTaskRunner::GetCurrentDefault(),
        remove_buckets_done);
  }
}

void StoragePartitionImpl::ClearDataForBucketsDone(
    const blink::StorageKey& storage_key,
    const std::set<std::string>& storage_buckets,
    base::OnceClosure callback,
    const std::vector<blink::mojom::QuotaStatusCode>& status_codes) {
  auto bucket_iterator = storage_buckets.begin();

  for (const auto status_code : status_codes) {
    if (bucket_iterator == storage_buckets.end()) {
      break;
    }

    const std::string bucket_name = *bucket_iterator;

    if (status_code != blink::mojom::QuotaStatusCode::kOk) {
      DLOG(ERROR) << "Couldn't remove bucket with name" << bucket_name
                  << " with storage key " << storage_key.GetDebugString()
                  << ". Status: " << static_cast<int>(status_code);
    }

    ++bucket_iterator;
  }

  std::move(callback).Run();
}

void StoragePartitionImpl::ClearData(uint32_t remove_mask,
                                     uint32_t quota_storage_remove_mask,
                                     const blink::StorageKey& storage_key,
                                     const base::Time begin,
                                     const base::Time end,
                                     base::OnceClosure callback) {
  DCHECK(initialized_);
  CookieDeletionFilterPtr deletion_filter = CookieDeletionFilter::New();
  if (!storage_key.origin().host().empty()) {
    deletion_filter->host_name = storage_key.origin().host();
  }
  bool perform_storage_cleanup =
      begin.is_null() && end.is_max() && storage_key.origin().opaque();
  ClearDataImpl(remove_mask, quota_storage_remove_mask, storage_key,
                /*filter_builder=*/nullptr, StorageKeyPolicyMatcherFunction(),
                std::move(deletion_filter), perform_storage_cleanup, begin, end,
                std::move(callback));
}

void StoragePartitionImpl::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    BrowsingDataFilterBuilder* filter_builder,
    StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
    network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  DCHECK(initialized_);
  ClearDataImpl(remove_mask, quota_storage_remove_mask, blink::StorageKey(),
                filter_builder, std::move(storage_key_policy_matcher),
                std::move(cookie_deletion_filter), perform_storage_cleanup,
                begin, end, std::move(callback));
}

void StoragePartitionImpl::ClearCodeCaches(
    const base::Time begin,
    const base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {
  DCHECK(initialized_);
  // StoragePartitionCodeCacheDataRemover deletes itself when it is done.
  StoragePartitionCodeCacheDataRemover::Create(this, url_matcher, begin, end)
      ->Remove(std::move(callback));
}

void StoragePartitionImpl::Flush() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(initialized_);
  if (GetDOMStorageContext()) {
    GetDOMStorageContext()->Flush();
  }
}

void StoragePartitionImpl::ResetURLLoaderFactories() {
  CHECK(initialized_);
  GetNetworkContext()->ResetURLLoaderFactories();
  shared_url_loader_factory_for_browser_process_->factory()->Reset();
  shared_url_loader_factory_for_browser_process_->factory_for_io_thread()
      ->Reset();
}

void StoragePartitionImpl::ClearBluetoothAllowedDevicesMapForTesting() {
  DCHECK(initialized_);
  bluetooth_allowed_devices_map_->Clear();
}

void StoragePartitionImpl::AddObserver(DataRemovalObserver* observer) {
  data_removal_observers_.AddObserver(observer);
}

void StoragePartitionImpl::RemoveObserver(DataRemovalObserver* observer) {
  data_removal_observers_.RemoveObserver(observer);
}

void StoragePartitionImpl::FlushNetworkInterfaceForTesting() {
  CHECK(initialized_);
  DCHECK(network_context_owner_->network_context);
  network_context_owner_->network_context.FlushForTesting();  // IN-TEST
  shared_url_loader_factory_for_browser_process_->factory()
      ->FlushForTesting();  // IN-TEST
  if (cookie_manager_for_browser_process_) {
    cookie_manager_for_browser_process_.FlushForTesting();  // IN-TEST
  }
}

void StoragePartitionImpl::FlushNetworkInterfaceOnIOThreadForTesting() {
  CHECK(initialized_);
  CHECK(shared_url_loader_factory_for_browser_process_);
  shared_url_loader_factory_for_browser_process_->factory_for_io_thread()
      ->FlushForTesting();  // IN-TEST
}

void StoragePartitionImpl::FlushCertVerifierInterfaceForTesting() {
  DCHECK(initialized_);
  DCHECK(cert_verifier_service_updater_);
  cert_verifier_service_updater_.FlushForTesting();  // IN-TEST
}

void StoragePartitionImpl::WaitForDeletionTasksForTesting() {
  DCHECK(initialized_);
  if (deletion_helpers_running_) {
    base::RunLoop loop;
    on_deletion_helpers_done_callback_ = loop.QuitClosure();
    loop.Run();
  }
}

void StoragePartitionImpl::WaitForCodeCacheShutdownForTesting() {
  DCHECK(initialized_);
  if (generated_code_cache_context_) {
    // If this is still running its initialization task it may check
    // enabled features on a sequenced worker pool which could race
    // between ScopedFeatureList destruction.
    base::RunLoop loop;
    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_, FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<GeneratedCodeCacheContext> context,
               base::OnceClosure quit) {
              context->generated_js_code_cache()->GetBackend(base::BindOnce(
                  [](base::OnceClosure quit, disk_cache::Backend*) {
                    std::move(quit).Run();
                  },
                  std::move(quit)));
            },
            generated_code_cache_context_, loop.QuitClosure()));
    loop.Run();
    generated_code_cache_context_->Shutdown();
  }
}

void StoragePartitionImpl::SetNetworkContextForTesting(
    mojo::PendingRemote<network::mojom::NetworkContext>
        network_context_remote) {
  network_context_owner_->network_context.reset();
  network_context_owner_->network_context.Bind(
      std::move(network_context_remote));
}

void StoragePartitionImpl::OverrideDeleteStaleSessionOnlyCookiesDelayForTesting(
    const base::TimeDelta& delay) {
  delete_stale_session_only_cookies_delay_ = delay;
}

void StoragePartitionImpl::SetClearNoncesInNetworkContextParamsForTesting(
    const base::TimeDelta& delay,
    base::RepeatingClosure callback) {
  clear_nonces_in_network_context_delay_ = delay;
  clear_nonces_in_network_context_callback_for_testing_ = callback;
}

base::WeakPtr<StoragePartitionImpl> StoragePartitionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

BrowserContext* StoragePartitionImpl::browser_context() const {
  return browser_context_;
}

storage::mojom::Partition* StoragePartitionImpl::GetStorageServicePartition() {
  if (!remote_partition_) {
    std::optional<base::FilePath> storage_path;
    if (!is_in_memory()) {
      storage_path =
          browser_context_->GetPath().Append(relative_partition_path_);
    }
    GetStorageServiceRemote()->BindPartition(
        storage_path, remote_partition_.BindNewPipeAndPassReceiver());
    remote_partition_.set_disconnect_handler(
        base::BindOnce(&StoragePartitionImpl::OnStorageServiceDisconnected,
                       base::Unretained(this)));
  }
  return remote_partition_.get();
}

// static
mojo::Remote<storage::mojom::StorageService>&
StoragePartitionImpl::GetStorageServiceForTesting() {
  return GetStorageServiceRemote();
}

void StoragePartitionImpl::BindIndexedDB(
    const storage::BucketLocator& bucket_locator,
    const storage::BucketClientInfo& client_info,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  indexed_db_control_wrapper_->BindIndexedDB(
      bucket_locator, client_info, std::move(client_state_checker_remote),
      std::move(receiver));
}

mojo::ReceiverId StoragePartitionImpl::BindDomStorage(
    int process_id,
    mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
    mojo::PendingRemote<blink::mojom::DomStorageClient> client) {
  DCHECK(initialized_);
  auto handle =
      ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(process_id);
  mojo::ReceiverId id = dom_storage_receivers_.Add(
      this, std::move(receiver),
      std::make_unique<SecurityPolicyHandle>(std::move(handle)));
  dom_storage_clients_[id].Bind(std::move(client));
  return id;
}

void StoragePartitionImpl::UnbindDomStorage(mojo::ReceiverId receiver_id) {
  DCHECK(initialized_);
  dom_storage_receivers_.Remove(receiver_id);
  dom_storage_clients_.erase(receiver_id);
}

void StoragePartitionImpl::OverrideQuotaManagerForTesting(
    storage::QuotaManager* quota_manager) {
  DCHECK(initialized_);
  quota_manager_ = quota_manager;
}

void StoragePartitionImpl::OverrideSpecialStoragePolicyForTesting(
    storage::SpecialStoragePolicy* special_storage_policy) {
  DCHECK(initialized_);
  special_storage_policy_ = special_storage_policy;
}

void StoragePartitionImpl::ShutdownBackgroundSyncContextForTesting() {
  DCHECK(initialized_);
  if (GetBackgroundSyncContext()) {
    GetBackgroundSyncContext()->Shutdown();
  }
}

void StoragePartitionImpl::OverrideBackgroundSyncContextForTesting(
    BackgroundSyncContextImpl* background_sync_context) {
  DCHECK(initialized_);
  DCHECK(!GetBackgroundSyncContext() ||
         !GetBackgroundSyncContext()->background_sync_manager());
  background_sync_context_ = background_sync_context;
}

void StoragePartitionImpl::OverrideSharedWorkerServiceForTesting(
    std::unique_ptr<SharedWorkerServiceImpl> shared_worker_service) {
  DCHECK(initialized_);
  shared_worker_service_ = std::move(shared_worker_service);
}

void StoragePartitionImpl::OverrideSharedStorageWorkletHostManagerForTesting(
    std::unique_ptr<SharedStorageWorkletHostManager>
        shared_storage_worklet_host_manager) {
  DCHECK(initialized_);
  shared_storage_worklet_host_manager_ =
      std::move(shared_storage_worklet_host_manager);
}

void StoragePartitionImpl::OverrideSharedStorageHeaderObserverForTesting(
    std::unique_ptr<SharedStorageHeaderObserver>
        shared_storage_header_observer) {
  DCHECK(initialized_);
  shared_storage_header_observer_ = std::move(shared_storage_header_observer);
}

void StoragePartitionImpl::OverrideAggregationServiceForTesting(
    std::unique_ptr<AggregationService> aggregation_service) {
  DCHECK(initialized_);
  aggregation_service_ = std::move(aggregation_service);
}

void StoragePartitionImpl::OverrideAttributionManagerForTesting(
    std::unique_ptr<AttributionManager> attribution_manager) {
  DCHECK(initialized_);
  attribution_manager_ = std::move(attribution_manager);
}

void StoragePartitionImpl::OverridePrivateAggregationManagerForTesting(
    std::unique_ptr<PrivateAggregationManagerImpl>
        private_aggregation_manager) {
  DCHECK(initialized_);
  private_aggregation_manager_ = std::move(private_aggregation_manager);
}

void StoragePartitionImpl::GetQuotaSettings(
    storage::OptionalQuotaSettingsCallback callback) {
  if (g_test_quota_settings) {
    // For debugging tests harness can inject settings.
    std::move(callback).Run(*g_test_quota_settings);
    return;
  }

  storage::GetNominalDynamicSettings(
      GetPath(), browser_context_->IsOffTheRecord(),
      storage::GetDefaultDeviceInfoHelper(), std::move(callback));
}

void StoragePartitionImpl::InitNetworkContext() {
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  cert_verifier::mojom::CertVerifierCreationParamsPtr
      cert_verifier_creation_params =
          cert_verifier::mojom::CertVerifierCreationParams::New();
  GetContentClient()->browser()->ConfigureNetworkContextParams(
      browser_context_, is_in_memory(), relative_partition_path_,
      context_params.get(), cert_verifier_creation_params.get());
  // Should be initialized with existing per-profile CORS access lists.
  DCHECK(context_params->cors_origin_access_list.empty())
      << "NetworkContextParams::cors_origin_access_list should be populated "
         "via SharedCorsOriginAccessList";
  context_params->cors_origin_access_list =
      browser_context_->GetSharedCorsOriginAccessList()
          ->GetOriginAccessList()
          .CreateCorsOriginAccessPatternsList();
  devtools_instrumentation::ApplyNetworkContextParamsOverrides(
      browser_context_, context_params.get());
  DCHECK(!context_params->cert_verifier_params)
      << "`cert_verifier_params` should not be set in the "
         "NetworkContextParams, as they will be replaced with a new pipe to "
         "the CertVerifierService.";

  cert_verifier_service_updater_.reset();
  context_params->cert_verifier_params = GetCertVerifierParamsWithUpdater(
      std::move(cert_verifier_creation_params),
      cert_verifier_service_updater_.BindNewPipeAndPassReceiver());

  // This mechanisms should be used only for legacy internal headers. You can
  // find a recommended alternative approach on URLRequest::cors_exempt_headers
  // at services/network/public/mojom/url_loader.mojom.
  context_params->cors_exempt_header_list.push_back(
      kCorsExemptPurposeHeaderName);
  context_params->cors_exempt_header_list.push_back(
      GetCorsExemptRequestedWithHeaderName());
  variations::UpdateCorsExemptHeaderForVariations(context_params.get());

  cors_exempt_header_list_ = context_params->cors_exempt_header_list;

  if (base::FeatureList::IsEnabled(
          network::features::kCompressionDictionaryTransportBackend) &&
      GetContentClient()->browser()->AllowCompressionDictionaryTransport(
          browser_context_)) {
    context_params->shared_dictionary_enabled = true;
    if (!is_in_memory()) {
      // Some callers may already initialize NetworkContextFilePaths, and we
      // don't want to overwrite them.
      if (!context_params->file_paths) {
        context_params->file_paths =
            network::mojom::NetworkContextFilePaths::New();
      }
      context_params->file_paths->shared_dictionary_directory =
          partition_path_.Append(FILE_PATH_LITERAL("Shared Dictionary"));
    }
    if (context_params->shared_dictionary_cache_max_size == 0u) {
      CalculateAndSetSharedDictionaryCacheMaxSize(
          GetWeakPtr(), is_in_memory() ? base::FilePath() : partition_path_);
    }
  }

  if (cookie_deprecation_label_manager_) {
    context_params->cookie_deprecation_label =
        cookie_deprecation_label_manager_->GetValue().value_or("");
  }

  network_context_owner_->network_context.reset();
  CreateNetworkContextInNetworkService(
      network_context_owner_->network_context.BindNewPipeAndPassReceiver(),
      std::move(context_params));
  DCHECK(network_context_owner_->network_context);

  // Restore the saved network revocation nonces. This allows fenced frames'
  // untrusted network access states to be persisted in case of a
  // `NetworkService` crash.
  std::vector<base::UnguessableToken> nonces(
      std::begin(network_revocation_nonces_),
      std::end(network_revocation_nonces_));
  network_context_owner_->network_context->RevokeNetworkForNonces(
      nonces, base::NullCallback());

  network_context_client_receiver_.reset();
  network_context_owner_->network_context->SetClient(
      network_context_client_receiver_.BindNewPipeAndPassRemote());
  network_context_owner_->network_context.set_disconnect_handler(base::BindOnce(
      &StoragePartitionImpl::InitNetworkContext, weak_factory_.GetWeakPtr()));
}

network::mojom::URLLoaderFactoryParamsPtr
StoragePartitionImpl::CreateURLLoaderFactoryParams() {
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  // This method is used for browser-process initiated requests for which there
  // is no corresponding RenderProcessHost.
  params->process_id = network::mojom::kBrowserProcessId;
  params->automatically_assign_isolation_info = true;
  params->is_orb_enabled = false;
  params->is_trusted = true;
  params->url_loader_network_observer =
      CreateAuthCertObserverForServiceWorker(params->process_id);
  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  return params;
}

void StoragePartitionImpl::CreateURLLoaderFactoryForBrowserProcessInternal(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  network::URLLoaderFactoryBuilder factory_builder;
  if (url_loader_factory::GetTestingInterceptor()) {
    url_loader_factory::GetTestingInterceptor().Run(
        network::mojom::kBrowserProcessId, factory_builder);
  }
  *out_factory =
      std::move(factory_builder)
          .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
              GetNetworkContext(), CreateURLLoaderFactoryParams());
}

void StoragePartition::SetDefaultQuotaSettingsForTesting(
    const storage::QuotaSettings* settings) {
  g_test_quota_settings = settings;
}

mojo::PendingRemote<network::mojom::CookieAccessObserver>
StoragePartitionImpl::CreateCookieAccessObserverForServiceWorker() {
  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  service_worker_cookie_observers_.Add(
      std::make_unique<ServiceWorkerCookieAccessObserver>(this),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
StoragePartitionImpl::CreateTrustTokenAccessObserverForServiceWorker() {
  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver> remote;
  service_worker_trust_token_observers_.Add(
      std::make_unique<ServiceWorkerTrustTokenAccessObserver>(this),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
StoragePartitionImpl::CreateSharedDictionaryAccessObserverForServiceWorker() {
  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver> remote;
  service_worker_shared_dictionary_observers_.Add(
      std::make_unique<ServiceWorkerSharedDictionaryAccessObserver>(this),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void StoragePartitionImpl::OpenLocalStorageForProcess(
    int process_id,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(initialized_);
  auto handle =
      ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(process_id);
  dom_storage_context_->OpenLocalStorage(storage_key, std::nullopt,
                                         std::move(receiver), std::move(handle),
                                         base::DoNothing());
}

void StoragePartitionImpl::BindSessionStorageAreaForProcess(
    int process_id,
    const blink::StorageKey& storage_key,
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(initialized_);
  auto handle =
      ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(process_id);
  dom_storage_context_->BindStorageArea(storage_key, std::nullopt, namespace_id,
                                        std::move(receiver), std::move(handle),
                                        base::DoNothing());
}

std::optional<blink::StorageKey> StoragePartitionImpl::CalculateStorageKey(
    const url::Origin& origin,
    const base::UnguessableToken* nonce) {
  if (!blink::StorageKey::IsThirdPartyStoragePartitioningEnabled()) {
    return std::nullopt;
  }

  NavigationOrDocumentHandle* handle =
      url_loader_network_observers_.current_context().navigation_or_document();
  if (!handle) {
    return std::nullopt;
  }
  FrameTreeNode* node = handle->GetFrameTreeNode();
  if (!node) {
    return std::nullopt;
  }
  RenderFrameHostImpl* frame_host = node->current_frame_host();
  if (!frame_host) {
    return std::nullopt;
  }

  return frame_host->CalculateStorageKey(origin, nonce);
}

GlobalRenderFrameHostId
StoragePartitionImpl::GetRenderFrameHostIdFromNetworkContext() {
  URLLoaderNetworkContext context =
      url_loader_network_observers_.current_context();

  // `navigation_or_document()` can be null for `kServiceWorkerContext`.
  RenderFrameHost* render_frame_host =
      context.navigation_or_document()
          ? context.navigation_or_document()->GetDocument()
          : nullptr;

  // It can pass empty GlobalRenderFrameHostId() when the context type is
  // not `kRenderFrameHostContext`.
  return render_frame_host ? render_frame_host->GetGlobalId()
                           : GlobalRenderFrameHostId();
}

StoragePartitionImpl::URLLoaderNetworkContext::URLLoaderNetworkContext(
    GlobalRenderFrameHostId render_frame_host_id)
    : type_(Type::kRenderFrameHostContext) {
  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id);
  if (!render_frame_host) {
    return;
  }

  navigation_or_document_ = render_frame_host->GetNavigationOrDocumentHandle();
}

StoragePartitionImpl::URLLoaderNetworkContext::URLLoaderNetworkContext(
    NavigationRequest& navigation_request)
    : type_(Type::kNavigationRequestContext) {
  navigation_or_document_ = navigation_request.navigation_or_document_handle();
}

StoragePartitionImpl::URLLoaderNetworkContext::URLLoaderNetworkContext(
    int process_id)
    : type_(Type::kServiceWorkerContext), process_id_(process_id) {}

StoragePartitionImpl::URLLoaderNetworkContext::URLLoaderNetworkContext(
    const URLLoaderNetworkContext& other) = default;

StoragePartitionImpl::URLLoaderNetworkContext&
StoragePartitionImpl::URLLoaderNetworkContext::operator=(
    const URLLoaderNetworkContext& other) = default;

StoragePartitionImpl::URLLoaderNetworkContext::~URLLoaderNetworkContext() =
    default;

StoragePartitionImpl::URLLoaderNetworkContext
StoragePartitionImpl::URLLoaderNetworkContext::CreateForRenderFrameHost(
    GlobalRenderFrameHostId render_frame_host_id) {
  return StoragePartitionImpl::URLLoaderNetworkContext(
      render_frame_host_id);
}

StoragePartitionImpl::URLLoaderNetworkContext
StoragePartitionImpl::URLLoaderNetworkContext::CreateForNavigation(
    NavigationRequest& navigation_request) {
  return StoragePartitionImpl::URLLoaderNetworkContext(navigation_request);
}

bool StoragePartitionImpl::URLLoaderNetworkContext::IsNavigationRequestContext()
    const {
  return type_ == ContextType::kNavigationRequestContext;
}

// Returns the WebContents corresponding to `context`.
WebContents* StoragePartitionImpl::URLLoaderNetworkContext::GetWebContents() {
  if (!navigation_or_document()) {
    return nullptr;
  }
  return navigation_or_document()->GetWebContents();
}

// Returns true if the request is the primary main frame navigation.
bool StoragePartitionImpl::URLLoaderNetworkContext::
    IsPrimaryMainFrameRequest() {
  if (!IsNavigationRequestContext()) {
    return false;
  }

  return navigation_or_document()->IsInPrimaryMainFrame();
}

}  // namespace content
