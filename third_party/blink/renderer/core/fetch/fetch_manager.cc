// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/web_cors.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_for_data_consumer_handle.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/fetch/response_init.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
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
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using network::mojom::FetchRedirectMode;
using network::mojom::FetchRequestMode;
using network::mojom::FetchResponseType;

namespace blink {

namespace {

class EmptyDataHandle final : public WebDataConsumerHandle {
 private:
  class EmptyDataReader final : public WebDataConsumerHandle::Reader {
   public:
    explicit EmptyDataReader(
        WebDataConsumerHandle::Client* client,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
        : factory_(this) {
      task_runner->PostTask(
          FROM_HERE, WTF::Bind(&EmptyDataReader::Notify, factory_.GetWeakPtr(),
                               WTF::Unretained(client)));
    }

   private:
    Result BeginRead(const void** buffer,
                     WebDataConsumerHandle::Flags,
                     size_t* available) override {
      *available = 0;
      *buffer = nullptr;
      return kDone;
    }
    Result EndRead(size_t) override {
      return WebDataConsumerHandle::kUnexpectedError;
    }
    void Notify(WebDataConsumerHandle::Client* client) {
      client->DidGetReadable();
    }
    base::WeakPtrFactory<EmptyDataReader> factory_;
  };

  std::unique_ptr<Reader> ObtainReader(
      Client* client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    return std::make_unique<EmptyDataReader>(client, std::move(task_runner));
  }
  const char* DebugName() const override { return "EmptyDataHandle"; }
};

bool HasNonEmptyLocationHeader(const FetchHeaderList* headers) {
  String value;
  if (!headers->Get(HTTPNames::Location, value))
    return false;
  return !value.IsEmpty();
}

class SRIBytesConsumer final : public BytesConsumer {
 public:
  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    if (!underlying_) {
      *buffer = nullptr;
      *available = 0;
      return is_cancelled_ ? Result::kDone : Result::kShouldWait;
    }
    return underlying_->BeginRead(buffer, available);
  }
  Result EndRead(size_t read_size) override {
    DCHECK(underlying_);
    return underlying_->EndRead(read_size);
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    return underlying_ ? underlying_->DrainAsBlobDataHandle(policy) : nullptr;
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    return underlying_ ? underlying_->DrainAsFormData() : nullptr;
  }
  void SetClient(BytesConsumer::Client* client) override {
    DCHECK(!client_);
    DCHECK(client);
    if (underlying_)
      underlying_->SetClient(client);
    else
      client_ = client;
  }
  void ClearClient() override {
    if (underlying_)
      underlying_->ClearClient();
    else
      client_ = nullptr;
  }
  void Cancel() override {
    if (underlying_) {
      underlying_->Cancel();
    } else {
      is_cancelled_ = true;
      client_ = nullptr;
    }
  }
  PublicState GetPublicState() const override {
    return underlying_ ? underlying_->GetPublicState()
                       : is_cancelled_ ? PublicState::kClosed
                                       : PublicState::kReadableOrWaiting;
  }
  Error GetError() const override {
    DCHECK(underlying_);
    // We must not be in the errored state until we get updated.
    return underlying_->GetError();
  }
  String DebugName() const override { return "SRIBytesConsumer"; }

  // This function can be called at most once.
  void Update(BytesConsumer* consumer) {
    DCHECK(!underlying_);
    if (is_cancelled_) {
      // This consumer has already been closed.
      return;
    }

    underlying_ = consumer;
    if (client_) {
      Client* client = client_;
      client_ = nullptr;
      underlying_->SetClient(client);
      if (GetPublicState() != PublicState::kReadableOrWaiting)
        client->OnStateChange();
    }
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(underlying_);
    visitor->Trace(client_);
    BytesConsumer::Trace(visitor);
  }

