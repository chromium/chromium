// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/place_holder_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using network::mojom::CredentialsMode;
using network::mojom::FetchResponseType;
using network::mojom::RedirectMode;
using network::mojom::RequestMode;

namespace blink {

namespace {

bool HasNonEmptyLocationHeader(const FetchHeaderList* headers) {
  String value;
  if (!headers->Get(http_names::kLocation, value))
    return false;
  return !value.IsEmpty();
}

// FailedReason enumerates reasons a fetch can return "TypeError: Failed to
// fetch". This is a temporary measure for debugging a surprisingly high
// incidence of "TypeError: Failed to fetch" when executing Trust Tokens
// issuance operations (crbug.com/1128174).
//
// Since these values are persisted to histograms, please do not remove or
// renumber entries.
//
// TODO(crbug.com/1133944): Once the investigation of Trust Tokens failures has
// ended, remove this enum and the associated logging logic.
enum class FailedReason {
  kRedirectToDataUrlWithImpermissibleFetchMode = 0,
  kContentSecurityPolicyViolation = 1,
  kSameOriginModeButUrlNotSameOrigin = 2,
  kModeIsNoCorsButRedirectModeIsNotFollow = 3,
  kCorsRequestToUrlWithUnsupportedScheme = 4,
  kSchemeFetchToUrlWithUnsupportedScheme = 5,
  kSubresourceIntegrityVerificationError = 6,
  kFailedRedirectCheck = 7,
  kTrustTokensError = 8,
  // The fetch call failed due to a reason other than any of the above, and the
  // response was not blocked. (If it was blocked, the histogram will contain a
  // ResourceRequestBlockedReason value.)
  kOtherNonBlockReason = 9,

  // The following correspond to the values of ResourceRequestBlockedReason:
  kBlockedBecauseOther = 10,
  kBlockedBecauseCSP = 11,
  kBlockedBecauseMixedContent = 12,
  kBlockedBecauseOrigin = 13,
  kBlockedBecauseInspector = 14,
  kBlockedBecauseSubresourceFilter = 15,
  kBlockedBecauseContentType = 16,
  kBlockedBecauseCollapsedByClient = 17,
  kBlockedBecauseCoepFrameResourceNeedsCoepHeader = 18,
  kBlockedBecauseCoopSandboxedIFrameCannotNavigateToCoopPage = 19,
  kBlockedBecauseCorpNotSameOrigin = 20,
  kBlockedBecauseCorpNotSameOriginAfterDefaultedToSameOriginByCoep = 21,
  kBlockedBecauseCorpNotSameSite = 22,
  kBlockedBecauseBlockedByExtensionCrbug1128174Investigation = 23,

  kMaxValue = kBlockedBecauseBlockedByExtensionCrbug1128174Investigation,
};

FailedReason ResourceRequestBlockedReasonToFailedReason(
    ResourceRequestBlockedReason blocked_reason) {
  switch (blocked_reason) {
    case ResourceRequestBlockedReason::kOther:
      return FailedReason::kBlockedBecauseOther;
    case ResourceRequestBlockedReason::kCSP:
      return FailedReason::kBlockedBecauseCSP;
    case ResourceRequestBlockedReason::kMixedContent:
      return FailedReason::kBlockedBecauseMixedContent;
    case ResourceRequestBlockedReason::kOrigin:
      return FailedReason::kBlockedBecauseOrigin;
    case ResourceRequestBlockedReason::kInspector:
      return FailedReason::kBlockedBecauseInspector;
    case ResourceRequestBlockedReason::kSubresourceFilter:
      return FailedReason::kBlockedBecauseSubresourceFilter;
    case ResourceRequestBlockedReason::kContentType:
      return FailedReason::kBlockedBecauseContentType;
    case ResourceRequestBlockedReason::kCollapsedByClient:
      return FailedReason::kBlockedBecauseCollapsedByClient;
    case ResourceRequestBlockedReason::kCoepFrameResourceNeedsCoepHeader:
      return FailedReason::kBlockedBecauseCoepFrameResourceNeedsCoepHeader;
    case ResourceRequestBlockedReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return FailedReason::
          kBlockedBecauseCoopSandboxedIFrameCannotNavigateToCoopPage;
    case ResourceRequestBlockedReason::kCorpNotSameOrigin:
      return FailedReason::kBlockedBecauseCorpNotSameOrigin;
    case ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return FailedReason::
          kBlockedBecauseCorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case ResourceRequestBlockedReason::kCorpNotSameSite:
      return FailedReason::kBlockedBecauseCorpNotSameSite;
    case ResourceRequestBlockedReason::
        kBlockedByExtensionCrbug1128174Investigation:
      return FailedReason::
          kBlockedBecauseBlockedByExtensionCrbug1128174Investigation;
  }
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

// Logs a more descriptive reason why a fetch with Trust Tokens parameters
// failed. This is a temporary measure for debugging a surprisingly high
// incidence of "TypeError: Failed to fetch" when executing Trust Tokens
// issuance operations (crbug.com/1128174).
void HistogramFetchFailureReasonForTrustTokensOperation(
    network::mojom::blink::TrustTokenOperationType operation_type,
    FailedReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Net.TrustTokens.FetchFailedReason", ".",
                    SerializeTrustTokenOperationType(operation_type)}),
      reason);
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

}  // namespace

