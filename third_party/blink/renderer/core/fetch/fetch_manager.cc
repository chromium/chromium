// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_later_result.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/place_holder_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

using network::mojom::CredentialsMode;
using network::mojom::FetchResponseType;
using network::mojom::RedirectMode;
using network::mojom::RequestMode;

namespace blink {

namespace {

// 64 kilobytes.
constexpr uint64_t kMaxScheduledDeferredBytesPerOrigin = 64 * 1024;

constexpr ResourceType kFetchLaterResourceType = ResourceType::kRaw;
constexpr TextResourceDecoderOptions::ContentType kFetchLaterContentType =
    TextResourceDecoderOptions::kPlainTextContent;

constexpr net::NetworkTrafficAnnotationTag kFetchLaterTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("blink_fetch_later_manager",
                                        R"(
    semantics {
      sender: "Blink Fetch Later Manager"
      description:
        "This request is a website-initiated FetchLater request."
      trigger:
        "On document unloaded or after developer specified timeout has passed."
      data: "Anything the initiator wants to send."
      user_data {
        type: ARBITRARY_DATA
      }
      destination: OTHER
      internal {
        contacts {
          email: "pending-beacon-experiment@chromium.org"
        }
      }
      last_reviewed: "2023-10-25"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting: "These requests cannot be fully disabled in settings. "
        "Only for the requests intended to send after document in BFCache, "
        "they can be disabled via the `Background Sync` section under the "
        "`Privacy and security` tab in settings. "
        "This feature is not yet enabled."
      policy_exception_justification: "The policy for Background sync is not "
      "yet implemented."
    })");

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must remain in sync with FetchLaterRendererMetricType in
// tools/metrics/histograms/enums.xml.
enum class FetchLaterRendererMetricType {
  kAbortedByUser = 0,
  kContextDestroyed = 1,
  kActivatedByTimeout = 2,
  kActivatedOnEnteredBackForwardCache = 3,
  kMaxValue = kActivatedOnEnteredBackForwardCache,
};

void LogFetchLaterMetric(const FetchLaterRendererMetricType& type) {
  base::UmaHistogramEnumeration("FetchLater.Renderer.Metrics", type);
}

// Tells whether the FetchLater request should use BackgroundSync permission to
// decide whether it should send out deferred requests on entering
// BackForwardCache.
bool IsFetchLaterUseBackgroundSyncPermissionEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kFetchLaterAPI, "use_background_sync_permission", true);
}

// Allows manually overriding the "send-on-enter-bfcache" behavior without
// considering BackgroundSync permission.
// Defaults to true to flush on entering BackForwardCache.
// See also
// https://github.com/WICG/pending-beacon/issues/30#issuecomment-1333869614
bool IsFetchLaterSendOnEnterBackForwardCacheEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kFetchLaterAPI,
                                                 "send_on_enter_bfcache", true);
}

// Tells whether the FetchLater should use the "deferred-fetch" policy.
// Defaults to false until the discussion is finalized.
// https://github.com/WICG/pending-beacon/issues/87#issuecomment-2315624105
bool IsFetchLaterUsePermissionsPolicyEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kFetchLaterAPI, "use_permissions_policy", false);
}

bool HasNonEmptyLocationHeader(const FetchHeaderList* headers) {
  String value;
  if (!headers->Get(http_names::kLocation, value))
    return false;
  return !value.empty();
}

const char* SerializeTrustTokenOperationType(
    network::mojom::TrustTokenOperationType operation_type) {
  switch (operation_type) {
    case network::mojom::blink::TrustTokenOperationType::kIssuance:
      return "Issuance";
    case network::mojom::blink::TrustTokenOperationType::kRedemption:
      return "Redemption";
    case network::mojom::blink::TrustTokenOperationType::kSigning:
      return "Signing";
  }
}

// Logs a net error describing why a fetch with Trust Tokens parameters
// failed. This is a temporary measure for debugging a surprisingly high
// incidence of "TypeError: Failed to fetch" when executing Trust Tokens
// issuance operations (crbug.com/1128174).
void HistogramNetErrorForTrustTokensOperation(
    network::mojom::blink::TrustTokenOperationType operation_type,
    int net_error) {
  base::UmaHistogramSparse(
      base::StrCat({"Net.TrustTokens.NetErrorForFetchFailure", ".",
                    SerializeTrustTokenOperationType(operation_type)}),
      net_error);
}

ResourceLoadPriority ComputeFetchLaterLoadPriority(
    const FetchParameters& params) {
  // FetchLater's ResourceType is ResourceType::kRaw, which should default to
  // ResourceLoadPriority::kHigh priority. See also TypeToPriority() in
  // resource_fetcher.cc
  return AdjustPriorityWithPriorityHintAndRenderBlocking(
      ResourceLoadPriority::kHigh, kFetchLaterResourceType,
      params.GetResourceRequest().GetFetchPriorityHint(),
      params.GetRenderBlockingBehavior());
  // TODO(crbug.com/1465781): Apply kLow when IsSubframeDeprioritizationEnabled.
}

class FetchManagerResourceRequestContext final : public ResourceRequestContext {
  STACK_ALLOCATED();

 public:
  ~FetchManagerResourceRequestContext() override = default;

  // Computes the ResourceLoadPriority. This is called if the priority was not
  // set.
  ResourceLoadPriority ComputeLoadPriority(
      const FetchParameters& params) override {
    return ComputeFetchLaterLoadPriority(params);
  }

  void RecordTrace() override {}
};

}  // namespace

// FetchLoaderBase provides common logic to prepare a blink::ResourceRequest
// from a FetchRequestData.
class FetchLoaderBase : public GarbageCollectedMixin {
 public:
  explicit FetchLoaderBase(ExecutionContext* ec,
                           FetchRequestData* data,
                           ScriptState* script_state,
                           AbortSignal* signal)
      : execution_context_(ec),
        fetch_request_data_(data),
        script_state_(script_state),
        world_(std::move(&script_state->World())),
        signal_(signal),
        abort_handle_(signal->AddAlgorithm(
            WTF::BindOnce(&FetchLoaderBase::Abort, WrapWeakPersistent(this)))) {
    CHECK(world_);
  }

  // Starts to perform the "Fetching" algorithm.
  // https://fetch.spec.whatwg.org/#fetching
  // Note that the actual loading is delegated to subclass via `CreateLoader()`,
  // which may or may not start loading immediately.
  void Start(ExceptionState&);

  // Disposes this loader.
  // The owner of this loader uses this method to notify disposing of this
  // loader after removing from its container.
  // Depending on how subclass is implemented, this method may be called
  // multiple times before this instance is gone.
  virtual void Dispose() = 0;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    visitor->Trace(fetch_request_data_);
    visitor->Trace(script_state_);
    visitor->Trace(signal_);
    visitor->Trace(abort_handle_);
    visitor->Trace(world_);
  }

 protected:
  virtual bool IsDeferred() const = 0;
  virtual void Abort() = 0;
  virtual void CreateLoader(
      ResourceRequest request,
      const ResourceLoaderOptions& resource_loader_options) = 0;
  virtual void Failed(
      const String& message,
      DOMException* dom_exception,
      std::optional<String> devtools_request_id = std::nullopt,
      std::optional<base::UnguessableToken> issue_id = std::nullopt) = 0;

  void PerformSchemeFetch(ExceptionState&);
  void PerformNetworkError(
      const String& message,
      std::optional<base::UnguessableToken> issue_id = std::nullopt);
  void FileIssueAndPerformNetworkError(RendererCorsIssueCode,
                                       int64_t identifier);
  void PerformHTTPFetch(ExceptionState&);
  void PerformDataFetch();
  bool AddConsoleMessage(const String& message,
                         std::optional<base::UnguessableToken> issue_id);

  ExecutionContext* GetExecutionContext() { return execution_context_.Get(); }
  void SetExecutionContext(ExecutionContext* ec) { execution_context_ = ec; }
  FetchRequestData* GetFetchRequestData() const {
    return fetch_request_data_.Get();
  }
  ScriptState* GetScriptState() { return script_state_.Get(); }
  const DOMWrapperWorld* World() { return world_; }
  AbortSignal* Signal() { return signal_.Get(); }

 private:
  Member<ExecutionContext> execution_context_;
  Member<FetchRequestData> fetch_request_data_;
  Member<ScriptState> script_state_;
  Member<const DOMWrapperWorld> world_;
  Member<AbortSignal> signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
};