 private:
  TraceWrapperMember<BytesConsumer> underlying_;
  Member<Client> client_;
  bool is_cancelled_ = false;
};

}  // namespace

class FetchManager::Loader final
    : public GarbageCollectedFinalized<FetchManager::Loader>,
      public ThreadableLoaderClient {
  USING_PRE_FINALIZER(FetchManager::Loader, Dispose);

 public:
  static Loader* Create(ExecutionContext* execution_context,
                        FetchManager* fetch_manager,
                        ScriptPromiseResolver* resolver,
                        FetchRequestData* request,
                        bool is_isolated_world,
                        AbortSignal* signal) {
    return new Loader(execution_context, fetch_manager, resolver, request,
                      is_isolated_world, signal);
  }

  ~Loader() override;
  virtual void Trace(blink::Visitor*);

  bool WillFollowRedirect(const KURL&, const ResourceResponse&) override;
  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidFinishLoading(unsigned long) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void Start(ExceptionState&);
  void Dispose();
  void Abort();

  class SRIVerifier final : public GarbageCollectedFinalized<SRIVerifier>,
                            public WebDataConsumerHandle::Client {
   public:
    // Promptly clear m_handle and m_reader.
    EAGERLY_FINALIZE();
    // SRIVerifier takes ownership of |handle| and |response|.
    // |updater| must be garbage collected. The other arguments
    // all must have the lifetime of the give loader.
    SRIVerifier(std::unique_ptr<WebDataConsumerHandle> handle,
                SRIBytesConsumer* updater,
                Response* response,
                FetchManager::Loader* loader,
                String integrity_metadata,
                const KURL& url,
                FetchResponseType response_type,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner)
        : handle_(std::move(handle)),
          updater_(updater),
          response_(response),
          loader_(loader),
          integrity_metadata_(integrity_metadata),
          url_(url),
          response_type_(response_type),
          finished_(false) {
      reader_ = handle_->ObtainReader(this, std::move(task_runner));
    }

    void Cancel() {
      reader_ = nullptr;
      handle_ = nullptr;
    }

    void DidGetReadable() override {
      DCHECK(reader_);
      DCHECK(loader_);
      DCHECK(response_);

      WebDataConsumerHandle::Result r = WebDataConsumerHandle::kOk;
      while (r == WebDataConsumerHandle::kOk) {
        const void* buffer;
        size_t size;
        r = reader_->BeginRead(&buffer, WebDataConsumerHandle::kFlagNone,
                               &size);
        if (r == WebDataConsumerHandle::kOk) {
          buffer_.Append(static_cast<const char*>(buffer), size);
          reader_->EndRead(size);
        }
      }
      if (r == WebDataConsumerHandle::kShouldWait)
        return;
      String error_message =
          "Unknown error occurred while trying to verify integrity.";
      finished_ = true;
      if (r == WebDataConsumerHandle::kDone) {
        SubresourceIntegrity::ReportInfo report_info;
        bool check_result = true;
        if (response_type_ != FetchResponseType::kBasic &&
            response_type_ != FetchResponseType::kCORS &&
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
          updater_->Update(
              new FormDataBytesConsumer(buffer_.data(), buffer_.size()));
          loader_->resolver_->Resolve(response_);
          loader_->resolver_.Clear();
          // FetchManager::Loader::didFinishLoading() can
          // be called before didGetReadable() is called
          // when the data is ready. In that case,
          // didFinishLoading() doesn't clean up and call
          // notifyFinished(), so it is necessary to
          // explicitly finish the loader here.
          if (loader_->did_finish_loading_)
            loader_->LoadSucceeded();
          return;
        }
      }
      updater_->Update(
          BytesConsumer::CreateErrored(BytesConsumer::Error(error_message)));
      loader_->PerformNetworkError(error_message);
    }

    bool IsFinished() const { return finished_; }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(updater_);
      visitor->Trace(response_);
      visitor->Trace(loader_);
    }

   private:
    std::unique_ptr<WebDataConsumerHandle> handle_;
    Member<SRIBytesConsumer> updater_;
    // We cannot store a Response because its JS wrapper can be collected.
    // TODO(yhirano): Fix this.
    Member<Response> response_;
    Member<FetchManager::Loader> loader_;
    String integrity_metadata_;
    KURL url_;
    const FetchResponseType response_type_;
    std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
    Vector<char> buffer_;
    bool finished_;
  };

 private:
  Loader(ExecutionContext*,
         FetchManager*,
         ScriptPromiseResolver*,
         FetchRequestData*,
         bool is_isolated_world,
         AbortSignal*);

