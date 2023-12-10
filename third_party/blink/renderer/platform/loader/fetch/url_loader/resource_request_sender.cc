// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/url_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/loader/inter_process_time_ticks_converter.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/mojo_url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace WTF {

template <>
struct CrossThreadCopier<
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<net::NetworkTrafficAnnotationTag>
    : public CrossThreadCopierPassThrough<net::NetworkTrafficAnnotationTag> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = net::NetworkTrafficAnnotationTag;
  static const Type& Copy(const Type& traffic_annotation) {
    return traffic_annotation;
  }
};

template <>
struct CrossThreadCopier<blink::WebVector<blink::WebString>>
    : public CrossThreadCopierPassThrough<blink::WebVector<blink::WebString>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

#if BUILDFLAG(IS_WIN)
// Converts |time| from a remote to local TimeTicks, overwriting the original
// value.
void RemoteToLocalTimeTicks(const InterProcessTimeTicksConverter& converter,
                            base::TimeTicks* time) {
  RemoteTimeTicks remote_time = RemoteTimeTicks::FromTimeTicks(*time);
  *time = converter.ToLocalTimeTicks(remote_time).ToTimeTicks();
}
#endif

void CheckSchemeForReferrerPolicy(const network::ResourceRequest& request) {
  if ((request.referrer_policy ==
           ReferrerUtils::GetDefaultNetReferrerPolicy() ||
       request.referrer_policy ==
           net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE) &&
      request.referrer.SchemeIsCryptographic() &&
      !url::Origin::Create(request.url).opaque() &&
      !network::IsUrlPotentiallyTrustworthy(request.url)) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request.url << "\n"
               << "URL's Origin = "
               << url::Origin::Create(request.url).Serialize() << "\n"
               << "Referrer = " << request.referrer;
  }
}

// Determines if the loader should be restarted on a redirect using
// ThrottlingURLLoader::FollowRedirectForcingRestart.
bool RedirectRequiresLoaderRestart(const GURL& original_url,
                                   const GURL& redirect_url) {
  // Restart is needed if the URL is no longer handled by network service.
  if (network::IsURLHandledByNetworkService(original_url)) {
    return !network::IsURLHandledByNetworkService(redirect_url);
  }

  // If URL wasn't originally handled by network service, restart is needed if
  // schemes are different.
  return original_url.scheme_piece() != redirect_url.scheme_piece();
}

bool ShouldFetchCodeCache(const network::ResourceRequest& request) {
  // Since code cache requests use a per-frame interface, don't fetch cached
  // code for keep-alive requests. These are only used for beaconing and we
  // don't expect code cache to help there.
  if (request.keepalive) {
    return false;
  }

  // Aside from http and https, the only other supported protocols are those
  // listed in the SchemeRegistry as requiring a content equality check.
  bool should_use_source_hash =
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
          String(request.url.scheme()));
  if (!request.url.SchemeIsHTTPOrHTTPS() && !should_use_source_hash) {
    return false;
  }

  // Supports script resource requests.
  // TODO(crbug.com/964467): Currently Chrome doesn't support code cache for
  // dedicated worker, shared worker, audio worklet and paint worklet. For
  // the service worker scripts, Blink receives the code cache via
  // URLLoaderClient::OnReceiveResponse() IPC.
  if (request.destination == network::mojom::RequestDestination::kScript) {
    return true;
  }

  // WebAssembly module request have RequestDestination::kEmpty. Note that
  // we always perform a code fetch for all of these requests because:
  //
  // * It is not easy to distinguish WebAssembly modules from other kEmpty
  //   requests
  // * The fetch might be handled by Service Workers, but we can't still know
  //   if the response comes from the CacheStorage (in such cases its own
  //   code cache will be used) or not.
  //
  // These fetches should be cheap, however, requiring one additional IPC and
  // no browser process disk IO since the cache index is in memory and the
  // resource key should not be present.
  //
  // The only case where it's easy to skip a kEmpty request is when a content
  // equality check is required, because only ScriptResource supports that
  // requirement.
  if (request.destination == network::mojom::RequestDestination::kEmpty &&
      !should_use_source_hash) {
    return true;
  }
  return false;
}