class FetchManager::Loader final
    : public GarbageCollected<FetchManager::Loader>,
      public ThreadableLoaderClient {
 public:
  Loader(ExecutionContext*,
         FetchManager*,
         ScriptPromiseResolver*,
         FetchRequestData*,
         scoped_refptr<const DOMWrapperWorld>,
         AbortSignal*);
  ~Loader() override;
  void Trace(Visitor*) const override;

  // ThreadableLoaderClient implementation.
  bool WillFollowRedirect(const KURL&, const ResourceResponse&) override;
  void DidReceiveResponse(uint64_t, const ResourceResponse&) override;
  void DidStartLoadingResponseBody(BytesConsumer&) override;
  void DidFinishLoading(uint64_t) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void Start();
  void Dispose();
  void Abort();

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
          response_type_(response_type),
          finished_(false) {
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
          buffer_.Append(buffer, SafeCast<wtf_size_t>(available));
          result = body_->EndRead(available);
        }
        if (result == Result::kShouldWait)
          return;
      }

      finished_ = true;
      if (result == Result::kDone) {
        SubresourceIntegrity::ReportInfo report_info;
        bool check_result = true;
        if (response_type_ != FetchResponseType::kBasic &&
            response_type_ != FetchResponseType::kCors &&
            response_type_ != FetchResponseType::kDefault) {
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
              buffer_.data(), buffer_.size(), url_, report_info);
        }
        SubresourceIntegrityHelper::DoReport(*loader_->GetExecutionContext(),
                                             report_info);
        if (check_result) {
          updater_->Update(MakeGarbageCollected<FormDataBytesConsumer>(
              buffer_.data(), buffer_.size()));
          loader_->resolver_->Resolve(response_);
          loader_->resolver_.Clear();
          return;
        }
      }
      String error_message =
          "Unknown error occurred while trying to verify integrity.";
      updater_->Update(
          BytesConsumer::CreateErrored(BytesConsumer::Error(error_message)));
      loader_->PerformNetworkError(
          error_message, FailedReason::kSubresourceIntegrityVerificationError);
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
    Vector<char> buffer_;
    bool finished_;
  };

 private:
  void PerformSchemeFetch();
  void PerformNetworkError(const String& message, FailedReason reason);
  void PerformHTTPFetch();
  void PerformDataFetch();
  // If |dom_exception| is provided, throws the specified DOMException instead
  // of the usual "Failed to fetch" TypeError.
  void Failed(const String& message,
              DOMException* dom_exception,
              FailedReason reason);
  void NotifyFinished();
  ExecutionContext* GetExecutionContext() { return execution_context_; }

  Member<FetchManager> fetch_manager_;
  Member<ScriptPromiseResolver> resolver_;
  Member<FetchRequestData> fetch_request_data_;
  Member<ThreadableLoader> threadable_loader_;
  Member<PlaceHolderBytesConsumer> place_holder_body_;
  bool failed_;
  bool finished_;
  int response_http_status_code_;
  bool response_has_no_store_header_ = false;
  Member<SRIVerifier> integrity_verifier_;
  scoped_refptr<const DOMWrapperWorld> world_;
  Member<AbortSignal> signal_;
  Vector<KURL> url_list_;
  Member<ExecutionContext> execution_context_;
};