class FetchManager::Loader final
    : public GarbageCollected<FetchManager::Loader>,
      public FetchLoaderBase,
      public ThreadableLoaderClient {
 public:
  Loader(ExecutionContext*,
         FetchManager*,
         ScriptPromiseResolver<Response>*,
         FetchRequestData*,
         ScriptState*,
         AbortSignal*);
  ~Loader() override;
  void Trace(Visitor*) const override;

  void Dispose() override;

  void LogIfKeepalive(std::string_view request_state) const;

  // ThreadableLoaderClient implementation.
  bool WillFollowRedirect(uint64_t,
                          const KURL&,
                          const ResourceResponse&) override;
  void DidReceiveResponse(uint64_t, const ResourceResponse&) override;
  void DidReceiveCachedMetadata(mojo_base::BigBuffer) override;
  void DidStartLoadingResponseBody(BytesConsumer&) override;
  void DidFinishLoading(uint64_t) override;
  void DidFail(uint64_t, const ResourceError&) override;
  void DidFailRedirectCheck(uint64_t) override;

  class SRIVerifier final : public GarbageCollected<SRIVerifier>,
                            public BytesConsumer::Client {
   public:
    SRIVerifier(BytesConsumer* body,
                PlaceHolderBytesConsumer* updater,
                Response* response,
                FetchManager::Loader* loader,
                String integrity_metadata,
                const KURL& url,
                FetchResponseType response_type)
        : body_(body),
          updater_(updater),
          response_(response),
          loader_(loader),
          integrity_metadata_(integrity_metadata),
          url_(url),
          response_type_(response_type) {
      body_->SetClient(this);

      OnStateChange();
    }

    void Cancel() { body_->Cancel(); }

    void OnStateChange() override {
      using Result = BytesConsumer::Result;

      DCHECK(loader_);
      DCHECK(response_);

      Result result = Result::kOk;
      while (result == Result::kOk) {
        const char* buffer;
        size_t available;
        result = body_->BeginRead(&buffer, &available);
        if (result == Result::kOk) {
          buffer_.Append(base::make_span(
              buffer, base::checked_cast<wtf_size_t>(available)));
          result = body_->EndRead(available);
        }
        if (result == Result::kShouldWait)
          return;
      }

      finished_ = true;
      if (result == Result::kDone) {
        SubresourceIntegrity::ReportInfo report_info;
        bool check_result = true;
        bool body_is_null = !updater_;
        if (body_is_null || (response_type_ != FetchResponseType::kBasic &&
                             response_type_ != FetchResponseType::kCors &&
                             response_type_ != FetchResponseType::kDefault)) {
          report_info.AddConsoleErrorMessage(
              "Subresource Integrity: The resource '" + url_.ElidedString() +
              "' has an integrity attribute, but the response is not "
              "eligible for integrity validation.");
          check_result = false;
        }
        if (check_result) {
          check_result = SubresourceIntegrity::CheckSubresourceIntegrity(
              integrity_metadata_,
              SubresourceIntegrityHelper::GetFeatures(
                  loader_->GetExecutionContext()),
              &buffer_, url_, report_info);
        }
        SubresourceIntegrityHelper::DoReport(*loader_->GetExecutionContext(),
                                             report_info);
        if (check_result) {
          updater_->Update(
              MakeGarbageCollected<FormDataBytesConsumer>(std::move(buffer_)));
          loader_->resolver_->Resolve(response_);
          loader_->resolver_.Clear();
          return;
        }
      }
      String error_message =
          "Unknown error occurred while trying to verify integrity.";
      if (updater_) {
        updater_->Update(
            BytesConsumer::CreateErrored(BytesConsumer::Error(error_message)));
      }
      loader_->PerformNetworkError(error_message);
    }

    String DebugName() const override { return "SRIVerifier"; }

    bool IsFinished() const { return finished_; }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(body_);
      visitor->Trace(updater_);
      visitor->Trace(response_);
      visitor->Trace(loader_);
    }

   private:
    Member<BytesConsumer> body_;
    Member<PlaceHolderBytesConsumer> updater_;
    // We cannot store a Response because its JS wrapper can be collected.
    // TODO(yhirano): Fix this.
    Member<Response> response_;
    Member<FetchManager::Loader> loader_;
    String integrity_metadata_;
    KURL url_;
    const FetchResponseType response_type_;
    SegmentedBuffer buffer_;
    bool finished_ = false;
  };

 private:
  bool IsDeferred() const override;
  void Abort() override;
  void NotifyFinished();
  void CreateLoader(
      ResourceRequest request,
      const ResourceLoaderOptions& resource_loader_options) override;
  // If |dom_exception| is provided, throws the specified DOMException instead
  // of the usual "Failed to fetch" TypeError.
  void Failed(
      const String& message,
      DOMException* dom_exception,
      std::optional<String> devtools_request_id = std::nullopt,
      std::optional<base::UnguessableToken> issue_id = std::nullopt) override;

  Member<FetchManager> fetch_manager_;
  Member<ScriptPromiseResolver<Response>> resolver_;
  Member<ThreadableLoader> threadable_loader_;
  Member<PlaceHolderBytesConsumer> place_holder_body_;
  bool failed_;
  bool finished_;
  int response_http_status_code_;
  bool response_has_no_store_header_ = false;
  Member<SRIVerifier> integrity_verifier_;
  Vector<KURL> url_list_;
  Member<ScriptCachedMetadataHandler> cached_metadata_handler_;
  TraceWrapperV8Reference<v8::Value> exception_;
  base::TimeTicks request_started_time_;
};

FetchManager::Loader::Loader(ExecutionContext* execution_context,
                             FetchManager* fetch_manager,
                             ScriptPromiseResolver<Response>* resolver,
                             FetchRequestData* fetch_request_data,
                             ScriptState* script_state,
                             AbortSignal* signal)
    : FetchLoaderBase(execution_context,
                      fetch_request_data,
                      script_state,
                      signal),
      fetch_manager_(fetch_manager),
      resolver_(resolver),
      failed_(false),
      finished_(false),
      response_http_status_code_(0),
      integrity_verifier_(nullptr),
      request_started_time_(base::TimeTicks::Now()) {
  DCHECK(World());
  url_list_.push_back(fetch_request_data->Url());
  v8::Isolate* isolate = script_state->GetIsolate();
  // Only use a handle scope as we should be in the right context already.
  v8::HandleScope scope(isolate);
  // Create the exception at this point so we get the stack-trace that belongs
  // to the fetch() call.
  v8::Local<v8::Value> exception =
      V8ThrowException::CreateTypeError(isolate, "Failed to fetch");
  exception_.Reset(isolate, exception);
}

FetchManager::Loader::~Loader() {
  DCHECK(!threadable_loader_);
}

void FetchManager::Loader::Trace(Visitor* visitor) const {
  visitor->Trace(fetch_manager_);
  visitor->Trace(resolver_);
  visitor->Trace(threadable_loader_);
  visitor->Trace(place_holder_body_);
  visitor->Trace(integrity_verifier_);
  visitor->Trace(cached_metadata_handler_);
  visitor->Trace(exception_);
  FetchLoaderBase::Trace(visitor);
  ThreadableLoaderClient::Trace(visitor);
}

bool FetchManager::Loader::WillFollowRedirect(
    uint64_t identifier,
    const KURL& url,
    const ResourceResponse& response) {
  const auto redirect_mode = GetFetchRequestData()->Redirect();
  if (redirect_mode == network::mojom::RedirectMode::kError) {
    DidFailRedirectCheck(identifier);
    Dispose();
    return false;
  }

  if (redirect_mode == network::mojom::RedirectMode::kManual) {
    const uint64_t unused = 0;
    // There is no need to read the body of redirect response because there is
    // no way to read the body of opaque-redirect filtered response's internal
    // response.
    // TODO(horo): If we support any API which expose the internal body, we
    // will have to read the body. And also HTTPCache changes will be needed
    // because it doesn't store the body of redirect responses.
    DidReceiveResponse(unused, response);
    DidStartLoadingResponseBody(*BytesConsumer::CreateClosed());

    if (threadable_loader_)
      NotifyFinished();

    Dispose();
    return false;
  }

  DCHECK_EQ(redirect_mode, network::mojom::RedirectMode::kFollow);
  url_list_.push_back(url);
  return true;
}