mojom::blink::CodeCacheType GetCodeCacheType(
    network::mojom::RequestDestination destination) {
  if (destination == network::mojom::RequestDestination::kEmpty) {
    // For requests initiated by the fetch function, we use code cache for
    // WASM compiled code.
    return mojom::blink::CodeCacheType::kWebAssembly;
  } else {
    // Otherwise, we use code cache for scripting.
    return mojom::blink::CodeCacheType::kJavascript;
  }
}

bool ShouldUseIsolatedCodeCache(
    const network::mojom::URLResponseHead& response_head,
    const KURL& initial_url,
    const KURL& current_url,
    base::Time code_cache_response_time) {
  // We only support code cache for other service worker provided
  // resources when a direct pass-through fetch handler is used. If the service
  // worker synthesizes a new Response or provides a Response fetched from a
  // different URL, then do not use the code cache.
  // Also, responses coming from cache storage use a separate code cache
  // mechanism.
  if (response_head.was_fetched_via_service_worker) {
    // Do the same check as !ResourceResponse::IsServiceWorkerPassThrough().
    if (!response_head.cache_storage_cache_name.empty()) {
      // Responses was produced by cache_storage
      return false;
    }
    if (response_head.url_list_via_service_worker.empty()) {
      // Response was synthetically constructed.
      return false;
    }
    if (KURL(response_head.url_list_via_service_worker.back()) != current_url) {
      // Response was fetched from different URLs.
      return false;
    }
  }
  if (SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
          initial_url.Protocol())) {
    // This resource should use a source text hash rather than a response time
    // comparison.
    if (!SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
            current_url.Protocol())) {
      // This kind of Resource doesn't support requiring a hash, so we can't
      // send cached code to it.
      return false;
    }
  } else if (!response_head.should_use_source_hash_for_js_code_cache) {
    // If the timestamps don't match or are null, the code cache data may be
    // for a different response. See https://crbug.com/1099587.
    if (code_cache_response_time.is_null() ||
        response_head.response_time.is_null() ||
        code_cache_response_time != response_head.response_time) {
      return false;
    }
  }
  return true;
}

}  // namespace

class ResourceRequestSender::CodeCacheFetcher
    : public WTF::RefCounted<ResourceRequestSender::CodeCacheFetcher> {
 public:
  static scoped_refptr<CodeCacheFetcher> TryCreateAndStart(
      const network::ResourceRequest& request,
      CodeCacheHost& code_cache_host,
      base::OnceClosure done_closure);

  CodeCacheFetcher(CodeCacheHost& code_cache_host,
                   mojom::blink::CodeCacheType code_cache_type,
                   const GURL& url,
                   base::OnceClosure done_closure);

  CodeCacheFetcher(const CodeCacheFetcher&) = delete;
  CodeCacheFetcher& operator=(const CodeCacheFetcher&) = delete;

  bool is_waiting() const { return is_waiting_; }

  void SetCurrentUrl(const GURL& new_url) { current_url_ = KURL(new_url); }
  void DidReceiveCachedMetadataFromUrlLoader();
  absl::optional<mojo_base::BigBuffer> TakeCodeCacheForResponse(
      const network::mojom::URLResponseHead& response_head);

 private:
  friend class WTF::RefCounted<CodeCacheFetcher>;
  ~CodeCacheFetcher() = default;

  void Start();

  void DidReceiveCachedCode(base::Time response_time,
                            mojo_base::BigBuffer data);

  void ClearCodeCacheEntryIfPresent();

  base::WeakPtr<CodeCacheHost> code_cache_host_;
  mojom::blink::CodeCacheType code_cache_type_;
  const KURL initial_url_;
  KURL current_url_;
  base::OnceClosure done_closure_;

  bool is_waiting_ = true;
  bool did_receive_cached_metadata_from_url_loader_ = false;
  absl::optional<mojo_base::BigBuffer> code_cache_data_;
  base::Time code_cache_response_time_;
};