FetchManager::Loader::Loader(ExecutionContext* execution_context,
                             FetchManager* fetch_manager,
                             ScriptPromiseResolver* resolver,
                             FetchRequestData* fetch_request_data,
                             scoped_refptr<const DOMWrapperWorld> world,
                             AbortSignal* signal)
    : fetch_manager_(fetch_manager),
      resolver_(resolver),
      fetch_request_data_(fetch_request_data),
      failed_(false),
      finished_(false),
      response_http_status_code_(0),
      integrity_verifier_(nullptr),
      world_(std::move(world)),
      signal_(signal),
      execution_context_(execution_context) {
  DCHECK(world_);
  url_list_.push_back(fetch_request_data->Url());
}

FetchManager::Loader::~Loader() {
  DCHECK(!threadable_loader_);
}

void FetchManager::Loader::Trace(Visitor* visitor) const {
  visitor->Trace(fetch_manager_);
  visitor->Trace(resolver_);
  visitor->Trace(fetch_request_data_);
  visitor->Trace(threadable_loader_);
  visitor->Trace(place_holder_body_);
  visitor->Trace(integrity_verifier_);
  visitor->Trace(signal_);
  visitor->Trace(execution_context_);
  ThreadableLoaderClient::Trace(visitor);
}

bool FetchManager::Loader::WillFollowRedirect(
    const KURL& url,
    const ResourceResponse& response) {
  const auto redirect_mode = fetch_request_data_->Redirect();
  if (redirect_mode == network::mojom::RedirectMode::kError) {
    DidFailRedirectCheck();
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

  ScriptState* script_state = resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);

  response_http_status_code_ = response.HttpStatusCode();
  FetchRequestData::Tainting tainting = fetch_request_data_->ResponseTainting();

  if (response.CurrentRequestUrl().ProtocolIsData()) {
    if (fetch_request_data_->Url() == response.CurrentRequestUrl()) {
      // A direct request to data.
      tainting = FetchRequestData::kBasicTainting;
    } else {
      // A redirect to data: scheme occured.
      // Redirects to data URLs are rejected by the spec because
      // same-origin data-URL flag is unset, except for no-cors mode.
      // TODO(hiroshige): currently redirects to data URLs in no-cors
      // mode is also rejected by Chromium side.
      switch (fetch_request_data_->Mode()) {
        case RequestMode::kNoCors:
          tainting = FetchRequestData::kOpaqueTainting;
          break;
        case RequestMode::kSameOrigin:
        case RequestMode::kCors:
        case RequestMode::kCorsWithForcedPreflight:
        case RequestMode::kNavigate:
          PerformNetworkError(
              "Fetch API cannot load " +
                  fetch_request_data_->Url().GetString() +
                  ". Redirects to data: URL are allowed only when "
                  "mode is \"no-cors\".",
              FailedReason::kRedirectToDataUrlWithImpermissibleFetchMode);
          return;
      }
    }
  } else if (!fetch_request_data_->Origin()->CanReadContent(
                 response.CurrentRequestUrl())) {
    // Recompute the tainting if the request was redirected to a different
    // origin.
    switch (fetch_request_data_->Mode()) {
      case RequestMode::kSameOrigin:
        NOTREACHED();
        break;
      case RequestMode::kNoCors:
        tainting = FetchRequestData::kOpaqueTainting;
        break;
      case RequestMode::kCors:
      case RequestMode::kCorsWithForcedPreflight:
        tainting = FetchRequestData::kCorsTainting;
        break;
      case RequestMode::kNavigate:
        LOG(FATAL);
        break;
    }
  }
  if (response.WasFetchedViaServiceWorker()) {
    switch (response.GetType()) {
      case FetchResponseType::kBasic:
      case FetchResponseType::kDefault:
        tainting = FetchRequestData::kBasicTainting;
        break;
      case FetchResponseType::kCors:
        tainting = FetchRequestData::kCorsTainting;
        break;
      case FetchResponseType::kOpaque:
        tainting = FetchRequestData::kOpaqueTainting;
        break;
      case FetchResponseType::kOpaqueRedirect:
        DCHECK(
            network_utils::IsRedirectResponseCode(response_http_status_code_));
        break;  // The code below creates an opaque-redirect filtered response.
      case FetchResponseType::kError:
        LOG(FATAL) << "When ServiceWorker respond to the request from fetch() "
                      "with an error response, FetchManager::Loader::didFail() "
                      "must be called instead.";
        break;
    }
  }

  // TODO(crbug.com/1072350): Remove this once enough data is collected.
  if (fetch_request_data_->Method() != http_names::kGET &&
      fetch_request_data_->Method() != http_names::kHEAD &&
      fetch_request_data_->Mode() == network::mojom::RequestMode::kNoCors &&
      tainting == FetchRequestData::kOpaqueTainting) {
    UseCounter::Count(execution_context_,
                      WebFeature::kFetchAPINonGetOrHeadOpaqueResponse);
    if (url_list_.size() > 1) {
      UseCounter::Count(
          execution_context_,
          WebFeature::kFetchAPINonGetOrHeadOpaqueResponseWithRedirect);
    }
  }

  place_holder_body_ = MakeGarbageCollected<PlaceHolderBytesConsumer>();
  FetchResponseData* response_data = FetchResponseData::CreateWithBuffer(
      BodyStreamBuffer::Create(script_state, place_holder_body_, signal_));

  response_data->InitFromResourceResponse(
      url_list_, fetch_request_data_->Method(),
      fetch_request_data_->Credentials(), tainting, response);

  FetchResponseData* tainted_response = nullptr;

  DCHECK(!(network_utils::IsRedirectResponseCode(response_http_status_code_) &&
           HasNonEmptyLocationHeader(response_data->HeaderList()) &&
           fetch_request_data_->Redirect() != RedirectMode::kManual));

  if (network_utils::IsRedirectResponseCode(response_http_status_code_) &&
      fetch_request_data_->Redirect() == RedirectMode::kManual) {
    tainted_response = response_data->CreateOpaqueRedirectFilteredResponse();
  } else {
    switch (tainting) {
      case FetchRequestData::kBasicTainting:
        tainted_response = response_data->CreateBasicFilteredResponse();
        break;
      case FetchRequestData::kCorsTainting: {
        HTTPHeaderSet header_names = cors::ExtractCorsExposedHeaderNamesList(
            fetch_request_data_->Credentials(), response);
        tainted_response =
            response_data->CreateCorsFilteredResponse(header_names);
        break;
      }
      case FetchRequestData::kOpaqueTainting:
        tainted_response = response_data->CreateOpaqueFilteredResponse();
        break;
    }
  }

  response_has_no_store_header_ = response.CacheControlContainsNoStore();

  Response* r =
      Response::Create(resolver_->GetExecutionContext(), tainted_response);
  r->headers()->SetGuard(Headers::kImmutableGuard);
  if (fetch_request_data_->Integrity().IsEmpty()) {
    resolver_->Resolve(r);
    resolver_.Clear();
  } else {
    DCHECK(!integrity_verifier_);
    // We have another place holder body for SRI.
    PlaceHolderBytesConsumer* verified = place_holder_body_;
    place_holder_body_ = MakeGarbageCollected<PlaceHolderBytesConsumer>();
    BytesConsumer* underlying = place_holder_body_;

    integrity_verifier_ = MakeGarbageCollected<SRIVerifier>(
        underlying, verified, r, this, fetch_request_data_->Integrity(),
        response.CurrentRequestUrl(), r->GetResponse()->GetType());
  }
}

