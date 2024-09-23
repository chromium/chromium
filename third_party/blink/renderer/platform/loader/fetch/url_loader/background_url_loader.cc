// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_url_loader.h"

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/loader/background_resource_fetch_histograms.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_background_resource_fetch_assets.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/back_forward_cache_buffer_limit_tracker.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/background_code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_response_processor.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace {

using FollowRedirectCallback =
    base::OnceCallback<void(std::vector<std::string> removed_headers,
                            net::HttpRequestHeaders modified_headers)>;

using BodyVariant = blink::BackgroundResponseProcessor::BodyVariant;

}  // namespace

namespace WTF {

template <>
struct CrossThreadCopier<FollowRedirectCallback> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = FollowRedirectCallback;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<url::Origin> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = url::Origin;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<network::mojom::URLResponseHeadPtr> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = network::mojom::URLResponseHeadPtr;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<network::URLLoaderCompletionStatus>
    : public CrossThreadCopierByValuePassThrough<
          network::URLLoaderCompletionStatus> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<net::RedirectInfo>
    : public CrossThreadCopierByValuePassThrough<net::RedirectInfo> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<std::vector<std::string>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::vector<std::string>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::vector<std::unique_ptr<blink::URLLoaderThrottle>>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<std::optional<mojo_base::BigBuffer>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::optional<mojo_base::BigBuffer>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<net::HttpRequestHeaders> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = net::HttpRequestHeaders;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<BodyVariant> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = BodyVariant;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<std::optional<network::URLLoaderCompletionStatus>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::optional<network::URLLoaderCompletionStatus>;
  static Type Copy(Type&& value) { return std::move(value); }
};

}  // namespace WTF

namespace blink {

namespace {

BackgroundResourceFetchSupportStatus CanHandleRequestInternal(
    const network::ResourceRequest& request,
    const ResourceLoaderOptions& options,
    bool is_prefech_only_document) {
  if (options.synchronous_policy == kRequestSynchronously) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedSyncRequest;
  }
  // Currently, BackgroundURLLoader only supports GET requests.
  if (request.method != net::HttpRequestHeaders::kGetMethod) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedNonGetRequest;
  }

  // Currently, only supports HTTP family and blob URL because:
  // - PDF plugin is using the mechanism of subresource overrides with
  //   "chrome-extension://" urls. But ChildURLLoaderFactoryBundle::Clone()
  //   can't clone `subresource_overrides_`. So BackgroundURLLoader can't handle
  //   requests from the PDF plugin.
  if (!request.url.SchemeIsHTTPOrHTTPS() && !request.url.SchemeIsBlob()) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedNonHttpUrlRequest;
  }

  // Don't support keepalive request which must be handled aligning with the
  // page lifecycle states. It is difficult to handle in the background thread.
  if (request.keepalive) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedKeepAliveRequest;
  }

  // Currently prerender::NoStatePrefetchHelper doesn't work on the background
  // thread.
  if (is_prefech_only_document) {
    return BackgroundResourceFetchSupportStatus::
        kUnsupportedPrefetchOnlyDocument;
  }

  // TODO(crbug.com/1379780): Determine the range of supported requests.
  return BackgroundResourceFetchSupportStatus::kSupported;
}

}  // namespace