// static
scoped_refptr<ResourceRequestSender::CodeCacheFetcher>
ResourceRequestSender::CodeCacheFetcher::TryCreateAndStart(
    const network::ResourceRequest& request,
    CodeCacheHost& code_cache_host,
    base::OnceClosure done_closure) {
  if (!ShouldFetchCodeCache(request)) {
    return nullptr;
  }
  auto fetcher = base::MakeRefCounted<ResourceRequestSender::CodeCacheFetcher>(
      code_cache_host, GetCodeCacheType(request.destination), request.url,
      std::move(done_closure));
  fetcher->Start();
  return fetcher;
}

ResourceRequestSender::CodeCacheFetcher::CodeCacheFetcher(
    CodeCacheHost& code_cache_host,
    mojom::blink::CodeCacheType code_cache_type,
    const GURL& url,
    base::OnceClosure done_closure)
    : code_cache_host_(code_cache_host.GetWeakPtr()),
      code_cache_type_(code_cache_type),
      initial_url_(url),
      current_url_(url),
      done_closure_(std::move(done_closure)) {}

void ResourceRequestSender::CodeCacheFetcher::Start() {
  CHECK(code_cache_host_);
  (*code_cache_host_)
      ->FetchCachedCode(code_cache_type_, KURL(initial_url_),
                        WTF::BindOnce(&CodeCacheFetcher::DidReceiveCachedCode,
                                      base::WrapRefCounted(this)));
}

void ResourceRequestSender::CodeCacheFetcher::
    DidReceiveCachedMetadataFromUrlLoader() {
  did_receive_cached_metadata_from_url_loader_ = true;
  if (!is_waiting_) {
    ClearCodeCacheEntryIfPresent();
  }
}

absl::optional<mojo_base::BigBuffer>
ResourceRequestSender::CodeCacheFetcher::TakeCodeCacheForResponse(
    const network::mojom::URLResponseHead& response_head) {
  CHECK(!is_waiting_);
  if (!ShouldUseIsolatedCodeCache(response_head, initial_url_, current_url_,
                                  code_cache_response_time_)) {
    ClearCodeCacheEntryIfPresent();
    return absl::nullopt;
  }
  return std::move(code_cache_data_);
}

void ResourceRequestSender::CodeCacheFetcher::DidReceiveCachedCode(
    base::Time response_time,
    mojo_base::BigBuffer data) {
  is_waiting_ = false;
  code_cache_data_ = std::move(data);
  if (did_receive_cached_metadata_from_url_loader_) {
    ClearCodeCacheEntryIfPresent();
    return;
  }
  code_cache_response_time_ = response_time;
  std::move(done_closure_).Run();
}

void ResourceRequestSender::CodeCacheFetcher::ClearCodeCacheEntryIfPresent() {
  if (code_cache_host_ && code_cache_data_ && (code_cache_data_->size() > 0)) {
    (*code_cache_host_)
        ->ClearCodeCacheEntry(code_cache_type_, KURL(initial_url_));
  }
  code_cache_data_.reset();
}

ResourceRequestSender::ResourceRequestSender() = default;

ResourceRequestSender::~ResourceRequestSender() = default;