void FetchManager::Loader::DidStartLoadingResponseBody(BytesConsumer& body) {
  if (fetch_request_data_->Integrity().IsEmpty() &&
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

  auto* window = DynamicTo<LocalDOMWindow>(execution_context_.Get());
  if (window && window->GetFrame() &&
      cors::IsOkStatus(response_http_status_code_)) {
    window->GetFrame()->GetPage()->GetChromeClient().AjaxSucceeded(
        window->GetFrame());
  }
  NotifyFinished();
}

void FetchManager::Loader::DidFail(const ResourceError& error) {
  if (fetch_request_data_ && fetch_request_data_->TrustTokenParams()) {
    HistogramNetErrorForTrustTokensOperation(
        fetch_request_data_->TrustTokenParams()->type, error.ErrorCode());
  }

  if (error.TrustTokenOperationError() !=
      network::mojom::blink::TrustTokenOperationStatus::kOk) {
    Failed(String(),
           TrustTokenErrorToDOMException(error.TrustTokenOperationError()),
           FailedReason::kTrustTokensError);
    return;
  }

  if (base::Optional<ResourceRequestBlockedReason> blocked_reason =
          error.GetResourceRequestBlockedReason()) {
    Failed(String(), nullptr,
           ResourceRequestBlockedReasonToFailedReason(*blocked_reason));
    return;
  }

  Failed(String(), nullptr, FailedReason::kOtherNonBlockReason);
}

void FetchManager::Loader::DidFailRedirectCheck() {
  Failed(String(), nullptr, FailedReason::kFailedRedirectCheck);
}

void FetchManager::Loader::Start() {
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
  if (!execution_context_->GetContentSecurityPolicyForWorld(world_.get())
           ->AllowConnectToSource(fetch_request_data_->Url(),
                                  fetch_request_data_->Url(),
                                  RedirectStatus::kNoRedirect)) {
    // "A network error."
    PerformNetworkError(
        "Refused to connect to '" + fetch_request_data_->Url().ElidedString() +
            "' because it violates the document's Content Security Policy.",
        FailedReason::kContentSecurityPolicyViolation);
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
    PerformSchemeFetch();
    return;
  }

  // "- |request|'s mode is |same-origin|"
  if (fetch_request_data_->Mode() == RequestMode::kSameOrigin) {
    // "A network error."
    PerformNetworkError("Fetch API cannot load " +
                            fetch_request_data_->Url().GetString() +
                            ". Request mode is \"same-origin\" but the URL\'s "
                            "origin is not same as the request origin " +
                            fetch_request_data_->Origin()->ToString() + ".",
                        FailedReason::kSameOriginModeButUrlNotSameOrigin);
    return;
  }

  // "- |request|'s mode is |no CORS|"
  if (fetch_request_data_->Mode() == RequestMode::kNoCors) {
    // "If |request|'s redirect mode is not |follow|, then return a network
    // error.
    if (fetch_request_data_->Redirect() != RedirectMode::kFollow) {
      PerformNetworkError(
          "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
              ". Request mode is \"no-cors\" but the redirect mode "
              "is not \"follow\".",
          FailedReason::kModeIsNoCorsButRedirectModeIsNotFollow);
      return;
    }

    // "Set |request|'s response tainting to |opaque|."
    fetch_request_data_->SetResponseTainting(FetchRequestData::kOpaqueTainting);
    // "The result of performing a scheme fetch using |request|."
    PerformSchemeFetch();
    return;
  }

  // "- |request|'s url's scheme is not one of 'http' and 'https'"
  // This may include other HTTP-like schemes if the embedder has added them
  // to SchemeRegistry::registerURLSchemeAsSupportingFetchAPI.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          fetch_request_data_->Url().Protocol())) {
    // "A network error."
    PerformNetworkError(
        "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
            ". URL scheme must be \"http\" or \"https\" for CORS request.",
        FailedReason::kCorsRequestToUrlWithUnsupportedScheme);
    return;
  }

  // "Set |request|'s response tainting to |CORS|."
  fetch_request_data_->SetResponseTainting(FetchRequestData::kCorsTainting);

  // "The result of performing an HTTP fetch using |request| with the
  // |CORS flag| set."
  PerformHTTPFetch();
}