void FetchManager::Loader::DidReceiveResponse(
    uint64_t,
    const ResourceResponse& response) {
  // Verify that we're dealing with the URL we expect (which could be an
  // HTTPS-upgraded variant of `url_list_.back()`.
  DCHECK(
      response.CurrentRequestUrl() == url_list_.back() ||
      (response.CurrentRequestUrl().ProtocolIs("https") &&
       url_list_.back().ProtocolIs("http") &&
       response.CurrentRequestUrl().Host() == url_list_.back().Host() &&
       response.CurrentRequestUrl().GetPath() == url_list_.back().GetPath() &&
       response.CurrentRequestUrl().Query() == url_list_.back().Query()));

  auto response_type = response.GetType();
  DCHECK_NE(response_type, FetchResponseType::kError);

  LogIfKeepalive("Succeeded");

  ScriptState::Scope scope(GetScriptState());

  response_http_status_code_ = response.HttpStatusCode();

  if (response.MimeType() == "application/wasm" &&
      (response.CurrentRequestUrl().ProtocolIsInHTTPFamily() ||
       CommonSchemeRegistry::IsExtensionScheme(
           response.CurrentRequestUrl().Protocol().Ascii()))) {
    // We create a ScriptCachedMetadataHandler for WASM modules.
    cached_metadata_handler_ =
        MakeGarbageCollected<ScriptCachedMetadataHandler>(
            WTF::TextEncoding(),
            CachedMetadataSender::Create(
                response, mojom::blink::CodeCacheType::kWebAssembly,
                GetExecutionContext()->GetSecurityOrigin()));
  }

  place_holder_body_ = MakeGarbageCollected<PlaceHolderBytesConsumer>();
  FetchResponseData* response_data = FetchResponseData::CreateWithBuffer(
      BodyStreamBuffer::Create(GetScriptState(), place_holder_body_, Signal(),
                               cached_metadata_handler_));
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed() ||
      response.GetType() == FetchResponseType::kError) {
    // BodyStreamBuffer::Create() may run scripts and cancel this request.
    // Do nothing in such a case.
    // See crbug.com/1373785 for more details.
    return;
  }

  DCHECK_EQ(response_type, response.GetType());
  DCHECK(!(network_utils::IsRedirectResponseCode(response_http_status_code_) &&
           HasNonEmptyLocationHeader(response_data->HeaderList()) &&
           GetFetchRequestData()->Redirect() != RedirectMode::kManual));

  if (network_utils::IsRedirectResponseCode(response_http_status_code_) &&
      GetFetchRequestData()->Redirect() == RedirectMode::kManual) {
    response_type = network::mojom::FetchResponseType::kOpaqueRedirect;
  }

  response_data->InitFromResourceResponse(
      GetExecutionContext(), response_type, url_list_,
      GetFetchRequestData()->Method(), GetFetchRequestData()->Credentials(),
      response);

  FetchResponseData* tainted_response = nullptr;
  switch (response_type) {
    case FetchResponseType::kBasic:
    case FetchResponseType::kDefault:
      tainted_response = response_data->CreateBasicFilteredResponse();
      break;
    case FetchResponseType::kCors: {
      HTTPHeaderSet header_names = cors::ExtractCorsExposedHeaderNamesList(
          GetFetchRequestData()->Credentials(), response);
      tainted_response =
          response_data->CreateCorsFilteredResponse(header_names);
      break;
    }
    case FetchResponseType::kOpaque:
      tainted_response = response_data->CreateOpaqueFilteredResponse();
      break;
    case FetchResponseType::kOpaqueRedirect:
      tainted_response = response_data->CreateOpaqueRedirectFilteredResponse();
      break;
    case FetchResponseType::kError:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  // TODO(crbug.com/1288221): Remove this once the investigation is done.
  CHECK(tainted_response);

  response_has_no_store_header_ = response.CacheControlContainsNoStore();

  Response* r =
      Response::Create(resolver_->GetExecutionContext(), tainted_response);
  r->headers()->SetGuard(Headers::kImmutableGuard);
  if (GetFetchRequestData()->Integrity().empty()) {
    resolver_->Resolve(r);
    resolver_.Clear();
  } else {
    DCHECK(!integrity_verifier_);
    // We have another place holder body for SRI.
    PlaceHolderBytesConsumer* verified = place_holder_body_;
    place_holder_body_ = MakeGarbageCollected<PlaceHolderBytesConsumer>();
    BytesConsumer* underlying = place_holder_body_;

    integrity_verifier_ = MakeGarbageCollected<SRIVerifier>(
        underlying, verified, r, this, GetFetchRequestData()->Integrity(),
        response.CurrentRequestUrl(), r->GetResponse()->GetType());
  }
}

void FetchManager::Loader::DidReceiveCachedMetadata(mojo_base::BigBuffer data) {
  if (cached_metadata_handler_) {
    cached_metadata_handler_->SetSerializedCachedMetadata(std::move(data));
  }
}

void FetchManager::Loader::DidStartLoadingResponseBody(BytesConsumer& body) {
  if (GetFetchRequestData()->Integrity().empty() &&
      !response_has_no_store_header_) {
    // BufferingBytesConsumer reads chunks from |bytes_consumer| as soon as
    // they get available to relieve backpressure.  Buffering starts after
    // a short delay, however, to allow the Response to be drained; e.g.
    // when the Response is passed to FetchEvent.respondWith(), etc.
    //
    // https://fetch.spec.whatwg.org/#fetching
    // The user agent should ignore the suspension request if the ongoing
    // fetch is updating the response in the HTTP cache for the request.
    place_holder_body_->Update(BufferingBytesConsumer::CreateWithDelay(
        &body, GetExecutionContext()->GetTaskRunner(TaskType::kNetworking)));
  } else {
    place_holder_body_->Update(&body);
  }
  place_holder_body_ = nullptr;
}

void FetchManager::Loader::DidFinishLoading(uint64_t) {
  DCHECK(!place_holder_body_);
  DCHECK(!failed_);

  finished_ = true;

  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  if (window && window->GetFrame() &&
      network::IsSuccessfulStatus(response_http_status_code_)) {
    window->GetFrame()->GetPage()->GetChromeClient().AjaxSucceeded(
        window->GetFrame());
  }
  NotifyFinished();
}

void FetchManager::Loader::DidFail(uint64_t identifier,
                                   const ResourceError& error) {
  if (GetFetchRequestData() && GetFetchRequestData()->TrustTokenParams()) {
    HistogramNetErrorForTrustTokensOperation(
        GetFetchRequestData()->TrustTokenParams()->operation,
        error.ErrorCode());
  }

  if (error.TrustTokenOperationError() !=
      network::mojom::blink::TrustTokenOperationStatus::kOk) {
    Failed(String(),
           TrustTokenErrorToDOMException(error.TrustTokenOperationError()),
           IdentifiersFactory::SubresourceRequestId(identifier));
    return;
  }

  auto issue_id = error.CorsErrorStatus()
                      ? std::optional<base::UnguessableToken>(
                            error.CorsErrorStatus()->issue_id)
                      : std::nullopt;
  Failed(String(), nullptr,
         IdentifiersFactory::SubresourceRequestId(identifier), issue_id);
}

void FetchManager::Loader::DidFailRedirectCheck(uint64_t identifier) {
  Failed(String(), nullptr,
         IdentifiersFactory::SubresourceRequestId(identifier));
}