void ResourceRequestSender::SendSync(
    std::unique_ptr<network::ResourceRequest> request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    SyncLoadResponse* response,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
    base::TimeDelta timeout,
    const Vector<String>& cors_exempt_header_list,
    base::WaitableEvent* terminate_sync_load_event,
    mojo::PendingRemote<mojom::blink::BlobRegistry> download_to_blob_registry,
    scoped_refptr<ResourceRequestClient> client,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  CheckSchemeForReferrerPolicy(*request);

  DCHECK(loader_options & network::mojom::kURLLoadOptionSynchronous);
  DCHECK(request->load_flags & net::LOAD_IGNORE_LIMITS);

  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory =
      url_loader_factory->Clone();
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Prepare the configured throttles for use on a separate thread.
  for (const auto& throttle : throttles) {
    throttle->DetachFromCurrentSequence();
  }

  // A task is posted to a separate thread to execute the request so that
  // this thread may block on a waitable event. It is safe to pass raw
  // pointers to on-stack objects as this stack frame will
  // survive until the request is complete.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({});
  SyncLoadContext* context_for_redirect = nullptr;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      WTF::CrossThreadBindOnce(
          &SyncLoadContext::StartAsyncWithWaitableEvent, std::move(request),
          task_runner, traffic_annotation, loader_options,
          std::move(pending_factory), std::move(throttles),
          CrossThreadUnretained(response),
          CrossThreadUnretained(&context_for_redirect),
          CrossThreadUnretained(&redirect_or_response_event),
          CrossThreadUnretained(terminate_sync_load_event), timeout,
          std::move(download_to_blob_registry), cors_exempt_header_list,
          std::move(resource_load_info_notifier_wrapper)));

  // `redirect_or_response_event` will signal when each redirect completes, and
  // when the final response is complete.
  redirect_or_response_event.Wait();

  while (context_for_redirect) {
    DCHECK(response->redirect_info);

    using RefCountedOptionalStringVector =
        base::RefCountedData<absl::optional<std::vector<std::string>>>;
    const scoped_refptr<RefCountedOptionalStringVector> removed_headers =
        base::MakeRefCounted<RefCountedOptionalStringVector>();
    using RefCountedOptionalHttpRequestHeaders =
        base::RefCountedData<absl::optional<net::HttpRequestHeaders>>;
    const scoped_refptr<RefCountedOptionalHttpRequestHeaders> modified_headers =
        base::MakeRefCounted<RefCountedOptionalHttpRequestHeaders>();
    client->OnReceivedRedirect(
        *response->redirect_info, response->head.Clone(),
        /*follow_redirect_callback=*/
        WTF::BindOnce(
            [](scoped_refptr<RefCountedOptionalStringVector>
                   removed_headers_out,
               scoped_refptr<RefCountedOptionalHttpRequestHeaders>
                   modified_headers_out,
               std::vector<std::string> removed_headers,
               net::HttpRequestHeaders modified_headers) {
              removed_headers_out->data = std::move(removed_headers);
              modified_headers_out->data = std::move(modified_headers);
            },
            removed_headers, modified_headers));
    // `follow_redirect_callback` can't be asynchronously called for synchronous
    // requests because the current thread will be blocked by
    // `redirect_or_response_event.Wait()` call. So we check `HasOneRef()` here
    // to ensure that `follow_redirect_callback` is not kept alive.
    CHECK(removed_headers->HasOneRef());
    redirect_or_response_event.Reset();
    if (removed_headers->data.has_value()) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&SyncLoadContext::FollowRedirect,
                                    base::Unretained(context_for_redirect),
                                    std::move(*removed_headers->data),
                                    std::move(*modified_headers->data)));
    } else {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&SyncLoadContext::CancelRedirect,
                                    base::Unretained(context_for_redirect)));
    }
    redirect_or_response_event.Wait();
  }
}

int ResourceRequestSender::SendAsync(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<base::SequencedTaskRunner> loading_task_runner,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    const Vector<String>& cors_exempt_header_list,
    scoped_refptr<ResourceRequestClient> client,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host,
    base::OnceCallback<void(mojom::blink::RendererEvictionReason)>
        evict_from_bfcache_callback,
    base::RepeatingCallback<void(size_t)>
        did_buffer_load_while_in_bfcache_callback) {
  loading_task_runner_ = loading_task_runner;
  CheckSchemeForReferrerPolicy(*request);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1286053): This used to be a DCHECK asserting "Main frame
  // shouldn't come here", but after removing and re-landing the DCHECK later it
  // started tripping in some teses. Was the DCHECK invalid or is there a bug
  // somewhere?
  if (!(request->is_outermost_main_frame &&
        IsRequestDestinationFrame(request->destination))) {
    if (request->has_user_gesture) {
      resource_load_info_notifier_wrapper
          ->NotifyUpdateUserGestureCarryoverInfo();
    }
  }