void FetchManager::Loader::Dispose() {
  // Prevent notification
  fetch_manager_ = nullptr;
  if (threadable_loader_) {
    if (fetch_request_data_->Keepalive() &&
        !base::FeatureList::IsEnabled(
            network::features::kDisableKeepaliveFetch)) {
      threadable_loader_->Detach();
    } else {
      threadable_loader_->Cancel();
    }
    threadable_loader_ = nullptr;
  }
  if (integrity_verifier_)
    integrity_verifier_->Cancel();
  execution_context_ = nullptr;
}

void FetchManager::Loader::Abort() {
  if (resolver_) {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
    resolver_.Clear();
  }
  if (threadable_loader_) {
    // Prevent re-entrancy.
    auto loader = threadable_loader_;
    threadable_loader_ = nullptr;
    loader->Cancel();
  }
  NotifyFinished();
}

void FetchManager::Loader::PerformSchemeFetch() {
  // "To perform a scheme fetch using |request|, switch on |request|'s url's
  // scheme, and run the associated steps:"
  if (SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          fetch_request_data_->Url().Protocol()) ||
      fetch_request_data_->Url().ProtocolIs("blob")) {
    // "Return the result of performing an HTTP fetch using |request|."
    PerformHTTPFetch();
  } else if (fetch_request_data_->Url().ProtocolIsData()) {
    PerformDataFetch();
  } else {
    // FIXME: implement other protocols.
    PerformNetworkError(
        "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
            ". URL scheme \"" + fetch_request_data_->Url().Protocol() +
            "\" is not supported.",
        FailedReason::kSchemeFetchToUrlWithUnsupportedScheme);
  }
}