void FetchLoaderBase::Start(ExceptionState& exception_state) {
  // "1. If |request|'s url contains a Known HSTS Host, modify it per the
  // requirements of the 'URI [sic] Loading and Port Mapping' chapter of HTTP
  // Strict Transport Security."
  // FIXME: Implement this.

  // "2. If |request|'s referrer is not none, set |request|'s referrer to the
  // result of invoking determine |request|'s referrer."
  // We set the referrer using workerGlobalScope's URL in
  // WorkerThreadableLoader.

  // "3. If |request|'s synchronous flag is unset and fetch is not invoked
  // recursively, run the remaining steps asynchronously."
  // We don't support synchronous flag.

  // "4. Let response be the value corresponding to the first matching
  // statement:"

  // "- should fetching |request| be blocked as mixed content returns blocked"
  // We do mixed content checking in ResourceFetcher.

  // "- should fetching |request| be blocked as content security returns
  //    blocked"
  CHECK(execution_context_);
  if (!execution_context_->GetContentSecurityPolicyForWorld(world_.Get())
           ->AllowConnectToSource(fetch_request_data_->Url(),
                                  fetch_request_data_->Url(),
                                  RedirectStatus::kNoRedirect)) {
    // "A network error."
    PerformNetworkError(
        "Refused to connect to '" + fetch_request_data_->Url().ElidedString() +
        "' because it violates the document's Content Security Policy.");
    return;
  }

  const KURL& url = fetch_request_data_->Url();
  // "- |request|'s url's origin is same origin with |request|'s origin,
  //    |request|'s tainted origin flag is unset, and the CORS flag is unset"
  // Note tainted origin flag is always unset here.
  // Note we don't support to call this method with |CORS flag|
  // "- |request|'s current URL's scheme is |data|"
  // "- |request|'s mode is |navigate| or |websocket|".
  if (fetch_request_data_->Origin()->CanReadContent(url) ||
      (fetch_request_data_->IsolatedWorldOrigin() &&
       fetch_request_data_->IsolatedWorldOrigin()->CanReadContent(url)) ||
      fetch_request_data_->Mode() == network::mojom::RequestMode::kNavigate) {
    // "The result of performing a scheme fetch using request."
    PerformSchemeFetch(exception_state);
    return;
  }

  // "- |request|'s mode is |same-origin|"
  if (fetch_request_data_->Mode() == RequestMode::kSameOrigin) {
    // This error is so early that there isn't an identifier yet, generate one.
    FileIssueAndPerformNetworkError(RendererCorsIssueCode::kDisallowedByMode,
                                    CreateUniqueIdentifier());
    return;
  }

  // "- |request|'s mode is |no CORS|"
  if (fetch_request_data_->Mode() == RequestMode::kNoCors) {
    // "If |request|'s redirect mode is not |follow|, then return a network
    // error.
    if (fetch_request_data_->Redirect() != RedirectMode::kFollow) {
      // This error is so early that there isn't an identifier yet, generate
      // one.
      FileIssueAndPerformNetworkError(
          RendererCorsIssueCode::kNoCorsRedirectModeNotFollow,
          CreateUniqueIdentifier());
      return;
    }

    // "Set |request|'s response tainting to |opaque|."
    // Response tainting is calculated in the CORS module in the network
    // service.
    //
    // "The result of performing a scheme fetch using |request|."
    PerformSchemeFetch(exception_state);
    return;
  }

  // "- |request|'s url's scheme is not one of 'http' and 'https'"
  // This may include other HTTP-like schemes if the embedder has added them
  // to SchemeRegistry::registerURLSchemeAsSupportingFetchAPI.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          fetch_request_data_->Url().Protocol())) {
    // This error is so early that there isn't an identifier yet, generate one.
    FileIssueAndPerformNetworkError(RendererCorsIssueCode::kCorsDisabledScheme,
                                    CreateUniqueIdentifier());
    return;
  }

  // "Set |request|'s response tainting to |CORS|."
  // Response tainting is calculated in the CORS module in the network
  // service.

  // "The result of performing an HTTP fetch using |request| with the
  // |CORS flag| set."
  PerformHTTPFetch(exception_state);
}

void FetchManager::Loader::Dispose() {
  // Prevent notification
  fetch_manager_ = nullptr;
  if (threadable_loader_) {
    if (GetFetchRequestData()->Keepalive()) {
      threadable_loader_->Detach();
    } else {
      threadable_loader_->Cancel();
    }
    threadable_loader_ = nullptr;
  }
  if (integrity_verifier_)
    integrity_verifier_->Cancel();
  SetExecutionContext(nullptr);
}

// https://fetch.spec.whatwg.org/#abort-fetch
// To abort a fetch() call with a promise, request, responseObject, and an
// error:
void FetchManager::Loader::Abort() {
  ScriptState* script_state = GetScriptState();
  v8::Local<v8::Value> error = Signal()->reason(script_state).V8Value();
  // 1. Reject promise with error.
  if (resolver_) {
    resolver_->Reject(error);
    resolver_.Clear();
  }
  if (threadable_loader_) {
    // Prevent re-entrancy.
    auto loader = threadable_loader_;
    threadable_loader_ = nullptr;
    loader->Cancel();
  }

  // 2. If request’s body is non-null and is readable, then cancel request’s
  //  body with error.
  if (FetchRequestData* fetch_request_data = GetFetchRequestData()) {
    if (BodyStreamBuffer* body_stream_buffer = fetch_request_data->Buffer()) {
      if (ReadableStream* readable_stream = body_stream_buffer->Stream()) {
        ReadableStream::Cancel(script_state, readable_stream, error);
      }
    }
  }
  NotifyFinished();
}

void FetchLoaderBase::PerformSchemeFetch(ExceptionState& exception_state) {
  // "To perform a scheme fetch using |request|, switch on |request|'s url's
  // scheme, and run the associated steps:"
  if (SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          fetch_request_data_->Url().Protocol()) ||
      fetch_request_data_->Url().ProtocolIs("blob")) {
    // "Return the result of performing an HTTP fetch using |request|."
    PerformHTTPFetch(exception_state);
  } else if (fetch_request_data_->Url().ProtocolIsData()) {
    PerformDataFetch();
  } else {
    // FIXME: implement other protocols.
    // This error is so early that there isn't an identifier yet, generate one.
    FileIssueAndPerformNetworkError(RendererCorsIssueCode::kCorsDisabledScheme,
                                    CreateUniqueIdentifier());
  }
}

void FetchLoaderBase::FileIssueAndPerformNetworkError(
    RendererCorsIssueCode network_error,
    int64_t identifier) {
  auto issue_id = base::UnguessableToken::Create();
  switch (network_error) {
    case RendererCorsIssueCode::kCorsDisabledScheme: {
      AuditsIssue::ReportCorsIssue(
          execution_context_, identifier, network_error,
          fetch_request_data_->Url().GetString(),
          fetch_request_data_->Origin()->ToString(),
          fetch_request_data_->Url().Protocol(), issue_id);
      PerformNetworkError(
          "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
              ". URL scheme \"" + fetch_request_data_->Url().Protocol() +
              "\" is not supported.",
          issue_id);
      break;
    }
    case RendererCorsIssueCode::kDisallowedByMode: {
      AuditsIssue::ReportCorsIssue(execution_context_, identifier,
                                   network_error,
                                   fetch_request_data_->Url().GetString(),
                                   fetch_request_data_->Origin()->ToString(),
                                   WTF::g_empty_string, issue_id);
      PerformNetworkError(
          "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
              ". Request mode is \"same-origin\" but the URL\'s "
              "origin is not same as the request origin " +
              fetch_request_data_->Origin()->ToString() + ".",
          issue_id);

      break;
    }
    case RendererCorsIssueCode::kNoCorsRedirectModeNotFollow: {
      AuditsIssue::ReportCorsIssue(execution_context_, identifier,
                                   network_error,
                                   fetch_request_data_->Url().GetString(),
                                   fetch_request_data_->Origin()->ToString(),
                                   WTF::g_empty_string, issue_id);
      PerformNetworkError(
          "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
              ". Request mode is \"no-cors\" but the redirect mode "
              "is not \"follow\".",
          issue_id);
      break;
    }
  }
}

void FetchLoaderBase::PerformNetworkError(
    const String& message,
    std::optional<base::UnguessableToken> issue_id) {
  Failed(message, nullptr, std::nullopt, issue_id);
}