#endif
  if (code_cache_host) {
    code_cache_fetcher_ = CodeCacheFetcher::TryCreateAndStart(
        *request, *code_cache_host,
        WTF::BindOnce(&ResourceRequestSender::DidReceiveCachedCode,
                      weak_factory_.GetWeakPtr()));
  }

  // Compute a unique request_id for this renderer process.
  int request_id = GenerateRequestId();
  request_info_ = std::make_unique<PendingRequestInfo>(
      std::move(client), request->destination, KURL(request->url),
      std::move(resource_load_info_notifier_wrapper));

  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceLoadInitiated(
          request_id, request->url, request->method, request->referrer,
          request_info_->request_destination, request->priority);

  auto url_loader_client = std::make_unique<MojoURLLoaderClient>(
      this, loading_task_runner, url_loader_factory->BypassRedirectChecks(),
      request->url, std::move(evict_from_bfcache_callback),
      std::move(did_buffer_load_while_in_bfcache_callback));

  std::vector<std::string> std_cors_exempt_header_list(
      cors_exempt_header_list.size());
  base::ranges::transform(cors_exempt_header_list,
                          std_cors_exempt_header_list.begin(),
                          [](const WebString& h) { return h.Latin1(); });
  std::unique_ptr<ThrottlingURLLoader> url_loader =
      ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(url_loader_factory), throttles.ReleaseVector(), request_id,
          loader_options, request.get(), url_loader_client.get(),
          traffic_annotation, std::move(loading_task_runner),
          absl::make_optional(std_cors_exempt_header_list));

  // The request may be canceled by `ThrottlingURLLoader::CreateAndStart()`, in
  // which case `DeletePendingRequest()` has reset the `request_info_` to
  // nullptr and `this` will be destroyed by `DeleteSoon()`. If so, just return
  // the `request_id`.
  if (!request_info_) {
    return request_id;
  }

  request_info_->url_loader = std::move(url_loader);
  request_info_->url_loader_client = std::move(url_loader_client);

  return request_id;
}

void ResourceRequestSender::Cancel(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Cancel the request if it didn't complete, and clean it up so the bridge
  // will receive no more messages.
  DeletePendingRequest(std::move(task_runner));
}

void ResourceRequestSender::Freeze(LoaderFreezeMode mode) {
  if (!request_info_) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  if (mode != LoaderFreezeMode::kNone) {
    request_info_->ignore_for_histogram = true;
    request_info_->freeze_mode = mode;
    request_info_->url_loader_client->Freeze(mode);
  } else if (request_info_->freeze_mode != LoaderFreezeMode::kNone) {
    request_info_->freeze_mode = LoaderFreezeMode::kNone;
    request_info_->url_loader_client->Freeze(LoaderFreezeMode::kNone);
    loading_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResourceRequestSender::MaybeRunPendingTasks,
                                  weak_factory_.GetWeakPtr()));
    FollowPendingRedirect(request_info_.get());
  }
}

void ResourceRequestSender::DidChangePriority(net::RequestPriority new_priority,
                                              int intra_priority_value) {
  if (!request_info_) {
    DLOG(ERROR) << "unknown request";
    return;
  }

  request_info_->url_loader->SetPriority(new_priority, intra_priority_value);
}

void ResourceRequestSender::DeletePendingRequest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (!request_info_) {
    return;
  }

  if (request_info_->net_error == net::ERR_IO_PENDING) {
    request_info_->net_error = net::ERR_ABORTED;
    request_info_->resource_load_info_notifier_wrapper
        ->NotifyResourceLoadCanceled(request_info_->net_error);
  }

  // Cancel loading.
  request_info_->url_loader.reset();

  // Clear URLLoaderClient to stop receiving further Mojo IPC from the browser
  // process.
  request_info_->url_loader_client.reset();

  // Always delete the `request_info_` asyncly so that cancelling the request
  // doesn't delete the request context which the `request_info_->client` points
  // to while its response is still being handled.
  task_runner->DeleteSoon(FROM_HERE, request_info_.release());
}

ResourceRequestSender::PendingRequestInfo::PendingRequestInfo(
    scoped_refptr<ResourceRequestClient> client,
    network::mojom::RequestDestination request_destination,
    const KURL& request_url,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper)
    : client(std::move(client)),
      request_destination(request_destination),
      url(request_url),
      response_url(request_url),
      local_request_start(base::TimeTicks::Now()),
      resource_load_info_notifier_wrapper(
          std::move(resource_load_info_notifier_wrapper)) {}