  void PerformSchemeFetch(ExceptionState&);
  void PerformNetworkError(const String& message);
  void PerformHTTPFetch(ExceptionState&);
  void PerformDataFetch();
  void Failed(const String& message);
  void NotifyFinished();
  Document* GetDocument() const;
  ExecutionContext* GetExecutionContext() { return execution_context_; }
  void LoadSucceeded();

  Member<FetchManager> fetch_manager_;
  Member<ScriptPromiseResolver> resolver_;
  Member<FetchRequestData> fetch_request_data_;
  Member<ThreadableLoader> threadable_loader_;
  bool failed_;
  bool finished_;
  int response_http_status_code_;
  Member<SRIVerifier> integrity_verifier_;
  bool did_finish_loading_;
  bool is_isolated_world_;
  Member<AbortSignal> signal_;
  Vector<KURL> url_list_;
  Member<ExecutionContext> execution_context_;
};

FetchManager::Loader::Loader(ExecutionContext* execution_context,
                             FetchManager* fetch_manager,
                             ScriptPromiseResolver* resolver,
                             FetchRequestData* fetch_request_data,
                             bool is_isolated_world,
                             AbortSignal* signal)
    : fetch_manager_(fetch_manager),
      resolver_(resolver),
      fetch_request_data_(fetch_request_data),
      failed_(false),
      finished_(false),
      response_http_status_code_(0),
      integrity_verifier_(nullptr),
      did_finish_loading_(false),
      is_isolated_world_(is_isolated_world),
      signal_(signal),
      execution_context_(execution_context) {
  url_list_.push_back(fetch_request_data->Url());
}

FetchManager::Loader::~Loader() {
  DCHECK(!threadable_loader_);
}

void FetchManager::Loader::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetch_manager_);
  visitor->Trace(resolver_);
  visitor->Trace(fetch_request_data_);
  visitor->Trace(threadable_loader_);
  visitor->Trace(integrity_verifier_);
  visitor->Trace(signal_);
  visitor->Trace(execution_context_);
}

bool FetchManager::Loader::WillFollowRedirect(
    const KURL& url,
    const ResourceResponse& response) {
  const auto redirect_mode = fetch_request_data_->Redirect();
  if (redirect_mode == network::mojom::FetchRedirectMode::kError) {
    DidFailRedirectCheck();
    Dispose();
    return false;
  }

  if (redirect_mode == network::mojom::FetchRedirectMode::kManual) {
    const unsigned long unused = 0;
    // There is no need to read the body of redirect response because there is
    // no way to read the body of opaque-redirect filtered response's internal
    // response.
    // TODO(horo): If we support any API which expose the internal body, we
    // will have to read the body. And also HTTPCache changes will be needed
    // because it doesn't store the body of redirect responses.
    DidReceiveResponse(unused, response, std::make_unique<EmptyDataHandle>());

    if (threadable_loader_)
      NotifyFinished();

    Dispose();
    return false;
  }

  DCHECK_EQ(redirect_mode, network::mojom::FetchRedirectMode::kFollow);
  url_list_.push_back(url);
  return true;
}

