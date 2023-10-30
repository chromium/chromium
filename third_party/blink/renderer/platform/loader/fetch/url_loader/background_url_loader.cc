// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_url_loader.h"

#include <atomic>

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/background_resource_fetch_histograms.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_background_resource_fetch_assets.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
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
struct CrossThreadCopier<absl::optional<mojo_base::BigBuffer>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = absl::optional<mojo_base::BigBuffer>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<net::HttpRequestHeaders> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = net::HttpRequestHeaders;
  static Type Copy(Type&& value) { return std::move(value); }
};

}  // namespace WTF

namespace blink {

namespace {

BackgroundResourceFetchSupportStatus CanHandleRequestInternal(
    const ResourceRequestHead& request,
    const ResourceLoaderOptions& options) {
  if (options.synchronous_policy == kRequestSynchronously) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedSyncRequest;
  }
  // Currently, BackgroundURLLoader only supports GET requests.
  if (request.HttpMethod() != http_names::kGET) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedNonGetRequest;
  }

  // Currently, only supports HTTP family because:
  // - PDF plugin is using the mechanism of subresource overrides with
  //   "chrome-extension://" urls. But ChildURLLoaderFactoryBundle::Clone()
  //   can't clone `subresource_overrides_`. So BackgroundURLLoader can't handle
  //   requests from the PDF plugin.
  if (!request.Url().ProtocolIsInHTTPFamily()) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedNonHttpUrlRequest;
  }

  // Don't support keepalive request which must be handled aligning with the
  // page lifecycle states. It is difficult to handle in the background thread.
  if (request.GetKeepalive()) {
    return BackgroundResourceFetchSupportStatus::kUnsupportedKeepAliveRequest;
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
          scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
          scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
          Vector<std::unique_ptr<URLLoaderThrottle>> throttles)
      : background_resource_fetch_context_(
            std::move(background_resource_fetch_context)),
        cors_exempt_header_list_(cors_exempt_header_list),
        freezable_task_runner_(std::move(freezable_task_runner)),
        unfreezable_task_runner_(std::move(unfreezable_task_runner)),
        background_task_runner_(
            background_resource_fetch_context_->GetTaskRunner()),
        throttles_(std::move(throttles)) {
    DETACH_FROM_SEQUENCE(background_sequence_checker_);
  }

  ~Context() = default;

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
      PostCrossThreadTask(*freezable_task_runner_, FROM_HERE,
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

    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.reserve(throttles_.size());
    for (auto& throttle : throttles_) {
      throttle->DetachFromCurrentSequence();
      throttles.push_back(std::move(throttle));
    }

    PostCrossThreadTask(
        *background_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &Context::StartOnBackground, scoped_refptr(this),
            std::move(background_resource_fetch_context_), std::move(request),
            top_frame_origin ? top_frame_origin->ToUrlOrigin() : url::Origin(),
            no_mime_sniffing, cors_exempt_header_list_, std::move(throttles),
            std::move(resource_load_info_notifier_wrapper),
            should_use_code_cache_host));
  }

 private:
  class RequestClient : public ResourceRequestClient {
   public:
    explicit RequestClient(scoped_refptr<Context> context)
        : context_(std::move(context)) {}
    ~RequestClient() override = default;

    // ResourceRequestClient overrides:
    void OnUploadProgress(uint64_t position, uint64_t size) override {
      // We don't support sending body.
      NOTREACHED_NORETURN();
    }
    void OnReceivedRedirect(
        const net::RedirectInfo& redirect_info,
        network::mojom::URLResponseHeadPtr head,
        FollowRedirectCallback follow_redirect_callback) override {
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::OnReceivedRedirect, context_, redirect_info,
          std::move(head), std::move(follow_redirect_callback)));
    }
    void OnReceivedResponse(
        network::mojom::URLResponseHeadPtr head,
        mojo::ScopedDataPipeConsumerHandle body,
        absl::optional<mojo_base::BigBuffer> cached_metadata,
        base::TimeTicks response_arrival_at_renderer) override {
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::OnReceivedResponse, context_, std::move(head),
          std::move(body), std::move(cached_metadata),
          response_arrival_at_renderer));
    }
    void OnTransferSizeUpdated(int transfer_size_diff) override {
      context_->PostTaskToMainThread(CrossThreadBindOnce(
          &Context::OnTransferSizeUpdated, context_, transfer_size_diff));
    }
    void OnCompletedRequest(
        const network::URLLoaderCompletionStatus& status) override {
      context_->PostTaskToMainThread(
          CrossThreadBindOnce(&Context::OnCompletedRequest, context_, status));
    }

   private:
    scoped_refptr<Context> context_;
  };

  void StartOnBackground(
      scoped_refptr<WebBackgroundResourceFetchAssets>
          background_resource_fetch_context,
      std::unique_ptr<network::ResourceRequest> request,
      const url::Origin& top_frame_origin,
      bool no_mime_sniffing,
      const Vector<String>& cors_exempt_header_list,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      bool should_use_code_cache_host) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    if (canceled_) {
      // This happens when the request was canceled (eg: window.stop())
      // quickly after starting the request.
      return;
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

    // TODDO(crbug.com/1379780): Fetch code cache from the background
    // thread when `should_use_code_cache_host` is set.
    request_id_ = resource_request_sender_->SendAsync(
        std::move(request), background_task_runner_, tag, loader_options,
        cors_exempt_header_list, base::MakeRefCounted<RequestClient>(this),
        background_resource_fetch_context->GetLoaderFactory(),
        std::move(throttles), std::move(resource_load_info_notifier_wrapper),
        /*code_cache_host=*/nullptr,
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

  void PostTaskToMainThread(CrossThreadOnceFunction<void(int)> task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    {
      base::AutoLock locker(tasks_lock_);
      tasks_.push_back(CrossThreadBindOnce(std::move(task), request_id_));
    }
    PostCrossThreadTask(*freezable_task_runner_, FROM_HERE,
                        CrossThreadBindOnce(&Context::RunTasksOnMainThread,
                                            scoped_refptr(this)));
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
      base::OnceCallback<void(std::vector<std::string> removed_headers,
                              net::HttpRequestHeaders modified_headers)>
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
          CrossThreadBindOnce(std::move(follow_redirect_callback),
                              std::move(removed_headers),
                              std::move(modified_headers)));
    } else {
      // `follow_redirect_callback` must be deleted in the background thread.
      background_task_runner_->DeleteSoon(
          FROM_HERE, std::make_unique<base::OnceCallback<void(
                         std::vector<std::string>, net::HttpRequestHeaders)>>(
                         std::move(follow_redirect_callback)));
    }
  }
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                          mojo::ScopedDataPipeConsumerHandle body,
                          absl::optional<mojo_base::BigBuffer> cached_metadata,
                          base::TimeTicks response_arrival_at_renderer,
                          int request_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    WebURLResponse response = WebURLResponse::Create(
        url_, *head, has_devtools_request_id_, request_id);
    response.SetArrivalTimeAtRenderer(response_arrival_at_renderer);
    client_->DidReceiveResponse(response, std::move(body),
                                std::move(cached_metadata));
  }
  void OnTransferSizeUpdated(int transfer_size_diff, int request_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    client_->DidReceiveTransferSizeUpdate(transfer_size_diff);
  }
  void OnCompletedRequest(const network::URLLoaderCompletionStatus& status,
                          int request_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
    int64_t total_transfer_size = status.encoded_data_length;
    int64_t encoded_body_size = status.encoded_body_length;
    if (status.error_code != net::OK) {
      client_->DidFail(WebURLError::Create(status, url_),
                       status.completion_time, total_transfer_size,
                       encoded_body_size, status.decoded_body_length);
    } else {
      client_->DidFinishLoading(status.completion_time, total_transfer_size,
                                encoded_body_size, status.decoded_body_length,
                                status.should_report_corb_blocking);
    }
  }

  void EvictFromBackForwardCacheOnBackground(
      mojom::blink::RendererEvictionReason reason) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    // TODDO(crbug.com/1379780): Implement this.
  }
  void DidBufferLoadWhileInBackForwardCacheOnBackground(size_t num_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
    // TODDO(crbug.com/1379780): Implement this.
  }

  scoped_refptr<WebBackgroundResourceFetchAssets>
      background_resource_fetch_context_
          GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  const Vector<String> cors_exempt_header_list_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  const scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  Vector<std::unique_ptr<URLLoaderThrottle>> throttles_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  Deque<CrossThreadOnceFunction<void(void)>> tasks_ GUARDED_BY(tasks_lock_);
  base::Lock tasks_lock_;

  URLLoaderClient* client_ GUARDED_BY_CONTEXT(main_thread_sequence_checker_) =
      nullptr;
  KURL url_ GUARDED_BY_CONTEXT(main_thread_sequence_checker_);
  bool has_devtools_request_id_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_) = false;
  LoaderFreezeMode freeze_mode_ GUARDED_BY_CONTEXT(
      main_thread_sequence_checker_) = LoaderFreezeMode::kNone;

  std::unique_ptr<ResourceRequestSender> resource_request_sender_
      GUARDED_BY_CONTEXT(background_sequence_checker_);
  int request_id_ GUARDED_BY_CONTEXT(background_sequence_checker_) = -1;

  std::atomic<bool> canceled_ = false;
  SEQUENCE_CHECKER(main_thread_sequence_checker_);
  SEQUENCE_CHECKER(background_sequence_checker_);
};