ResourceRequestSender::PendingRequestInfo::~PendingRequestInfo() = default;

void ResourceRequestSender::FollowPendingRedirect(
    PendingRequestInfo* request_info) {
  if (request_info->has_pending_redirect) {
    request_info->has_pending_redirect = false;
    // net::URLRequest clears its request_start on redirect, so should we.
    request_info->local_request_start = base::TimeTicks::Now();
    // Redirect URL may not be handled by the network service, so force a
    // restart in case another URLLoaderFactory should handle the URL.
    if (request_info->redirect_requires_loader_restart) {
      request_info->modified_headers.Clear();
      request_info->url_loader->FollowRedirectForcingRestart();
    } else {
      std::vector<std::string> removed_headers(
          request_info_->removed_headers.size());
      base::ranges::transform(request_info_->removed_headers,
                              removed_headers.begin(), &WebString::Ascii);
      request_info->url_loader->FollowRedirect(
          removed_headers, request_info->modified_headers,
          {} /* modified_cors_exempt_headers */);
      request_info->modified_headers.Clear();
    }
  }
}

void ResourceRequestSender::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  if (ShouldDeferTask()) {
    pending_tasks_.emplace_back(
        WTF::BindOnce(&ResourceRequestSender::OnTransferSizeUpdated,
                      weak_factory_.GetWeakPtr(), transfer_size_diff));
    return;
  }

  DCHECK_GT(transfer_size_diff, 0);
  if (!request_info_) {
    return;
  }

  // TODO(yhirano): Consider using int64_t in
  // ResourceRequestClient::OnTransferSizeUpdated.
  request_info_->client->OnTransferSizeUpdated(transfer_size_diff);
  if (!request_info_) {
    return;
  }
  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceTransferSizeUpdated(transfer_size_diff);
}

void ResourceRequestSender::OnUploadProgress(int64_t position, int64_t size) {
  if (ShouldDeferTask()) {
    pending_tasks_.emplace_back(
        WTF::BindOnce(&ResourceRequestSender::OnUploadProgress,
                      weak_factory_.GetWeakPtr(), position, size));
    return;
  }
  if (!request_info_) {
    return;
  }

  request_info_->client->OnUploadProgress(position, size);
}

void ResourceRequestSender::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata,
    base::TimeTicks response_ipc_arrival_time) {
  if (code_cache_fetcher_ && cached_metadata) {
    code_cache_fetcher_->DidReceiveCachedMetadataFromUrlLoader();
    code_cache_fetcher_.reset();
    MaybeRunPendingTasks();
  }

  if (ShouldDeferTask()) {
    pending_tasks_.push_back(WTF::BindOnce(
        &ResourceRequestSender::OnReceivedResponse, weak_factory_.GetWeakPtr(),
        std::move(response_head), std::move(body), std::move(cached_metadata),
        response_ipc_arrival_time));
    return;
  }
  TRACE_EVENT0("loading", "ResourceRequestSender::OnReceivedResponse");
  if (!request_info_) {
    return;
  }
  request_info_->local_response_start = response_ipc_arrival_time;
  request_info_->remote_request_start =
      response_head->load_timing.request_start;
  // Now that response_start has been set, we can properly set the TimeTicks in
  // the URLResponseHead.
  base::TimeTicks remote_response_start =
      ToLocalURLResponseHead(*request_info_, *response_head);
  if (!request_info_->ignore_for_histogram &&
      !remote_response_start.is_null()) {
    base::UmaHistogramTimes("Blink.ResourceRequest.ResponseDelay2",
                            response_ipc_arrival_time - remote_response_start);
  }
  request_info_->load_timing_info = response_head->load_timing;

  if (code_cache_fetcher_) {
    CHECK(!cached_metadata);
    cached_metadata =
        code_cache_fetcher_->TakeCodeCacheForResponse(*response_head);
  }

  request_info_->client->OnReceivedResponse(
      response_head.Clone(), std::move(body), std::move(cached_metadata));
  if (!request_info_) {
    return;
  }

  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceResponseReceived(std::move(response_head));
}