class BackgroundURLLoader::Context
    : public WTF::ThreadSafeRefCounted<BackgroundURLLoader::Context> {
 public:
  Context(scoped_refptr<WebBackgroundResourceFetchAssets>
              background_resource_fetch_context,
          const Vector<String>& cors_exempt_header_list,
          scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
          BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
          scoped_refptr<BackgroundCodeCacheHost> background_code_cache_host)
      : background_resource_fetch_context_(
            std::move(background_resource_fetch_context)),
        cors_exempt_header_list_(cors_exempt_header_list),
        unfreezable_task_runner_(std::move(unfreezable_task_runner)),
        background_task_runner_(
            background_resource_fetch_context_->GetTaskRunner()),
        back_forward_cache_loader_helper_(
            std::make_unique<WeakPersistent<BackForwardCacheLoaderHelper>>(
                back_forward_cache_loader_helper)),
        background_code_cache_host_(std::move(background_code_cache_host)) {
    DETACH_FROM_SEQUENCE(background_sequence_checker_);
  }

  ~Context() {
    // WeakPersistent must be destructed in the original thread.
    unfreezable_task_runner_->DeleteSoon(
        FROM_HERE, std::move(back_forward_cache_loader_helper_));
  }
  scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    return unfreezable_task_runner_;
  }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    canceled_ = true;
    client_ = nullptr;
    PostCrossThreadTask(
        *background_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&Context::CancelOnBackground, scoped_refptr(this)));
    {
      base::AutoLock locker(tasks_lock_);
      tasks_.clear();
    }
  }

  void Freeze(LoaderFreezeMode mode) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    if (freeze_mode_ == mode) {
      return;
    }
    freeze_mode_ = mode;
    PostCrossThreadTask(*background_task_runner_, FROM_HERE,
                        CrossThreadBindOnce(&Context::FreezeOnBackground,
                                            scoped_refptr(this), mode));

    if (freeze_mode_ == LoaderFreezeMode::kNone) {
      PostCrossThreadTask(*unfreezable_task_runner_, FROM_HERE,
                          CrossThreadBindOnce(&Context::RunTasksOnMainThread,
                                              scoped_refptr(this)));
    }
  }

  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    PostCrossThreadTask(
        *background_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&Context::DidChangePriorityOnBackground,
                            scoped_refptr(this), new_priority,
                            intra_priority_value));
  }

  void SetBackgroundResponseProcessorFactory(
      std::unique_ptr<BackgroundResponseProcessorFactory>
          background_response_processor_factory) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    background_response_processor_factory_ =
        std::move(background_response_processor_factory);
  }

  void Start(std::unique_ptr<network::ResourceRequest> request,
             scoped_refptr<const SecurityOrigin> top_frame_origin,
             bool no_mime_sniffing,
             std::unique_ptr<ResourceLoadInfoNotifierWrapper>
                 resource_load_info_notifier_wrapper,
             bool should_use_code_cache_host,
             URLLoaderClient* client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    url_ = KURL(request->url);
    has_devtools_request_id_ = request->devtools_request_id.has_value();
    client_ = client;

    PostCrossThreadTask(
        *background_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &Context::StartOnBackground, scoped_refptr(this),
            std::move(background_resource_fetch_context_), std::move(request),
            top_frame_origin ? top_frame_origin->ToUrlOrigin() : url::Origin(),
            no_mime_sniffing, cors_exempt_header_list_,
            std::move(resource_load_info_notifier_wrapper),
            should_use_code_cache_host,
            std::move(background_response_processor_factory_)));
  }

 private:
  class RequestClient : public ResourceRequestClient,
                        public BackgroundResponseProcessor::Client {
   public:
    explicit RequestClient(
        scoped_refptr<Context> context,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner,
        std::unique_ptr<BackgroundResponseProcessor>
            background_response_processor)
        : context_(std::move(context)),
          background_task_runner_(std::move(background_task_runner)),
          background_response_processor_(
              std::move(background_response_processor)) {
      CHECK(background_task_runner_->RunsTasksInCurrentSequence());
    }
    ~RequestClient() override = default;

    // ResourceRequestClient overrides:
    void OnUploadProgress(uint64_t position, uint64_t size) override {
      // We don't support sending body.
      NOTREACHED();
    }
    void OnReceivedRedirect(
        const net::RedirectInfo& redirect_info,
        network::mojom::URLResponseHeadPtr head,
        FollowRedirectCallback follow_redirect_callback) override {
      CHECK(background_task_runner_->RunsTasksInCurrentSequence());
      // Wrapping `follow_redirect_callback` with base::OnTaskRunnerDeleter to
      // make sure that `follow_redirect_callback` will be destructed in the
      // background thread when `client_->WillFollowRedirect()` returns false
      // in Context::OnReceivedRedirect() or the request is canceled before
      // Context::OnReceivedRedirect() is called in the main thread.
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::OnReceivedRedirect, context_, redirect_info,
          std::move(head),
          std::unique_ptr<FollowRedirectCallback, base::OnTaskRunnerDeleter>(
              new FollowRedirectCallback(std::move(follow_redirect_callback)),
              base::OnTaskRunnerDeleter(context_->background_task_runner_))));
    }
    void OnReceivedResponse(
        network::mojom::URLResponseHeadPtr head,
        mojo::ScopedDataPipeConsumerHandle body,
        std::optional<mojo_base::BigBuffer> cached_metadata) override {
      CHECK(background_task_runner_->RunsTasksInCurrentSequence());
      if (background_response_processor_) {
        if (background_response_processor_->MaybeStartProcessingResponse(
                head, body, cached_metadata, background_task_runner_, this)) {
          waiting_for_background_response_processor_ = true;
          return;
        }
        background_response_processor_.reset();
      }
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::OnReceivedResponse, context_, std::move(head),
          std::move(body), std::move(cached_metadata)));
    }
    void OnTransferSizeUpdated(int transfer_size_diff) override {
      CHECK(background_task_runner_->RunsTasksInCurrentSequence());
      if (waiting_for_background_response_processor_) {
        deferred_transfer_size_diff_ =
            base::CheckAdd(deferred_transfer_size_diff_, transfer_size_diff)
                .ValueOrDie();
        return;
      }
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::OnTransferSizeUpdated, context_, transfer_size_diff));
    }
    void OnCompletedRequest(
        const network::URLLoaderCompletionStatus& status) override {
      CHECK(background_task_runner_->RunsTasksInCurrentSequence());
      if (waiting_for_background_response_processor_) {
        deferred_status_ = status;
        return;
      }
      context_->PostTaskToMainThread(
          CrossThreadBindOnce(&Context::OnCompletedRequest, context_, status));
    }

    // BackgroundResponseProcessor::Client overrides:
    void DidFinishBackgroundResponseProcessor(
        network::mojom::URLResponseHeadPtr head,
        BodyVariant body,
        std::optional<mojo_base::BigBuffer> cached_metadata) override {
      CHECK(background_task_runner_->RunsTasksInCurrentSequence());
      background_response_processor_.reset();
      waiting_for_background_response_processor_ = false;
      if (absl::holds_alternative<SegmentedBuffer>(body)) {
        context_->DidReadDataByBackgroundResponseProcessorOnBackground(
            absl::get<SegmentedBuffer>(body).size());
      }
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::DidFinishBackgroundResponseProcessor, context_,
          std::move(head), std::move(body), std::move(cached_metadata),
          deferred_transfer_size_diff_, std::move(deferred_status_)));
    }
    void PostTaskToMainThread(CrossThreadOnceClosure task) override {
      context_->PostTaskToMainThread(std::move(task));
    }

   private:
    scoped_refptr<Context> context_;
    const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
    std::unique_ptr<BackgroundResponseProcessor> background_response_processor_;

    int deferred_transfer_size_diff_ = 0;
    std::optional<network::URLLoaderCompletionStatus> deferred_status_;
    bool waiting_for_background_response_processor_ = false;
    base::WeakPtrFactory<RequestClient> weak_factory_{this};
  };

  void StartOnBackground(scoped_refptr<WebBackgroundResourceFetchAssets>
                             background_resource_fetch_context,
                         std::unique_ptr<network::ResourceRequest> request,
                         const url::Origin& top_frame_origin,
                         bool no_mime_sniffing,
                         const Vector<String>& cors_exempt_header_list,
                         std::unique_ptr<ResourceLoadInfoNotifierWrapper>
                             resource_load_info_notifier_wrapper,
                         bool should_use_code_cache_host,
                         std::unique_ptr<BackgroundResponseProcessorFactory>
                             background_response_processor_factory) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    if (canceled_) {
      // This happens when the request was canceled (eg: window.stop())
      // quickly after starting the request.
      return;
    }

    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    URLLoaderThrottleProvider* throttle_provider =
        background_resource_fetch_context->GetThrottleProvider();
    if (throttle_provider) {
      WebVector<std::unique_ptr<blink::URLLoaderThrottle>> web_throttles =
          throttle_provider->CreateThrottles(
              background_resource_fetch_context->GetLocalFrameToken(),
              *request);
      throttles.reserve(base::checked_cast<wtf_size_t>(web_throttles.size()));
      for (auto& throttle : web_throttles) {
        throttles.push_back(std::move(throttle));
      }
    }

    resource_request_sender_ = std::make_unique<ResourceRequestSender>();
    net::NetworkTrafficAnnotationTag tag =
        FetchUtils::GetTrafficAnnotationTag(*request);
    Platform::Current()->AppendVariationsThrottles(top_frame_origin,
                                                   &throttles);

    uint32_t loader_options = network::mojom::kURLLoadOptionNone;
    if (!no_mime_sniffing) {
      loader_options |= network::mojom::kURLLoadOptionSniffMimeType;
      throttles.push_back(
          std::make_unique<MimeSniffingThrottle>(background_task_runner_));
    }
    request_id_ = resource_request_sender_->SendAsync(
        std::move(request), background_task_runner_, tag, loader_options,
        cors_exempt_header_list,
        base::MakeRefCounted<RequestClient>(
            this, background_task_runner_,
            background_response_processor_factory
                ? std::move(*background_response_processor_factory).Create()
                : nullptr),
        background_resource_fetch_context->GetLoaderFactory(),
        std::move(throttles), std::move(resource_load_info_notifier_wrapper),
        should_use_code_cache_host && background_code_cache_host_
            ? &background_code_cache_host_->GetCodeCacheHost(
                  background_task_runner_)
            : nullptr,
        base::BindOnce(&Context::EvictFromBackForwardCacheOnBackground, this),
        base::BindRepeating(
            &Context::DidBufferLoadWhileInBackForwardCacheOnBackground, this));
  }

  void CancelOnBackground() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    if (request_id_ != -1) {
      resource_request_sender_->Cancel(background_task_runner_);
      resource_request_sender_.reset();
      request_id_ = -1;
    }
  }

  void FreezeOnBackground(LoaderFreezeMode mode) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    if (request_id_ != -1) {
      resource_request_sender_->Freeze(mode);
    }
  }

  void DidChangePriorityOnBackground(WebURLRequest::Priority new_priority,
                                     int intra_priority_value) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    if (request_id_ != -1) {
      net::RequestPriority net_priority =
          WebURLRequest::ConvertToNetPriority(new_priority);
      resource_request_sender_->DidChangePriority(net_priority,
                                                  intra_priority_value);
    }
  }

  void PostTaskToMainThread(CrossThreadOnceClosure task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    {
      base::AutoLock locker(tasks_lock_);
      tasks_.push_back(std::move(task));
    }
    PostCrossThreadTask(*unfreezable_task_runner_, FROM_HERE,
                        CrossThreadBindOnce(&Context::RunTasksOnMainThread,
                                            scoped_refptr(this)));
  }

  void PostTaskToMainThread(CrossThreadOnceFunction<void(int)> task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    PostTaskToMainThread(CrossThreadBindOnce(std::move(task), request_id_));
  }

  void RunTasksOnMainThread() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    if (!client_) {
      // The request was canceled.
      base::AutoLock locker(tasks_lock_);
      tasks_.clear();
      return;
    }

    while (freeze_mode_ == LoaderFreezeMode::kNone) {
      CrossThreadOnceFunction<void(void)> task;
      {
        base::AutoLock locker(tasks_lock_);
        if (tasks_.empty()) {
          return;
        }
        if (!client_) {
          tasks_.clear();
          return;
        }
        task = tasks_.TakeFirst();
      }
      std::move(task).Run();
    }
  }

  void OnReceivedRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr head,
      std::unique_ptr<FollowRedirectCallback, base::OnTaskRunnerDeleter>
          follow_redirect_callback,
      int request_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    WebURLResponse response = WebURLResponse::Create(
        url_, *head, has_devtools_request_id_, request_id);
    url_ = KURL(redirect_info.new_url);
    std::vector<std::string> removed_headers;
    net::HttpRequestHeaders modified_headers;
    if (client_->WillFollowRedirect(
            url_, redirect_info.new_site_for_cookies,
            WebString::FromUTF8(redirect_info.new_referrer),
            ReferrerUtils::NetToMojoReferrerPolicy(
                redirect_info.new_referrer_policy),
            WebString::FromUTF8(redirect_info.new_method), response,
            has_devtools_request_id_, &removed_headers, modified_headers,
            redirect_info.insecure_scheme_was_upgraded)) {
      PostCrossThreadTask(
          *background_task_runner_, FROM_HERE,
          CrossThreadBindOnce(std::move(*follow_redirect_callback),
                              std::move(removed_headers),
                              std::move(modified_headers)));
    }
  }
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                          BodyVariant body,
                          std::optional<mojo_base::BigBuffer> cached_metadata,
                          int request_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    WebURLResponse response = WebURLResponse::Create(
        url_, *head, has_devtools_request_id_, request_id);
    client_->DidReceiveResponse(response, std::move(body),
                                std::move(cached_metadata));
  }
  void DidFinishBackgroundResponseProcessor(
      network::mojom::URLResponseHeadPtr head,
      BodyVariant body,
      std::optional<mojo_base::BigBuffer> cached_metadata,
      int deferred_transfer_size_diff,
      std::optional<network::URLLoaderCompletionStatus> deferred_status,
      int request_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

    OnReceivedResponse(std::move(head), std::move(body),
                       std::move(cached_metadata), request_id);
    if (client_ && deferred_transfer_size_diff > 0) {
      OnTransferSizeUpdated(deferred_transfer_size_diff);
    }
    if (client_ && deferred_status) {
      OnCompletedRequest(*deferred_status);
    }
  }
  void OnTransferSizeUpdated(int transfer_size_diff) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    client_->DidReceiveTransferSizeUpdate(transfer_size_diff);
  }
  void OnCompletedRequest(const network::URLLoaderCompletionStatus& status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    int64_t total_transfer_size = status.encoded_data_length;
    int64_t encoded_body_size = status.encoded_body_length;
    if (status.error_code != net::OK) {
      client_->DidFail(WebURLError::Create(status, url_),
                       status.completion_time, total_transfer_size,
                       encoded_body_size, status.decoded_body_length);
    } else {
      client_->DidFinishLoading(status.completion_time, total_transfer_size,
                                encoded_body_size, status.decoded_body_length);
    }
  }

  void EvictFromBackForwardCacheOnBackground(
      mojom::blink::RendererEvictionReason reason) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    PostCrossThreadTask(*unfreezable_task_runner_, FROM_HERE,
                        CrossThreadBindOnce(&Context::EvictFromBackForwardCache,
                                            scoped_refptr(this), reason));
  }
  void EvictFromBackForwardCache(mojom::blink::RendererEvictionReason reason) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    if (back_forward_cache_loader_helper_ &&
        *back_forward_cache_loader_helper_) {
      (*back_forward_cache_loader_helper_)->EvictFromBackForwardCache(reason);
    }
  }
  void DidBufferLoadWhileInBackForwardCacheOnBackground(size_t num_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    // Need to update the process wide count in the background thread.
    BackForwardCacheBufferLimitTracker::Get().DidBufferBytes(num_bytes);
    PostCrossThreadTask(
        *unfreezable_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&Context::DidBufferLoadWhileInBackForwardCache,
                            scoped_refptr(this), num_bytes));
  }
  void DidBufferLoadWhileInBackForwardCache(size_t num_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    if (freeze_mode_ != LoaderFreezeMode::kBufferIncoming) {
      // This happens when the page was restored from BFCache, and
      // Context::Freeze(LoaderFreezeMode::kNone) was called in the main thread,
      // but Context::FreezeOnBackground(LoaderFreezeMode::kNone) was not called
      // in the background thread when MojoURLLoaderClient::BodyBuffer received
      // the data. In that case, we need to decrease the process-wide total
      // byte count tracked by BackForwardCacheBufferLimitTracker because we
      // have updated it in DidBufferLoadWhileInBackForwardCacheOnBackground().
      BackForwardCacheBufferLimitTracker::Get()
          .DidRemoveFrameOrWorkerFromBackForwardCache(num_bytes);
      return;
    }
    if (back_forward_cache_loader_helper_ &&
        *back_forward_cache_loader_helper_) {
      // We updated the process wide count in the background thread, so setting
      // `update_process_wide_count` to false.
      (*back_forward_cache_loader_helper_)
          ->DidBufferLoadWhileInBackForwardCache(
              /*update_process_wide_count=*/false, num_bytes);
    }
  }

  void DidReadDataByBackgroundResponseProcessorOnBackground(
      size_t total_read_size) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    PostCrossThreadTask(
        *unfreezable_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&Context::DidReadDataByBackgroundResponseProcessor,
                            scoped_refptr(this), total_read_size));
  }

  void DidReadDataByBackgroundResponseProcessor(size_t total_read_size) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    if (freeze_mode_ != LoaderFreezeMode::kBufferIncoming ||
        !back_forward_cache_loader_helper_ ||
        !*back_forward_cache_loader_helper_) {
      return;
    }
    (*back_forward_cache_loader_helper_)
        ->DidBufferLoadWhileInBackForwardCache(
            /*update_process_wide_count=*/true, total_read_size);
    if (!BackForwardCacheBufferLimitTracker::Get()
             .IsUnderPerProcessBufferLimit()) {
      (*back_forward_cache_loader_helper_)
          ->EvictFromBackForwardCache(
              mojom::blink::RendererEvictionReason::kNetworkExceedsBufferLimit);
    }
  }

  scoped_refptr<WebBackgroundResourceFetchAssets>
      background_resource_fetch_context_
          GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  const Vector<String> cors_exempt_header_list_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  const scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  std::unique_ptr<WeakPersistent<BackForwardCacheLoaderHelper>>
      back_forward_cache_loader_helper_
          GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  scoped_refptr<BackgroundCodeCacheHost> background_code_cache_host_
      GUARDED_BY_CONTEXT(background_sequence_checker_);

  Deque<CrossThreadOnceFunction<void(void)>> tasks_ GUARDED_BY(tasks_lock_);
  base::Lock tasks_lock_;

  raw_ptr<URLLoaderClient> client_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_) = nullptr;
  KURL url_ GUARDED_BY_CONTEXT(main_thread_sequence_checker_);
  bool has_devtools_request_id_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_) = false;
  LoaderFreezeMode freeze_mode_ GUARDED_BY_CONTEXT(
      main_thread_sequence_checker_) = LoaderFreezeMode::kNone;

  std::unique_ptr<BackgroundResponseProcessorFactory>
      background_response_processor_factory_
          GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  std::unique_ptr<ResourceRequestSender> resource_request_sender_
      GUARDED_BY_CONTEXT(background_sequence_checker_);
  int request_id_ GUARDED_BY_CONTEXT(background_sequence_checker_) = -1;

  std::atomic<bool> canceled_ = false;
  SEQUENCE_CHECKER(main_thread_sequence_checker_);
  SEQUENCE_CHECKER(background_sequence_checker_);
};