// static
bool BackgroundURLLoader::CanHandleRequest(
    const ResourceRequestHead& request,
    const ResourceLoaderOptions& options) {
  CHECK(IsMainThread());
  auto result = CanHandleRequestInternal(request, options);
  base::UmaHistogramEnumeration(
      kBackgroundResourceFetchSupportStatusHistogramName, result);
  return result == BackgroundResourceFetchSupportStatus::kSupported;
}

BackgroundURLLoader::BackgroundURLLoader(
    scoped_refptr<WebBackgroundResourceFetchAssets>
        background_resource_fetch_context,
    const Vector<String>& cors_exempt_header_list,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    Vector<std::unique_ptr<URLLoaderThrottle>> throttles)
    : context_(base::MakeRefCounted<Context>(
          std::move(background_resource_fetch_context),
          cors_exempt_header_list,
          std::move(freezable_task_runner),
          std::move(unfreezable_task_runner),
          std::move(throttles))) {
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
    absl::optional<WebURLError>& error,
    scoped_refptr<SharedBuffer>& data,
    int64_t& encoded_data_length,
    uint64_t& encoded_body_length,
    scoped_refptr<BlobDataHandle>& downloaded_blob,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  // BackgroundURLLoader doesn't support sync requests.
  NOTREACHED_NORETURN();
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

}  // namespace blink