void FetchManager::Loader::PerformNetworkError(const String& message,
                                               FailedReason reason) {
  Failed(message, nullptr, reason);
}

void FetchManager::Loader::PerformHTTPFetch() {
  // CORS preflight fetch procedure is implemented inside ThreadableLoader.

  // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
  //  body is a tee of |request|'s body."
  // We use ResourceRequest class for HTTPRequest.
  // FIXME: Support body.
  ResourceRequest request(fetch_request_data_->Url());
  request.SetRequestorOrigin(fetch_request_data_->Origin());
  request.SetIsolatedWorldOrigin(fetch_request_data_->IsolatedWorldOrigin());
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetRequestDestination(fetch_request_data_->Destination());
  request.SetHttpMethod(fetch_request_data_->Method());
  request.SetFetchWindowId(fetch_request_data_->WindowId());
  request.SetTrustTokenParams(fetch_request_data_->TrustTokenParams());

  switch (fetch_request_data_->Mode()) {
    case RequestMode::kSameOrigin:
    case RequestMode::kNoCors:
    case RequestMode::kCors:
    case RequestMode::kCorsWithForcedPreflight:
      request.SetMode(fetch_request_data_->Mode());
      break;
    case RequestMode::kNavigate:
      // NetworkService (i.e. CorsURLLoaderFactory::IsSane) rejects kNavigate
      // requests coming from renderers, so using kSameOrigin here.
      // TODO(lukasza): Tweak CorsURLLoaderFactory::IsSane to accept kNavigate
      // if request_initiator and the target are same-origin.
      request.SetMode(RequestMode::kSameOrigin);
      break;
  }

  request.SetCredentialsMode(fetch_request_data_->Credentials());
  for (const auto& header : fetch_request_data_->HeaderList()->List()) {
    request.AddHttpHeaderField(AtomicString(header.first),
                               AtomicString(header.second));
  }

  if (fetch_request_data_->Method() != http_names::kGET &&
      fetch_request_data_->Method() != http_names::kHEAD) {
    if (fetch_request_data_->Buffer()) {
      scoped_refptr<EncodedFormData> form_data =
          fetch_request_data_->Buffer()->DrainAsFormData();
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
            resolver_->GetScriptState(),
            pending_remote.InitWithNewPipeAndPassReceiver());
        request.MutableBody().SetStreamBody(std::move(pending_remote));
        request.SetAllowHTTP1ForStreamingUpload(
            fetch_request_data_->AllowHTTP1ForStreamingUpload());
      }
    }
  }
  request.SetCacheMode(fetch_request_data_->CacheMode());
  request.SetRedirectMode(fetch_request_data_->Redirect());
  request.SetFetchImportanceMode(fetch_request_data_->Importance());
  request.SetPriority(fetch_request_data_->Priority());
  request.SetUseStreamOnResponse(true);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context_->AddressSpace());
  request.SetReferrerString(fetch_request_data_->ReferrerString());
  request.SetReferrerPolicy(fetch_request_data_->GetReferrerPolicy());

  request.SetSkipServiceWorker(world_->IsIsolatedWorld());

  if (fetch_request_data_->Keepalive()) {
    request.SetKeepalive(true);
    UseCounter::Count(execution_context_, mojom::WebFeature::kFetchKeepalive);
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

  threadable_loader_ = MakeGarbageCollected<ThreadableLoader>(
      *execution_context_, this, resource_loader_options);
  threadable_loader_->Start(std::move(request));
}