void ResourceRequestSender::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head,
    base::TimeTicks redirect_ipc_arrival_time) {
  if (ShouldDeferTask()) {
    pending_tasks_.emplace_back(WTF::BindOnce(
        &ResourceRequestSender::OnReceivedRedirect, weak_factory_.GetWeakPtr(),
        redirect_info, std::move(response_head), redirect_ipc_arrival_time));
    return;
  }
  TRACE_EVENT0("loading", "ResourceRequestSender::OnReceivedRedirect");
  if (!request_info_) {
    return;
  }
  CHECK(request_info_->url_loader);

  if (code_cache_fetcher_) {
    code_cache_fetcher_->SetCurrentUrl(redirect_info.new_url);
  }

  request_info_->local_response_start = redirect_ipc_arrival_time;
  request_info_->remote_request_start =
      response_head->load_timing.request_start;
  request_info_->redirect_requires_loader_restart =
      RedirectRequiresLoaderRestart(GURL(request_info_->response_url),
                                    redirect_info.new_url);

  base::TimeTicks remote_response_start =
      ToLocalURLResponseHead(*request_info_, *response_head);
  if (!request_info_->ignore_for_histogram &&
      !remote_response_start.is_null()) {
    UmaHistogramTimes(
        "Blink.ResourceRequest.RedirectDelay2",
        request_info_->local_response_start - remote_response_start);
  }

  auto callback = WTF::BindOnce(
      &ResourceRequestSender::OnFollowRedirectCallback,
      weak_factory_.GetWeakPtr(), redirect_info, response_head.Clone());
  request_info_->client->OnReceivedRedirect(
      redirect_info, std::move(response_head), std::move(callback));
}

void ResourceRequestSender::OnFollowRedirectCallback(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head,
    std::vector<std::string> removed_headers,
    net::HttpRequestHeaders modified_headers) {
  // DeletePendingRequest() may have cleared request_info_.
  if (!request_info_) {
    return;
  }
  if (request_info_->net_error != net::ERR_IO_PENDING) {
    // The request has been completed.
    return;
  }

  // TODO(yoav): If request_info doesn't change above, we could avoid this
  // copy.
  WebVector<WebString> vector(removed_headers.size());
  base::ranges::transform(removed_headers, vector.begin(),
                          &WebString::FromASCII);
  request_info_->removed_headers = vector;
  request_info_->response_url = KURL(redirect_info.new_url);
  request_info_->has_pending_redirect = true;
  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceRedirectReceived(redirect_info, std::move(response_head));
  request_info_->modified_headers.MergeFrom(modified_headers);

  if (request_info_->freeze_mode == LoaderFreezeMode::kNone) {
    FollowPendingRedirect(request_info_.get());
  }
}