void FetchLoaderBase::PerformHTTPFetch(ExceptionState& exception_state) {
  // CORS preflight fetch procedure is implemented inside ThreadableLoader.

  // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
  //  body is a tee of |request|'s body."
  // We use ResourceRequest class for HTTPRequest.
  // FIXME: Support body.
  ResourceRequest request(fetch_request_data_->Url());
  request.SetRequestorOrigin(fetch_request_data_->Origin());
  request.SetNavigationRedirectChain(
      fetch_request_data_->NavigationRedirectChain());
  request.SetIsolatedWorldOrigin(fetch_request_data_->IsolatedWorldOrigin());
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetRequestDestination(fetch_request_data_->Destination());
  request.SetFetchLikeAPI(true);
  request.SetHttpMethod(fetch_request_data_->Method());
  request.SetFetchWindowId(fetch_request_data_->WindowId());
  request.SetTrustTokenParams(fetch_request_data_->TrustTokenParams());
  request.SetMode(fetch_request_data_->Mode());
  request.SetTargetAddressSpace(fetch_request_data_->TargetAddressSpace());

  request.SetCredentialsMode(fetch_request_data_->Credentials());
  for (const auto& header : fetch_request_data_->HeaderList()->List()) {
    request.AddHttpHeaderField(AtomicString(header.first),
                               AtomicString(header.second));
  }

  if (fetch_request_data_->Method() != http_names::kGET &&
      fetch_request_data_->Method() != http_names::kHEAD) {
    if (fetch_request_data_->Buffer()) {
      scoped_refptr<EncodedFormData> form_data =
          fetch_request_data_->Buffer()->DrainAsFormData(exception_state);
      if (form_data) {
        request.SetHttpBody(form_data);
      } else if (RuntimeEnabledFeatures::FetchUploadStreamingEnabled(
                     execution_context_)) {
        UseCounter::Count(execution_context_,
                          WebFeature::kFetchUploadStreaming);
        DCHECK(!fetch_request_data_->Buffer()->IsStreamLocked());
        mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
            pending_remote;
        fetch_request_data_->Buffer()->DrainAsChunkedDataPipeGetter(
            script_state_, pending_remote.InitWithNewPipeAndPassReceiver(),
            /*client=*/nullptr);
        request.MutableBody().SetStreamBody(std::move(pending_remote));
      }
    }
  }
  request.SetCacheMode(fetch_request_data_->CacheMode());
  request.SetRedirectMode(fetch_request_data_->Redirect());
  request.SetFetchPriorityHint(fetch_request_data_->FetchPriorityHint());
  request.SetPriority(fetch_request_data_->Priority());
  request.SetUseStreamOnResponse(true);
  request.SetReferrerString(fetch_request_data_->ReferrerString());
  request.SetReferrerPolicy(fetch_request_data_->GetReferrerPolicy());

  if (IsDeferred()) {
    // https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#request-a-deferred-fetch
    // "Deferred fetching"
    // 4. Set request’s service-workers mode to "none".
    request.SetSkipServiceWorker(true);
  } else {
    request.SetSkipServiceWorker(world_->IsIsolatedWorld());
  }

  if (fetch_request_data_->Keepalive()) {
    request.SetKeepalive(true);
    UseCounter::Count(execution_context_, mojom::WebFeature::kFetchKeepalive);
  }

  request.SetBrowsingTopics(fetch_request_data_->BrowsingTopics());
  request.SetAdAuctionHeaders(fetch_request_data_->AdAuctionHeaders());
  request.SetAttributionReportingEligibility(
      fetch_request_data_->AttributionReportingEligibility());
  request.SetAttributionReportingSupport(
      fetch_request_data_->AttributionSupport());
  request.SetSharedStorageWritableOptedIn(
      fetch_request_data_->SharedStorageWritable());

  request.SetOriginalDestination(fetch_request_data_->OriginalDestination());

  request.SetServiceWorkerRaceNetworkRequestToken(
      fetch_request_data_->ServiceWorkerRaceNetworkRequestToken());

  request.SetFetchLaterAPI(IsDeferred());

  if (execution_context_->IsSharedWorkerGlobalScope() &&
      DynamicTo<SharedWorkerGlobalScope>(*execution_context_)
          ->DoesRequireCrossSiteRequestForCookies()) {
    request.SetSiteForCookies(net::SiteForCookies());
  }

  // "3. Append `Host`, ..."
  // FIXME: Implement this when the spec is fixed.

  // "4.If |HTTPRequest|'s force Origin header flag is set, append `Origin`/
  // |HTTPRequest|'s origin, serialized and utf-8 encoded, to |HTTPRequest|'s
  // header list."
  // We set Origin header in updateRequestForAccessControl() called from
  // ThreadableLoader::makeCrossOriginAccessRequest

  // "5. Let |credentials flag| be set if either |HTTPRequest|'s credentials
  // mode is |include|, or |HTTPRequest|'s credentials mode is |same-origin|
  // and the |CORS flag| is unset, and unset otherwise."

  ResourceLoaderOptions resource_loader_options(world_);
  resource_loader_options.initiator_info.name =
      fetch_initiator_type_names::kFetch;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;
  if (fetch_request_data_->URLLoaderFactory()) {
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory> factory_clone;
    fetch_request_data_->URLLoaderFactory()->Clone(
        factory_clone.InitWithNewPipeAndPassReceiver());
    resource_loader_options.url_loader_factory =
        base::MakeRefCounted<base::RefCountedData<
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>>>(
            std::move(factory_clone));
  }

  if (fetch_request_data_->Keepalive() && !request.IsFetchLaterAPI()) {
    FetchUtils::LogFetchKeepAliveRequestMetric(
        request.GetRequestContext(),
        FetchUtils::FetchKeepAliveRequestState::kTotal);
  }
  CreateLoader(std::move(request), resource_loader_options);
}

// performDataFetch() is almost the same as performHTTPFetch(), except for:
// - We set AllowCrossOriginRequests to allow requests to data: URLs in
//   'same-origin' mode.
// - We reject non-GET method.
void FetchLoaderBase::PerformDataFetch() {
  DCHECK(fetch_request_data_->Url().ProtocolIsData());

  ResourceRequest request(fetch_request_data_->Url());
  request.SetRequestorOrigin(fetch_request_data_->Origin());
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetRequestDestination(fetch_request_data_->Destination());
  request.SetFetchLikeAPI(true);
  request.SetUseStreamOnResponse(true);
  request.SetHttpMethod(fetch_request_data_->Method());
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  request.SetRedirectMode(RedirectMode::kError);
  request.SetFetchPriorityHint(fetch_request_data_->FetchPriorityHint());
  request.SetPriority(fetch_request_data_->Priority());
  // We intentionally skip 'setExternalRequestStateFromRequestorAddressSpace',
  // as 'data:' can never be external.

  ResourceLoaderOptions resource_loader_options(world_);
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  CreateLoader(std::move(request), resource_loader_options);
}

void FetchManager::Loader::CreateLoader(
    ResourceRequest request,
    const ResourceLoaderOptions& resource_loader_options) {
  threadable_loader_ = MakeGarbageCollected<ThreadableLoader>(
      *GetExecutionContext(), this, resource_loader_options);
  threadable_loader_->Start(std::move(request));
}

bool FetchLoaderBase::AddConsoleMessage(
    const String& message,
    std::optional<base::UnguessableToken> issue_id) {
  if (execution_context_->IsContextDestroyed())
    return false;
  if (!message.empty()) {
    // CORS issues are reported via network service instrumentation, with the
    // exception of early errors reported in FileIssueAndPerformNetworkError.
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kError, message);
    if (issue_id) {
      console_message->SetCategory(mojom::blink::ConsoleMessageCategory::Cors);
    }
    execution_context_->AddConsoleMessage(console_message);
  }
  return true;
}