// performDataFetch() is almost the same as performHTTPFetch(), except for:
// - We set AllowCrossOriginRequests to allow requests to data: URLs in
//   'same-origin' mode.
// - We reject non-GET method.
void FetchManager::Loader::PerformDataFetch() {
  DCHECK(fetch_request_data_->Url().ProtocolIsData());

  ResourceRequest request(fetch_request_data_->Url());
  request.SetRequestorOrigin(fetch_request_data_->Origin());
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetRequestDestination(fetch_request_data_->Destination());
  request.SetUseStreamOnResponse(true);
  request.SetHttpMethod(fetch_request_data_->Method());
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  request.SetRedirectMode(RedirectMode::kError);
  request.SetFetchImportanceMode(fetch_request_data_->Importance());
  request.SetPriority(fetch_request_data_->Priority());
  // We intentionally skip 'setExternalRequestStateFromRequestorAddressSpace',
  // as 'data:' can never be external.

  ResourceLoaderOptions resource_loader_options(world_);
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  threadable_loader_ = MakeGarbageCollected<ThreadableLoader>(
      *execution_context_, this, resource_loader_options);
  threadable_loader_->Start(std::move(request));
}

void FetchManager::Loader::Failed(const String& message,
                                  DOMException* dom_exception,
                                  FailedReason reason) {
  if (failed_ || finished_)
    return;
  failed_ = true;
  if (execution_context_->IsContextDestroyed())
    return;
  if (!message.IsEmpty()) {
    execution_context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError, message));
  }

  if (fetch_request_data_ && fetch_request_data_->TrustTokenParams()) {
    HistogramFetchFailureReasonForTrustTokensOperation(
        fetch_request_data_->TrustTokenParams()->type, reason);
  }

  if (resolver_) {
    ScriptState* state = resolver_->GetScriptState();
    ScriptState::Scope scope(state);
    if (dom_exception) {
      resolver_->Reject(dom_exception);
    } else {
      resolver_->Reject(V8ThrowException::CreateTypeError(state->GetIsolate(),
                                                          "Failed to fetch"));
    }
  }
  NotifyFinished();
}

void FetchManager::Loader::NotifyFinished() {
  if (fetch_manager_)
    fetch_manager_->OnLoaderFinished(this);
}

FetchManager::FetchManager(ExecutionContext* execution_context)
    : ExecutionContextLifecycleObserver(execution_context) {}

ScriptPromise FetchManager::Fetch(ScriptState* script_state,
                                  FetchRequestData* request,
                                  AbortSignal* signal,
                                  ExceptionState& exception_state) {
  DCHECK(signal);
  if (signal->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The user aborted a request.");
    return ScriptPromise();
  }

  request->SetDestination(network::mojom::RequestDestination::kEmpty);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* loader =
      MakeGarbageCollected<Loader>(GetExecutionContext(), this, resolver,
                                   request, &script_state->World(), signal);
  loaders_.insert(loader);
  signal->AddAlgorithm(WTF::Bind(&Loader::Abort, WrapWeakPersistent(loader)));
  // TODO(ricea): Reject the Response body with AbortError, not TypeError.
  loader->Start();
  return promise;
}

void FetchManager::ContextDestroyed() {
  for (auto& loader : loaders_)
    loader->Dispose();
}

void FetchManager::OnLoaderFinished(Loader* loader) {
  loaders_.erase(loader);
  loader->Dispose();
}

void FetchManager::Trace(Visitor* visitor) const {
  visitor->Trace(loaders_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