void ResourceRequestSender::OnRequestComplete(
    const network::URLLoaderCompletionStatus& status,
    base::TimeTicks complete_ipc_arrival_time) {
  if (ShouldDeferTask()) {
    pending_tasks_.emplace_back(WTF::BindOnce(
        &ResourceRequestSender::OnRequestComplete, weak_factory_.GetWeakPtr(),
        status, complete_ipc_arrival_time));
    return;
  }
  TRACE_EVENT0("loading", "ResourceRequestSender::OnRequestComplete");

  if (!request_info_) {
    return;
  }
  request_info_->net_error = status.error_code;

  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceLoadCompleted(status);

  ResourceRequestClient* client = request_info_->client.get();

  network::URLLoaderCompletionStatus renderer_status(status);
  if (status.completion_time.is_null()) {
    // No completion timestamp is provided, leave it as is.
  } else if (request_info_->remote_request_start.is_null() ||
             request_info_->load_timing_info.request_start.is_null()) {
    // We cannot convert the remote time to a local time, let's use the current
    // timestamp. This happens when
    //  - We get an error before OnReceivedRedirect or OnReceivedResponse is
    //    called, or
    //  - Somehow such a timestamp was missing in the LoadTimingInfo.
    renderer_status.completion_time = complete_ipc_arrival_time;
  } else {
    // We have already converted the request start timestamp, let's use that
    // conversion information.
    // Note: We cannot create a InterProcessTimeTicksConverter with
    // (local_request_start, now, remote_request_start, remote_completion_time)
    // as that may result in inconsistent timestamps.
    renderer_status.completion_time =
        std::min(status.completion_time - request_info_->remote_request_start +
                     request_info_->load_timing_info.request_start,
                 complete_ipc_arrival_time);
  }

  if (!request_info_->ignore_for_histogram) {
    const net::LoadTimingInfo& timing_info = request_info_->load_timing_info;
    if (!timing_info.request_start.is_null()) {
      UmaHistogramTimes(
          "Blink.ResourceRequest.StartDelay2",
          timing_info.request_start - request_info_->local_request_start);
    }
    if (!renderer_status.completion_time.is_null()) {
      UmaHistogramTimes(
          "Blink.ResourceRequest.CompletionDelay2",
          complete_ipc_arrival_time - renderer_status.completion_time);
    }
  }
  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  // TODO(kinuko): Revisit here. This probably needs to call
  // request_info_->client but the past attempt to change it seems to have
  // caused crashes. (crbug.com/547047)
  client->OnCompletedRequest(renderer_status);
}

base::TimeTicks ResourceRequestSender::ToLocalURLResponseHead(
    const PendingRequestInfo& request_info,
    network::mojom::URLResponseHead& response_head) const {
  base::TimeTicks remote_response_start = response_head.response_start;
  if (base::TimeTicks::IsConsistentAcrossProcesses() ||
      request_info.local_request_start.is_null() ||
      request_info.local_response_start.is_null() ||
      response_head.request_start.is_null() ||
      remote_response_start.is_null() ||
      response_head.load_timing.request_start.is_null()) {
    return remote_response_start;
  }

#if BUILDFLAG(IS_WIN)
  // This code below can only be reached on Windows as the
  // base::TimeTicks::IsConsistentAcrossProcesses() above always returns true
  // except on Windows platform.
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(request_info.local_request_start),
      LocalTimeTicks::FromTimeTicks(request_info.local_response_start),
      RemoteTimeTicks::FromTimeTicks(response_head.request_start),
      RemoteTimeTicks::FromTimeTicks(remote_response_start));

  net::LoadTimingInfo* load_timing = &response_head.load_timing;
  RemoteToLocalTimeTicks(converter, &load_timing->request_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_end);
  RemoteToLocalTimeTicks(converter,
                         &load_timing->connect_timing.domain_lookup_start);
  RemoteToLocalTimeTicks(converter,
                         &load_timing->connect_timing.domain_lookup_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_end);
  RemoteToLocalTimeTicks(converter, &load_timing->send_start);
  RemoteToLocalTimeTicks(converter, &load_timing->send_end);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_start);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_end);
  RemoteToLocalTimeTicks(converter, &load_timing->push_start);
  RemoteToLocalTimeTicks(converter, &load_timing->push_end);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_start_time);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_ready_time);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_fetch_start);
  RemoteToLocalTimeTicks(converter,
                         &load_timing->service_worker_respond_with_settled);
  RemoteToLocalTimeTicks(converter, &remote_response_start);
#endif
  return remote_response_start;
}

void ResourceRequestSender::DidReceiveCachedCode() {
  MaybeRunPendingTasks();
}

bool ResourceRequestSender::ShouldDeferTask() const {
  return (code_cache_fetcher_ && code_cache_fetcher_->is_waiting()) ||
         !pending_tasks_.empty();
}

void ResourceRequestSender::MaybeRunPendingTasks() {
  if (!request_info_ ||
      (code_cache_fetcher_ && code_cache_fetcher_->is_waiting()) ||
      (request_info_->freeze_mode != LoaderFreezeMode::kNone)) {
    return;
  }

  WTF::Vector<base::OnceClosure> tasks = std::move(pending_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

}  // namespace blink