// static
bool BackgroundURLLoader::CanHandleRequest(
    const network::ResourceRequest& request,
    const ResourceLoaderOptions& options,
    bool is_prefech_only_document) {
  CHECK(IsMainThread());
  auto result =
      CanHandleRequestInternal(request, options, is_prefech_only_document);
  base::UmaHistogramEnumeration(
      kBackgroundResourceFetchSupportStatusHistogramName, result);
  return result == BackgroundResourceFetchSupportStatus::kSupported;
}

BackgroundURLLoader::BackgroundURLLoader(
    scoped_refptr<WebBackgroundResourceFetchAssets>
        background_resource_fetch_context,
    const Vector<String>& cors_exempt_header_list,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
    scoped_refptr<BackgroundCodeCacheHost> background_code_cache_host)
    : context_(base::MakeRefCounted<Context>(
          std::move(background_resource_fetch_context),
          cors_exempt_header_list,
          std::move(unfreezable_task_runner),
          back_forward_cache_loader_helper,
          std::move(background_code_cache_host))) {
  CHECK(IsMainThread());
}

BackgroundURLLoader::~BackgroundURLLoader() {
  CHECK(IsMainThread());
  context_->Cancel();
}

void BackgroundURLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool download_to_blob,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    URLLoaderClient* client,
    WebURLResponse& response,
    std::optional<WebURLError>& error,
    scoped_refptr<SharedBuffer>& data,
    int64_t& encoded_data_length,
    uint64_t& encoded_body_length,
    scoped_refptr<BlobDataHandle>& downloaded_blob,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  // BackgroundURLLoader doesn't support sync requests.
  NOTREACHED();
}

void BackgroundURLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool no_mime_sniffing,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host,
    URLLoaderClient* client) {
  bool should_use_code_cache_host = !!code_cache_host;
  context_->Start(std::move(request), std::move(top_frame_origin),
                  no_mime_sniffing,
                  std::move(resource_load_info_notifier_wrapper),
                  should_use_code_cache_host, client);
}

void BackgroundURLLoader::Freeze(LoaderFreezeMode mode) {
  context_->Freeze(mode);
}

void BackgroundURLLoader::DidChangePriority(
    WebURLRequest::Priority new_priority,
    int intra_priority_value) {
  context_->DidChangePriority(new_priority, intra_priority_value);
}

scoped_refptr<base::SingleThreadTaskRunner>
BackgroundURLLoader::GetTaskRunnerForBodyLoader() {
  return context_->unfreezable_task_runner();
}

void BackgroundURLLoader::SetBackgroundResponseProcessorFactory(
    std::unique_ptr<BackgroundResponseProcessorFactory>
        background_response_processor_factory) {
  context_->SetBackgroundResponseProcessorFactory(
      std::move(background_response_processor_factory));
}

}  // namespace blink