void FetchManager::Loader::Failed(
    const String& message,
    DOMException* dom_exception,
    std::optional<String> devtools_request_id,
    std::optional<base::UnguessableToken> issue_id) {
  if (failed_ || finished_) {
    return;
  }
  failed_ = true;
  if (!AddConsoleMessage(message, issue_id)) {
    return;
  }
  if (resolver_) {
    ScriptState::Scope scope(GetScriptState());
    if (dom_exception) {
      resolver_->Reject(dom_exception);
    } else {
      v8::Local<v8::Value> value =
          exception_.Get(GetScriptState()->GetIsolate());
      exception_.Reset();
      ThreadDebugger* debugger =
          ThreadDebugger::From(GetScriptState()->GetIsolate());
      if (devtools_request_id) {
        debugger->GetV8Inspector()->associateExceptionData(
            GetScriptState()->GetContext(), value,
            V8AtomicString(GetScriptState()->GetIsolate(), "requestId"),
            V8String(GetScriptState()->GetIsolate(), *devtools_request_id));
      }
      if (issue_id) {
        debugger->GetV8Inspector()->associateExceptionData(
            GetScriptState()->GetContext(), value,
            V8AtomicString(GetScriptState()->GetIsolate(), "issueId"),
            V8String(GetScriptState()->GetIsolate(),
                     IdentifiersFactory::IdFromToken(*issue_id)));
      }
      resolver_->Reject(value);
      LogIfKeepalive("Failed");
    }
  }
  NotifyFinished();
}

void FetchManager::Loader::NotifyFinished() {
  if (fetch_manager_)
    fetch_manager_->OnLoaderFinished(this);
}

bool FetchManager::Loader::IsDeferred() const {
  return false;
}

void FetchManager::Loader::LogIfKeepalive(
    std::string_view request_state) const {
  return;
  CHECK(request_state == "Succeeded" || request_state == "Failed");
  if (!GetFetchRequestData()->Keepalive()) {
    return;
  }

  base::TimeDelta duration = base::TimeTicks::Now() - request_started_time_;
  base::UmaHistogramMediumTimes("FetchKeepAlive.RequestDuration", duration);
  base::UmaHistogramMediumTimes(
      base::StrCat({"FetchKeepAlive.RequestDuration.", request_state}),
      duration);
}

// A subtype of FetchLoader to handle the deferred fetching algorithm [1].
//
// This loader and FetchManager::Loader are similar that they both runs the
// fetching algorithm provided by the base class. However, this loader does not
// go down ThreadableLoader and ResourceFetcher. Rather, it creates requests via
// a similar mojo FetchLaterLoaderFactory. Other differences include:
//   - `IsDeferred()` is true, which helps the base generate different requests.
//   - Expect no response after `Start()` is called.
//   - Support activateAfter from [2] to allow sending at specified time.
//   - Support FetchLaterResult from [2].
//
// Underlying, this loader intends to create a "deferred" fetch request,
// i.e. `ResourceRequest.is_fetch_later_api` is true, when `Start()` is called.
// The request will not be sent by network service (handled via browser)
// immediately until ExecutionContext of the FetchLaterManager is destroyed.
//
// Note that this loader does not use the "defer" mechanism as described in
// `ResourcFetcher::RequestResource()` or `ResourceFetcher::StartLoad()`, as
// the latter method can only be called when ResourcFetcher is not detached.
// Plus, the browser companion must be notified when the context is still alive.
//
// [1]:
// https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#request-a-deferred-fetch
// [2]:
// https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#dom-global-fetch-later
class FetchLaterManager::DeferredLoader final
    : public GarbageCollected<FetchLaterManager::DeferredLoader>,
      public FetchLoaderBase {
 public:
  DeferredLoader(ExecutionContext* ec,
                 FetchLaterManager* fetch_later_manager,
                 FetchRequestData* fetch_request_data,
                 ScriptState* script_state,
                 AbortSignal* signal,
                 const std::optional<base::TimeDelta>& activate_after)
      : FetchLoaderBase(ec, fetch_request_data, script_state, signal),
        fetch_later_manager_(fetch_later_manager),
        fetch_later_result_(MakeGarbageCollected<FetchLaterResult>()),
        activate_after_(activate_after),
        timer_(ec->GetTaskRunner(FetchLaterManager::kTaskType),
               this,
               &DeferredLoader::TimerFired),
        loader_(ec) {
    base::UmaHistogramBoolean("FetchLater.Renderer.Total", true);
    // `timer_` is started in `CreateLoader()` so that it won't end before a
    // request is created.
  }

  FetchLaterResult* fetch_later_result() { return fetch_later_result_.Get(); }

  // FetchLoaderBase overrides:
  void Dispose() override {
    // Prevent notification
    fetch_later_manager_ = nullptr;
    SetExecutionContext(nullptr);

    timer_.Stop();
    // The browser companion will take care of the actual request sending when
    // discoverying the URL loading connections from here are gone.
  }

  void Process(const FetchLaterRendererMetricType& metric_type) {
    // https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#process-a-deferred-fetch
    // To process a deferred fetch deferredRecord:
    // 1. If deferredRecord’s invoke state is not "deferred", then return.
    if (invoke_state_ != InvokeState::DEFERRED) {
      return;
    }
    // 2. Set deferredRecord’s invoke state to "activated".
    SetInvokeState(InvokeState::ACTIVATED);
    // 3. Fetch deferredRecord’s request.
    if (loader_) {
      LogFetchLaterMetric(metric_type);
      loader_->SendNow();
    }
  }

  // Returns this loader's request body length if the followings are all true:
  // - this loader's request has a non-null body.
  // - `url` is "same origin" with this loader's request URL.
  uint64_t GetDeferredBytesForUrlOrigin(const KURL& url) const {
    return GetFetchRequestData()->Buffer() &&
                   SecurityOrigin::AreSameOrigin(GetFetchRequestData()->Url(),
                                                 url)
               ? GetFetchRequestData()->BufferByteLength()
               : 0;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(fetch_later_manager_);
    visitor->Trace(fetch_later_result_);
    visitor->Trace(timer_);
    visitor->Trace(loader_);
    FetchLoaderBase::Trace(visitor);
  }

  // For testing only:
  void RecreateTimerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* tick_clock) {
    timer_.Stop();
    timer_.SetTaskRunnerForTesting(std::move(task_runner), tick_clock);
    if (activate_after_.has_value()) {
      timer_.StartOneShot(*activate_after_, FROM_HERE);
    }
  }

 private:
  enum class InvokeState {
    DEFERRED,
    ABORTED,
    ACTIVATED
  };
  void SetInvokeState(InvokeState state) {
    switch (state) {
      case InvokeState::DEFERRED:
        UseCounter::Count(GetExecutionContext(),
                          WebFeature::kFetchLaterInvokeStateDeferred);
        break;
      case InvokeState::ABORTED:
        UseCounter::Count(GetExecutionContext(),
                          WebFeature::kFetchLaterInvokeStateAborted);
        break;
      case InvokeState::ACTIVATED:
        UseCounter::Count(GetExecutionContext(),
                          WebFeature::kFetchLaterInvokeStateActivated);
        break;
      default:
        NOTREACHED();
    };
    invoke_state_ = state;
    fetch_later_result_->SetActivated(state == InvokeState::ACTIVATED);
  }

  // FetchLoaderBase overrides:
  bool IsDeferred() const override { return true; }
  void Abort() override {
    // https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#dom-global-fetch-later
    // 10. Add the following abort steps to requestObject’s signal:
    // 10-1. Set deferredRecord’s invoke state to "aborted".
    SetInvokeState(InvokeState::ABORTED);
    // 10-2. Remove deferredRecord from request’s client’s fetch group’s
    // deferred fetch records.
    if (loader_) {
      LogFetchLaterMetric(FetchLaterRendererMetricType::kAbortedByUser);
      loader_->Cancel();
    }
    NotifyFinished();
  }
  // Triggered after `Start()`.
  void CreateLoader(
      ResourceRequest request,
      const ResourceLoaderOptions& resource_loader_options) override {
    auto* factory = fetch_later_manager_->GetFactory();
    if (!factory) {
      Failed(/*message=*/String(), /*dom_exception=*/nullptr);
      return;
    }
    std::unique_ptr<network::ResourceRequest> network_request =
        fetch_later_manager_->PrepareNetworkRequest(std::move(request),
                                                    resource_loader_options);
    if (!network_request) {
      Failed(/*message=*/String(), /*dom_exception=*/nullptr);
      return;
    }

    // Don't do mime sniffing for fetch (crbug.com/2016)
    uint32_t url_loader_options = network::mojom::blink::kURLLoadOptionNone;
    // Computes a unique request_id for this renderer process.
    int request_id = GenerateRequestId();
    factory->CreateFetchLaterLoader(
        loader_.BindNewEndpointAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(FetchLaterManager::kTaskType)),
        request_id, url_loader_options, *network_request,
        net::MutableNetworkTrafficAnnotationTag(
            kFetchLaterTrafficAnnotationTag));
    CHECK(loader_.is_bound());
    loader_.set_disconnect_handler(WTF::BindOnce(
        &DeferredLoader::NotifyFinished, WrapWeakPersistent(this)));

    // https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#request-a-deferred-fetch
    // Continued with "request a deferred fetch"
    // 13. If `activate_after_` is not null, then run the following steps in
    // parallel:
    if (activate_after_.has_value()) {
      // 13-1. The user agent should wait until `activate_after_`
      // milliseconds have passed,
      // Implementation followed by `TimerFired()`.
      timer_.StartOneShot(*activate_after_, FROM_HERE);
    }
  }
  void Failed(
      const String& message,
      DOMException* dom_exception,
      std::optional<String> devtools_request_id = std::nullopt,
      std::optional<base::UnguessableToken> issue_id = std::nullopt) override {
    AddConsoleMessage(message, issue_id);
    NotifyFinished();
  }

  // Notifies the owner to remove `this` from its container, after which
  // `Dispose()` will also be called.
  void NotifyFinished() {
    if (fetch_later_manager_) {
      fetch_later_manager_->OnDeferredLoaderFinished(this);
    }
  }

  // Triggered by `timer_`.
  void TimerFired(TimerBase*) {
    // https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#request-a-deferred-fetch
    // Continued with "request a deferred fetch":
    // 13-3. Process a deferred fetch given deferredRecord.
    Process(FetchLaterRendererMetricType::kActivatedByTimeout);
    NotifyFinished();
  }

  // A deferred fetch record's "invoke state" field.
  InvokeState invoke_state_ = InvokeState::DEFERRED;

  // Owns this instance.
  Member<FetchLaterManager> fetch_later_manager_;

  // Retains strong reference to the returned V8 object of a FetchLater API call
  // that creates this loader.
  //
  // The object itself may be held by a script, and may easily outlive `this` if
  // the script keeps holding the object after the FetchLater request completes.
  //
  // This field should be updated whenever `invoke_state_` changes.
  Member<FetchLaterResult> fetch_later_result_;

  // The "activateAfter" to request a deferred fetch.
  // https://whatpr.org/fetch/1647/9ca4bda...7bff4de.html#request-a-deferred-fetch
  const std::optional<base::TimeDelta> activate_after_;
  // A timer to handle `activate_after_`.
  HeapTaskRunnerTimer<DeferredLoader> timer_;

  // Connects to FetchLaterLoader in browser.
  HeapMojoAssociatedRemote<mojom::blink::FetchLaterLoader> loader_;
};