void FetchManager::Loader::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(handle);
  // TODO(horo): This check could be false when we will use the response url
  // in service worker responses. (crbug.com/553535)
  DCHECK(response.Url() == url_list_.back());
  ScriptState* script_state = resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);

  response_http_status_code_ = response.HttpStatusCode();
  FetchRequestData::Tainting tainting = fetch_request_data_->ResponseTainting();

  if (response.Url().ProtocolIsData()) {
    if (fetch_request_data_->Url() == response.Url()) {
      // A direct request to data.
      tainting = FetchRequestData::kBasicTainting;
    } else {
      // A redirect to data: scheme occured.
      // Redirects to data URLs are rejected by the spec because
      // same-origin data-URL flag is unset, except for no-cors mode.
      // TODO(hiroshige): currently redirects to data URLs in no-cors
      // mode is also rejected by Chromium side.
      switch (fetch_request_data_->Mode()) {
        case FetchRequestMode::kNoCORS:
          tainting = FetchRequestData::kOpaqueTainting;
          break;
        case FetchRequestMode::kSameOrigin:
        case FetchRequestMode::kCORS:
        case FetchRequestMode::kCORSWithForcedPreflight:
        case FetchRequestMode::kNavigate:
          PerformNetworkError("Fetch API cannot load " +
                              fetch_request_data_->Url().GetString() +
                              ". Redirects to data: URL are allowed only when "
                              "mode is \"no-cors\".");
          return;
      }
    }
  } else if (!SecurityOrigin::Create(response.Url())
                  ->IsSameSchemeHostPort(fetch_request_data_->Origin().get())) {
    // Recompute the tainting if the request was redirected to a different
    // origin.
    switch (fetch_request_data_->Mode()) {
      case FetchRequestMode::kSameOrigin:
        NOTREACHED();
        break;
      case FetchRequestMode::kNoCORS:
        tainting = FetchRequestData::kOpaqueTainting;
        break;
      case FetchRequestMode::kCORS:
      case FetchRequestMode::kCORSWithForcedPreflight:
        tainting = FetchRequestData::kCORSTainting;
        break;
      case FetchRequestMode::kNavigate:
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
      case FetchResponseType::kCORS:
        tainting = FetchRequestData::kCORSTainting;
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

  FetchResponseData* response_data = nullptr;
  SRIBytesConsumer* sri_consumer = nullptr;
  if (fetch_request_data_->Integrity().IsEmpty()) {
    response_data = FetchResponseData::CreateWithBuffer(new BodyStreamBuffer(
        script_state,
        new BytesConsumerForDataConsumerHandle(
            ExecutionContext::From(script_state), std::move(handle)),
        signal_));
  } else {
    sri_consumer = new SRIBytesConsumer();
    response_data = FetchResponseData::CreateWithBuffer(
        new BodyStreamBuffer(script_state, sri_consumer, signal_));
  }
  response_data->SetStatus(response.HttpStatusCode());
  if (response.Url().ProtocolIsAbout() || response.Url().ProtocolIsData() ||
      response.Url().ProtocolIs("blob")) {
    response_data->SetStatusMessage("OK");
  } else {
    response_data->SetStatusMessage(response.HttpStatusText());
  }

  for (auto& it : response.HttpHeaderFields())
    response_data->HeaderList()->Append(it.key, it.value);
  if (response.UrlListViaServiceWorker().IsEmpty()) {
    // Note: |urlListViaServiceWorker| is empty, unless the response came from a
    // service worker, in which case it will only be empty if it was created
    // through new Response().
    response_data->SetURLList(url_list_);
  } else {
    DCHECK(response.WasFetchedViaServiceWorker());
    response_data->SetURLList(response.UrlListViaServiceWorker());
  }
  response_data->SetMIMEType(response.MimeType());
  response_data->SetResponseTime(response.ResponseTime());

  FetchResponseData* tainted_response = nullptr;

  DCHECK(!(network_utils::IsRedirectResponseCode(response_http_status_code_) &&
           HasNonEmptyLocationHeader(response_data->HeaderList()) &&
           fetch_request_data_->Redirect() != FetchRedirectMode::kManual));

  if (network_utils::IsRedirectResponseCode(response_http_status_code_) &&
      fetch_request_data_->Redirect() == FetchRedirectMode::kManual) {
    tainted_response = response_data->CreateOpaqueRedirectFilteredResponse();
  } else {
    switch (tainting) {
      case FetchRequestData::kBasicTainting:
        tainted_response = response_data->CreateBasicFilteredResponse();
        break;
      case FetchRequestData::kCORSTainting: {
        WebHTTPHeaderSet header_names =
            WebCORS::ExtractCorsExposedHeaderNamesList(
                fetch_request_data_->Credentials(),
                WrappedResourceResponse(response));
        tainted_response =
            response_data->CreateCORSFilteredResponse(header_names);
        break;
      }
      case FetchRequestData::kOpaqueTainting:
        tainted_response = response_data->CreateOpaqueFilteredResponse();
        break;
    }
  }

  Response* r =
      Response::Create(resolver_->GetExecutionContext(), tainted_response);
  r->headers()->SetGuard(Headers::kImmutableGuard);

  if (fetch_request_data_->Integrity().IsEmpty()) {
    resolver_->Resolve(r);
    resolver_.Clear();
  } else {
    DCHECK(!integrity_verifier_);
    integrity_verifier_ = new SRIVerifier(
        std::move(handle), sri_consumer, r, this,
        fetch_request_data_->Integrity(), response.Url(),
        r->GetResponse()->GetType(),
        resolver_->GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  }
}

void FetchManager::Loader::DidFinishLoading(unsigned long) {
  did_finish_loading_ = true;
  // If there is an integrity verifier, and it has not already finished, it
  // will take care of finishing the load or performing a network error when
  // verification is complete.
  if (integrity_verifier_ && !integrity_verifier_->IsFinished())
    return;

  LoadSucceeded();
}

void FetchManager::Loader::DidFail(const ResourceError& error) {
  Failed(String());
}

void FetchManager::Loader::DidFailRedirectCheck() {
  Failed(String());
}

Document* FetchManager::Loader::GetDocument() const {
  return DynamicTo<Document>(execution_context_.Get());
}

void FetchManager::Loader::LoadSucceeded() {
  DCHECK(!failed_);

  finished_ = true;

  if (GetDocument() && GetDocument()->GetFrame() &&
      GetDocument()->GetFrame()->GetPage() &&
      CORS::IsOkStatus(response_http_status_code_)) {
    GetDocument()->GetFrame()->GetPage()->GetChromeClient().AjaxSucceeded(
        GetDocument()->GetFrame());
  }
  NotifyFinished();
}

void FetchManager::Loader::Start(ExceptionState& exception_state) {
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
  if (!ContentSecurityPolicy::ShouldBypassMainWorld(execution_context_) &&
      !execution_context_->GetContentSecurityPolicy()->AllowConnectToSource(
          fetch_request_data_->Url())) {
    // "A network error."
    PerformNetworkError(
        "Refused to connect to '" + fetch_request_data_->Url().ElidedString() +
        "' because it violates the document's Content Security Policy.");
    return;
  }

  // "- |request|'s url's origin is |request|'s origin and the |CORS flag| is
  //    unset"
  // "- |request|'s url's scheme is 'data' and |request|'s same-origin data
  //    URL flag is set"
  // "- |request|'s url's scheme is 'about'"
  // Note we don't support to call this method with |CORS flag|
  // "- |request|'s mode is |navigate|".
  if ((SecurityOrigin::Create(fetch_request_data_->Url())
           ->IsSameSchemeHostPort(fetch_request_data_->Origin().get())) ||
      (fetch_request_data_->Url().ProtocolIsData() &&
       fetch_request_data_->SameOriginDataURLFlag()) ||
      (fetch_request_data_->Mode() == FetchRequestMode::kNavigate)) {
    // "The result of performing a scheme fetch using request."
    PerformSchemeFetch(exception_state);
    return;
  }

  // "- |request|'s mode is |same-origin|"
  if (fetch_request_data_->Mode() == FetchRequestMode::kSameOrigin) {
    // "A network error."
    PerformNetworkError("Fetch API cannot load " +
                        fetch_request_data_->Url().GetString() +
                        ". Request mode is \"same-origin\" but the URL\'s "
                        "origin is not same as the request origin " +
                        fetch_request_data_->Origin()->ToString() + ".");
    return;
  }

  // "- |request|'s mode is |no CORS|"
  if (fetch_request_data_->Mode() == FetchRequestMode::kNoCORS) {
    // "If |request|'s redirect mode is not |follow|, then return a network
    // error.
    if (fetch_request_data_->Redirect() != FetchRedirectMode::kFollow) {
      PerformNetworkError("Fetch API cannot load " +
                          fetch_request_data_->Url().GetString() +
                          ". Request mode is \"no-cors\" but the redirect mode "
                          " is not \"follow\".");
      return;
    }

    // "Set |request|'s response tainting to |opaque|."
    fetch_request_data_->SetResponseTainting(FetchRequestData::kOpaqueTainting);
    // "The result of performing a scheme fetch using |request|."
    PerformSchemeFetch(exception_state);
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
        ". URL scheme must be \"http\" or \"https\" for CORS request.");
    return;
  }

  // "Set |request|'s response tainting to |CORS|."
  fetch_request_data_->SetResponseTainting(FetchRequestData::kCORSTainting);

  // "The result of performing an HTTP fetch using |request| with the
  // |CORS flag| set."
  PerformHTTPFetch(exception_state);
}

void FetchManager::Loader::Dispose() {
  // Prevent notification
  fetch_manager_ = nullptr;
  if (threadable_loader_) {
    if (fetch_request_data_->Keepalive())
      threadable_loader_->Detach();
    else
      threadable_loader_->Cancel();
    threadable_loader_ = nullptr;
  }
  if (integrity_verifier_)
    integrity_verifier_->Cancel();
  execution_context_ = nullptr;
}

void FetchManager::Loader::Abort() {
  if (resolver_) {
    resolver_->Reject(DOMException::Create(DOMExceptionCode::kAbortError));
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

void FetchManager::Loader::PerformSchemeFetch(ExceptionState& exception_state) {
  // "To perform a scheme fetch using |request|, switch on |request|'s url's
  // scheme, and run the associated steps:"
  if (SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          fetch_request_data_->Url().Protocol()) ||
      fetch_request_data_->Url().ProtocolIs("blob")) {
    // "Return the result of performing an HTTP fetch using |request|."
    PerformHTTPFetch(exception_state);
    if (exception_state.HadException())
      return;
  } else if (fetch_request_data_->Url().ProtocolIsData()) {
    PerformDataFetch();
  } else {
    // FIXME: implement other protocols.
    PerformNetworkError(
        "Fetch API cannot load " + fetch_request_data_->Url().GetString() +
        ". URL scheme \"" + fetch_request_data_->Url().Protocol() +
        "\" is not supported.");
  }
}

void FetchManager::Loader::PerformNetworkError(const String& message) {
  Failed(message);
}

void FetchManager::Loader::PerformHTTPFetch(ExceptionState& exception_state) {
  // CORS preflight fetch procedure is implemented inside ThreadableLoader.

  // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
  //  body is a tee of |request|'s body."
  // We use ResourceRequest class for HTTPRequest.
  // FIXME: Support body.
  ResourceRequest request(fetch_request_data_->Url());
  request.SetRequestorOrigin(fetch_request_data_->Origin());
  request.SetRequestContext(fetch_request_data_->Context());
  request.SetHTTPMethod(fetch_request_data_->Method());

  switch (fetch_request_data_->Mode()) {
    case FetchRequestMode::kSameOrigin:
    case FetchRequestMode::kNoCORS:
    case FetchRequestMode::kCORS:
    case FetchRequestMode::kCORSWithForcedPreflight:
      request.SetFetchRequestMode(fetch_request_data_->Mode());
      break;
    case FetchRequestMode::kNavigate:
      // Using kSameOrigin here to reduce the security risk.
      // "navigate" request is only available in ServiceWorker.
      request.SetFetchRequestMode(FetchRequestMode::kSameOrigin);
      break;
  }

  request.SetFetchCredentialsMode(fetch_request_data_->Credentials());
  for (const auto& header : fetch_request_data_->HeaderList()->List()) {
    // Since |fetch_request_data_|'s headers are populated with either of the
    // "request" guard or "request-no-cors" guard, we can assume that none of
    // the headers have a name listed in the forbidden header names.
    DCHECK(!CORS::IsForbiddenHeaderName(header.first));

    request.AddHTTPHeaderField(AtomicString(header.first),
                               AtomicString(header.second));
  }

  if (fetch_request_data_->Method() != HTTPNames::GET &&
      fetch_request_data_->Method() != HTTPNames::HEAD) {
    if (fetch_request_data_->Buffer()) {
      request.SetHTTPBody(
          fetch_request_data_->Buffer()->DrainAsFormData(exception_state));
      if (exception_state.HadException())
        return;
    }
  }
  request.SetCacheMode(fetch_request_data_->CacheMode());
  request.SetFetchRedirectMode(fetch_request_data_->Redirect());
  request.SetFetchImportanceMode(fetch_request_data_->Importance());
  request.SetPriority(fetch_request_data_->Priority());
  request.SetUseStreamOnResponse(true);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context_->GetSecurityContext().AddressSpace());
  request.SetReferrerString(fetch_request_data_->ReferrerString());
  request.SetReferrerPolicy(fetch_request_data_->GetReferrerPolicy());

  request.SetSkipServiceWorker(is_isolated_world_);

  if (fetch_request_data_->Keepalive()) {
    if (!CORS::IsCORSSafelistedMethod(request.HttpMethod()) ||
        !CORS::ContainsOnlyCORSSafelistedOrForbiddenHeaders(
            request.HttpHeaderFields())) {
      PerformNetworkError(
          "Preflight request for request with keepalive "
          "specified is currently not supported");
      return;
    }
    request.SetKeepalive(true);
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

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.initiator_info.name = FetchInitiatorTypeNames::fetch;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;
  if (fetch_request_data_->URLLoaderFactory()) {
    network::mojom::blink::URLLoaderFactoryPtr factory_clone;
    fetch_request_data_->URLLoaderFactory()->Clone(MakeRequest(&factory_clone));
    resource_loader_options.url_loader_factory = base::MakeRefCounted<
        base::RefCountedData<network::mojom::blink::URLLoaderFactoryPtr>>(
        std::move(factory_clone));
  }

  threadable_loader_ = new ThreadableLoader(*execution_context_, this,
                                            resource_loader_options);
  threadable_loader_->Start(request);
}

// performDataFetch() is almost the same as performHTTPFetch(), except for:
// - We set AllowCrossOriginRequests to allow requests to data: URLs in
//   'same-origin' mode.
// - We reject non-GET method.
void FetchManager::Loader::PerformDataFetch() {
  DCHECK(fetch_request_data_->Url().ProtocolIsData());

  ResourceRequest request(fetch_request_data_->Url());
  request.SetRequestorOrigin(fetch_request_data_->Origin());
  request.SetRequestContext(fetch_request_data_->Context());
  request.SetUseStreamOnResponse(true);
  request.SetHTTPMethod(fetch_request_data_->Method());
  request.SetFetchCredentialsMode(network::mojom::FetchCredentialsMode::kOmit);
  request.SetFetchRedirectMode(FetchRedirectMode::kError);
  request.SetFetchImportanceMode(fetch_request_data_->Importance());
  request.SetPriority(fetch_request_data_->Priority());
  // We intentionally skip 'setExternalRequestStateFromRequestorAddressSpace',
  // as 'data:' can never be external.

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  threadable_loader_ = new ThreadableLoader(*execution_context_, this,
                                            resource_loader_options);
  threadable_loader_->Start(request);
}

void FetchManager::Loader::Failed(const String& message) {
  if (failed_ || finished_)
    return;
  failed_ = true;
  if (execution_context_->IsContextDestroyed())
    return;
  if (!message.IsEmpty()) {
    execution_context_->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
  }
  if (resolver_) {
    ScriptState* state = resolver_->GetScriptState();
    ScriptState::Scope scope(state);
    resolver_->Reject(V8ThrowException::CreateTypeError(state->GetIsolate(),
                                                        "Failed to fetch"));
  }
  NotifyFinished();
}

void FetchManager::Loader::NotifyFinished() {
  if (fetch_manager_)
    fetch_manager_->OnLoaderFinished(this);
}

FetchManager* FetchManager::Create(ExecutionContext* execution_context) {
  return new FetchManager(execution_context);
}

FetchManager::FetchManager(ExecutionContext* execution_context)
    : ContextLifecycleObserver(execution_context) {}

ScriptPromise FetchManager::Fetch(ScriptState* script_state,
                                  FetchRequestData* request,
                                  AbortSignal* signal,
                                  ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  DCHECK(signal);
  if (signal->aborted()) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kAbortError));
    return promise;
  }

  request->SetContext(mojom::RequestContextType::FETCH);

  Loader* loader =
      Loader::Create(GetExecutionContext(), this, resolver, request,
                     script_state->World().IsIsolatedWorld(), signal);
  loaders_.insert(loader);
  signal->AddAlgorithm(WTF::Bind(&Loader::Abort, WrapWeakPersistent(loader)));
  // TODO(ricea): Reject the Response body with AbortError, not TypeError.
  loader->Start(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return promise;
}

void FetchManager::ContextDestroyed(ExecutionContext*) {
  for (auto& loader : loaders_)
    loader->Dispose();
}

void FetchManager::OnLoaderFinished(Loader* loader) {
  loaders_.erase(loader);
  loader->Dispose();
}

void FetchManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(loaders_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