FetchManager::FetchManager(ExecutionContext* execution_context)
    : ExecutionContextLifecycleObserver(execution_context) {}

ScriptPromise<Response> FetchManager::Fetch(ScriptState* script_state,
                                            FetchRequestData* request,
                                            AbortSignal* signal,
                                            ExceptionState& exception_state) {
  DCHECK(signal);
  if (signal->aborted()) {
    return ScriptPromise<Response>::Reject(script_state,
                                           signal->reason(script_state));
  }

  request->SetDestination(network::mojom::RequestDestination::kEmpty);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<Response>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  auto* loader = MakeGarbageCollected<Loader>(
      GetExecutionContext(), this, resolver, request, script_state, signal);
  loaders_.insert(loader);
  // TODO(ricea): Reject the Response body with AbortError, not TypeError.
  loader->Start(exception_state);
  return promise;
}

FetchLaterResult* FetchLaterManager::FetchLater(
    ScriptState* script_state,
    FetchRequestData* request,
    AbortSignal* signal,
    std::optional<DOMHighResTimeStamp> activate_after_ms,
    ExceptionState& exception_state) {
  // https://whatpr.org/fetch/1647.html#dom-global-fetch-later
  // Continuing the fetchLater(input, init) method steps:
  CHECK(signal);
  // 2. If request’s signal is aborted, then throw signal’s abort reason.
  if (signal->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The user aborted a fetchLater request.");
    return nullptr;
  }

  std::optional<base::TimeDelta> activate_after = std::nullopt;
  if (activate_after_ms.has_value()) {
    activate_after = base::Milliseconds(*activate_after_ms);
    // 7. If `activate_after` is less than 0 then throw a RangeError.
    if (activate_after->is_negative()) {
      exception_state.ThrowRangeError(
          "fetchLater's activateAfter cannot be negative.");
      return nullptr;
    }
  }

  // 8. Let deferredRecord be the result of calling "request a deferred fetch"
  // given `request` and `activate_after`. This may throw an exception.
  //
  // "request a deferred fetch":
  // https://whatpr.org/fetch/1647.html#request-a-deferred-fetch

  // 1. If request’s client is not a fully active Document, then throw an
  // "InvalidStateError" DOMException.
  if (!DomWindow() || GetExecutionContext()->is_in_back_forward_cache()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "fetchLater can only be called from a fully active Document.");
    return nullptr;
  }

  // 2. If request’s URL’s scheme is not an HTTPS scheme, then throw a
  // TypeError.
  if (!request->Url().ProtocolIs(WTF::g_https_atom)) {
    exception_state.ThrowTypeError("fetchLater is only supported over HTTPS.");
    return nullptr;
  }
  // 3. If request’s URL is not a potentially trustworthy url, then throw a
  // "SecurityError" DOMException.
  if (!network::IsUrlPotentiallyTrustworthy(GURL(request->Url()))) {
    exception_state.ThrowSecurityError(
        "fetchLater was passed an insecure URL.");
    return nullptr;
  }

  // 4. If request’s client’s fetch group is eligible for deferred fetching is
  // false, then throw a "NotAllowedError" DOMException.
  // https://w3c.github.io/webappsec-permissions-policy/#algo-is-feature-enabled
  // NOTE: The default value of True for report means that most permissions
  // policy checks will generate a violation report if the feature is disabled.
  if (IsFetchLaterUsePermissionsPolicyEnabled() &&
      !GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kDeferredFetch,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Access to fetchLater requires the permissions policy "
        "\"deferred-fetch\" be enabled for the origin of this document.");
    return nullptr;
  }

  // TODO(crbug.com/40276121): Update the following steps to match latest PR.

  // 4. Set request’s service-workers mode to "none".
  // Done in `PerformHTTPFetch()`.
  // 5. If request’s body is not null and request’s body’s source is null, then
  // throw a TypeError.
  // This disallows sending deferred fetches with a live ReadableStream.
  // Equivalent to Step 7 below, as implementation does not set
  // BufferByteLength() for ReadableStream.

  uint64_t bytes_for_origin = 0;
  // 7. If request’s body is not null then:
  if (request->Buffer()) {
    // 7-1. If request’s body’s length is null, then throw a TypeError.
    if (request->BufferByteLength() == 0) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kFetchLaterErrorUnknownBodyLength);
      exception_state.ThrowTypeError(
          "fetchLater doesn't support body with unknown length.");
      return nullptr;
    }
    // 7-2. Set `bytes_for_origin` to request’s body’s length.
    bytes_for_origin = request->BufferByteLength();
  }
  // Run Step 9 below for potential early termination. It also caps
  // `bytes_per_origin`.
  if (bytes_for_origin > kMaxScheduledDeferredBytesPerOrigin) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kFetchLaterErrorQuotaExceeded);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kQuotaExceededError,
        "fetchLater exceeds its quota for the origin.");
    return nullptr;
  }

  // 8. For each deferredRecord in request’s client’s fetch group’s deferred
  // fetch records: if deferredRecord’s request’s body is not null and
  // deferredRecord’s request’s URL’s origin is same origin with request’s
  // URL’s origin, then increment `bytes_for_origin` by deferredRecord’s
  // request’s body’s length.
  for (const auto& deferred_loader : deferred_loaders_) {
    // `bytes_for_orign` is capped below the max (64 kilobytes), and the value
    // returned by every deferred_loader has run through the same cap. Hence,
    // the sum here is guaranteed to be <= 128 kilobytes.
    bytes_for_origin +=
        deferred_loader->GetDeferredBytesForUrlOrigin(request->Url());
    // 9. If `bytes_for_origin` is greater than 64 kilobytes, then throw a
    // QuotaExceededError.
    if (bytes_for_origin > kMaxScheduledDeferredBytesPerOrigin) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kFetchLaterErrorQuotaExceeded);
      exception_state.ThrowDOMException(
          DOMExceptionCode::kQuotaExceededError,
          "fetchLater exceeds its quota for the origin.");
      return nullptr;
    }
  }

  request->SetDestination(network::mojom::RequestDestination::kEmpty);
  // 10. Set request’s keepalive to true.
  request->SetKeepalive(true);

  // 11. Let deferredRecord be a new deferred fetch record whose request is
  // `request`.
  auto* deferred_loader = MakeGarbageCollected<DeferredLoader>(
      GetExecutionContext(), this, request, script_state, signal,
      activate_after);
  // 12. Append deferredRecord to request’s client’s fetch group’s deferred
  // fetch records.
  deferred_loaders_.insert(deferred_loader);

  deferred_loader->Start(exception_state);
  return deferred_loader->fetch_later_result();
}

void FetchManager::ContextDestroyed() {
  // https://whatpr.org/fetch/1647/9ca4bda...7bff4de.html#concept-defer=fetch-record
  // When a fetch group fetchGroup is terminated:
  // 1. For each fetch record of fetchGroup's fetch records, if record's
  // controller is non-null and record’s done flag is unset and keepalive is
  // false, terminate the fetch record’s controller .
  for (auto& loader : loaders_) {
    loader->Dispose();
  }
}

void FetchManager::OnLoaderFinished(Loader* loader) {
  loaders_.erase(loader);
  loader->Dispose();
}

void FetchManager::Trace(Visitor* visitor) const {
  visitor->Trace(loaders_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

FetchLaterManager::FetchLaterManager(ExecutionContext* ec)
    : ExecutionContextLifecycleObserver(ec),
      permission_observer_receiver_(this, ec) {
  // TODO(crbug.com/1356128): FetchLater API is only supported in Document.
  // Supporting it in workers is blocked by keepalive in browser migration.
  CHECK(ec->IsWindow());

  if (IsFetchLaterUseBackgroundSyncPermissionEnabled()) {
    auto* permission_service =
        DomWindow()->document()->GetPermissionService(ec);
    CHECK(permission_service);

    mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
    permission_observer_receiver_.Bind(
        observer.InitWithNewPipeAndPassReceiver(),
        // Same as `permission_service`'s task type.
        ec->GetTaskRunner(TaskType::kPermission));
    CHECK(permission_observer_receiver_.is_bound());
    // Registers an observer for BackgroundSync permission.
    // Cannot use `HasPermission()` as it's asynchronous. At the time the
    // permission status is needed, e.g. on entering BackForwardCache, it may
    // not have enough time to wait for response.
    auto descriptor = mojom::blink::PermissionDescriptor::New();
    descriptor->name = mojom::blink::PermissionName::BACKGROUND_SYNC;
    permission_service->AddPermissionObserver(std::move(descriptor),
                                              background_sync_permission_,
                                              std::move(observer));
  }
}

blink::ChildURLLoaderFactoryBundle* FetchLaterManager::GetFactory() {
  // Do nothing if context is detached.
  if (!DomWindow()) {
    return nullptr;
  }
  return DomWindow()->GetFrame()->Client()->GetLoaderFactoryBundle();
}

void FetchLaterManager::ContextDestroyed() {
  // https://whatpr.org/fetch/1647/9ca4bda...7bff4de.html#concept-defer=fetch-record
  // When a fetch group fetchGroup is terminated:
  // 2. process deferred fetches for fetchGroup.
  // https://whatpr.org/fetch/1647/9ca4bda...9994c1d.html#process-deferred-fetches
  // To process deferred fetches given a fetch group fetchGroup:
  for (auto& deferred_loader : deferred_loaders_) {
    // 3. For each deferred fetch record deferredRecord, process a deferred
    // fetch given deferredRecord.
    deferred_loader->Process(FetchLaterRendererMetricType::kContextDestroyed);
    deferred_loader->Dispose();
  }
  // Unlike regular Fetch loaders, FetchLater loaders should be cleared
  // immediately when the context is gone, as there is no work left here.
  deferred_loaders_.clear();
}

void FetchLaterManager::ContextEnteredBackForwardCache() {
  // TODO(crbug.com/1465781): Replace with spec once it's finalized.
  // https://github.com/WICG/pending-beacon/issues/3#issuecomment-1286397825
  // Sending any requests "after" the context goes into BackForwardCache
  // requires BackgroundSync permission. If not granted, we should force sending
  // all of them now instead of waiting until `ContextDestroyed()`.
  if (IsFetchLaterSendOnEnterBackForwardCacheEnabled() ||
      (IsFetchLaterUseBackgroundSyncPermissionEnabled() &&
       !IsBackgroundSyncGranted())) {
    for (auto& deferred_loader : deferred_loaders_) {
      deferred_loader->Process(
          FetchLaterRendererMetricType::kActivatedOnEnteredBackForwardCache);
      deferred_loader->Dispose();
    }
    deferred_loaders_.clear();
  }
}

void FetchLaterManager::OnDeferredLoaderFinished(
    DeferredLoader* deferred_loader) {
  deferred_loaders_.erase(deferred_loader);
  deferred_loader->Dispose();
}

bool FetchLaterManager::IsBackgroundSyncGranted() const {
  return background_sync_permission_ == mojom::blink::PermissionStatus::GRANTED;
}

void FetchLaterManager::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  background_sync_permission_ = status;
}

size_t FetchLaterManager::NumLoadersForTesting() const {
  return deferred_loaders_.size();
}

void FetchLaterManager::RecreateTimerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  for (auto& deferred_loader : deferred_loaders_) {
    deferred_loader->RecreateTimerForTesting(task_runner, tick_clock);
  }
}

// static
ResourceLoadPriority FetchLaterManager::ComputeLoadPriorityForTesting(
    const FetchParameters& params) {
  return ComputeFetchLaterLoadPriority(params);
}

std::unique_ptr<network::ResourceRequest>
FetchLaterManager::PrepareNetworkRequest(
    ResourceRequest request,
    const ResourceLoaderOptions& options) const {
  if (!GetExecutionContext()) {
    // No requests if the context is destroyed.
    return nullptr;
  }
  CHECK(DomWindow());
  ResourceFetcher* fetcher = DomWindow()->Fetcher();
  CHECK(fetcher);

  FetchParameters params(std::move(request), options);
  WebScopedVirtualTimePauser unused_virtual_time_pauser;
  params.OverrideContentType(kFetchLaterContentType);
  const FetchClientSettingsObject& fetch_client_settings_object =
      fetcher->GetProperties().GetFetchClientSettingsObject();

  FetchManagerResourceRequestContext resource_request_context;
  if (!RuntimeEnabledFeatures::
          MinimimalResourceRequestPrepBeforeCacheLookupEnabled()) {
    if (PrepareResourceRequest(
            kFetchLaterResourceType, fetch_client_settings_object, params,
            fetcher->Context(), unused_virtual_time_pauser,
            resource_request_context, KURL()) != std::nullopt) {
      return nullptr;
    }
  } else {
    if (PrepareResourceRequestForCacheAccess(
            kFetchLaterResourceType, fetch_client_settings_object, KURL(),
            resource_request_context, fetcher->Context(),
            params) != std::nullopt) {
      return nullptr;
    }
    UpgradeResourceRequestForLoaderNew(
        kFetchLaterResourceType, params, fetcher->Context(),
        resource_request_context, unused_virtual_time_pauser);
  }

  // From `ResourceFetcher::StartLoad()`:
  ScriptForbiddenScope script_forbidden_scope;
  auto network_resource_request = std::make_unique<network::ResourceRequest>();
  PopulateResourceRequest(
      params.GetResourceRequest(),
      std::move(params.MutableResourceRequest().MutableBody()),
      network_resource_request.get());
  return network_resource_request;
}

void FetchLaterManager::Trace(Visitor* visitor) const {
  visitor->Trace(deferred_loaders_);
  visitor->Trace(permission_observer_receiver_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
