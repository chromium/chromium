// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/devtools/protocol/fetch_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_stream_pipe.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"

namespace content {
namespace protocol {

// static
std::vector<FetchHandler*> FetchHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<FetchHandler>(Fetch::Metainfo::domainName);
}

FetchHandler::FetchHandler(
    DevToolsIOContext* io_context,
    UpdateLoaderFactoriesCallback update_loader_factories_callback)
    : DevToolsDomainHandler(Fetch::Metainfo::domainName),
      io_context_(io_context),
      update_loader_factories_callback_(
          std::move(update_loader_factories_callback)) {}

FetchHandler::~FetchHandler() = default;

void FetchHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Fetch::Frontend>(dispatcher->channel());
  Fetch::Dispatcher::wire(dispatcher, this);
}

DevToolsURLLoaderInterceptor::InterceptionStage RequestStageToInterceptorStage(
    const Fetch::RequestStage& stage) {
  if (stage == Fetch::RequestStageEnum::Request)
    return DevToolsURLLoaderInterceptor::kRequest;
  if (stage == Fetch::RequestStageEnum::Response)
    return DevToolsURLLoaderInterceptor::kResponse;
  NOTREACHED_IN_MIGRATION();
  return DevToolsURLLoaderInterceptor::kRequest;
}

Response ToInterceptionPatterns(
    Maybe<Array<Fetch::RequestPattern>>& maybe_patterns,
    std::vector<DevToolsURLLoaderInterceptor::Pattern>* result) {
  result->clear();
  if (!maybe_patterns.has_value()) {
    result->emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
    return Response::Success();
  }
  Array<Fetch::RequestPattern>& patterns = maybe_patterns.value();
  for (const std::unique_ptr<Fetch::RequestPattern>& pattern : patterns) {
    base::flat_set<blink::mojom::ResourceType> resource_types;
    std::string resource_type = pattern->GetResourceType("");
    if (!resource_type.empty()) {
      if (!NetworkHandler::AddInterceptedResourceType(resource_type,
                                                      &resource_types)) {
        return Response::InvalidParams(
            base::StringPrintf("Unknown resource type in fetch filter: '%s'",
                               resource_type.c_str()));
      }
    }
    result->emplace_back(
        pattern->GetUrlPattern("*"), std::move(resource_types),
        RequestStageToInterceptorStage(
            pattern->GetRequestStage(Fetch::RequestStageEnum::Request)));
  }
  return Response::Success();
}

bool FetchHandler::MaybeCreateProxyForInterception(
    int process_id,
    StoragePartition* storage_partition,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* intercepting_factory) {
  return interceptor_ && interceptor_->CreateProxyForInterception(
                             process_id, storage_partition, frame_token,
                             is_navigation, is_download, intercepting_factory);
}

void FetchHandler::Enable(Maybe<Array<Fetch::RequestPattern>> patterns,
                          Maybe<bool> handleAuth,
                          std::unique_ptr<EnableCallback> callback) {
  if (!interceptor_) {
    interceptor_ =
        std::make_unique<DevToolsURLLoaderInterceptor>(base::BindRepeating(
            &FetchHandler::RequestIntercepted, weak_factory_.GetWeakPtr()));
  }
  std::vector<DevToolsURLLoaderInterceptor::Pattern> interception_patterns;
  Response response = ToInterceptionPatterns(patterns, &interception_patterns);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  if (!interception_patterns.size() && handleAuth.value_or(false)) {
    callback->sendFailure(Response::InvalidParams(
        "Can\'t specify empty patterns with handleAuth set"));
    return;
  }
  interceptor_->SetPatterns(std::move(interception_patterns),
                            handleAuth.value_or(false));
  update_loader_factories_callback_.Run(
      base::BindOnce(&EnableCallback::sendSuccess, std::move(callback)));
}

Response FetchHandler::Disable() {
  const bool was_enabled = !!interceptor_;
  interceptor_.reset();
  if (was_enabled)
    update_loader_factories_callback_.Run(base::DoNothing());
  return Response::Success();
}

namespace {
using ContinueInterceptedRequestCallback =
    DevToolsURLLoaderInterceptor::ContinueInterceptedRequestCallback;

template <typename Callback, typename Base, typename... Args>
class CallbackWrapper : public Base {
 public:
  explicit CallbackWrapper(std::unique_ptr<Callback> callback)
      : callback_(std::move(callback)) {}

 private:
  friend typename std::unique_ptr<CallbackWrapper>::deleter_type;

  void sendSuccess(Args... args) override {
    callback_->sendSuccess(std::forward<Args>(args)...);
  }
  void sendFailure(const DispatchResponse& response) override {
    callback_->sendFailure(response);
  }
  void fallThrough() override { NOTREACHED_IN_MIGRATION(); }
  ~CallbackWrapper() override {}

  std::unique_ptr<Callback> callback_;
};

template <typename Callback>
std::unique_ptr<CallbackWrapper<Callback, ContinueInterceptedRequestCallback>>
WrapCallback(std::unique_ptr<Callback> cb) {
  return std::make_unique<
      CallbackWrapper<Callback, ContinueInterceptedRequestCallback>>(
      std::move(cb));
}

template <typename Callback>
bool ValidateHeaders(Fetch::HeaderEntry* entry, Callback* callback) {
  if (!net::HttpUtil::IsValidHeaderName(entry->GetName()) ||
      !net::HttpUtil::IsValidHeaderValue(entry->GetValue())) {
    callback->sendFailure(Response::InvalidParams("Invalid header"));
    return false;
  }
  return true;
}
}  // namespace

void FetchHandler::FailRequest(const String& requestId,
                               const String& errorReason,
                               std::unique_ptr<FailRequestCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  bool ok = false;
  net::Error reason = NetworkHandler::NetErrorFromString(errorReason, &ok);
  if (!ok) {
    callback->sendFailure(Response::InvalidParams("Unknown errorReason"));
    return;
  }
  auto modifications =
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(reason);
  interceptor_->ContinueInterceptedRequest(requestId, std::move(modifications),
                                           WrapCallback(std::move(callback)));
}

namespace {
std::string GetReasonPhrase(int responseCode) {
  if (const char* phrase = net::TryToGetHttpReasonPhrase(
          static_cast<net::HttpStatusCode>(responseCode))) {
    return phrase;
  }
  return "";
}
}  // namespace

void FetchHandler::FulfillRequest(
    const String& requestId,
    int responseCode,
    Maybe<Array<Fetch::HeaderEntry>> responseHeaders,
    Maybe<Binary> binaryResponseHeaders,
    Maybe<Binary> body,
    Maybe<String> responsePhrase,
    std::unique_ptr<FulfillRequestCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  const std::string status_phrase = responsePhrase.has_value()
                                        ? responsePhrase.value()
                                        : GetReasonPhrase(responseCode);
  if (status_phrase.empty()) {
    callback->sendFailure(
        Response::InvalidParams("Invalid http status code or phrase"));
    return;
  }
  std::string headers =
      base::StringPrintf("HTTP/1.1 %d %s", responseCode, status_phrase.c_str());
  headers.append(1, '\0');
  if (responseHeaders.has_value()) {
    if (binaryResponseHeaders.has_value()) {
      callback->sendFailure(Response::InvalidParams(
          "Only one of responseHeaders or binaryHeaders may be present"));
      return;
    }
    for (const auto& entry : responseHeaders.value()) {
      if (!ValidateHeaders(entry.get(), callback.get()))
        return;
      headers.append(entry->GetName());
      headers.append(":");
      headers.append(entry->GetValue());
      headers.append(1, '\0');
    }
  } else if (binaryResponseHeaders.has_value()) {
    Binary response_headers = binaryResponseHeaders.value();
    headers.append(reinterpret_cast<const char*>(response_headers.data()),
                   response_headers.size());
    if (headers.back() != '\0')
      headers.append(1, '\0');
  }
  headers.append(1, '\0');
  auto modifications =
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(
          base::MakeRefCounted<net::HttpResponseHeaders>(headers),
          body.has_value() ? body.value().bytes() : nullptr);
  interceptor_->ContinueInterceptedRequest(requestId, std::move(modifications),
                                           WrapCallback(std::move(callback)));
}

void FetchHandler::ContinueRequest(
    const String& requestId,
    Maybe<String> url,
    Maybe<String> method,
    Maybe<protocol::Binary> postData,
    Maybe<Array<Fetch::HeaderEntry>> headers,
    Maybe<bool> interceptResponse,
    std::unique_ptr<ContinueRequestCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  std::unique_ptr<DevToolsURLLoaderInterceptor::Modifications::HeadersVector>
      request_headers;
  if (headers.has_value()) {
    request_headers = std::make_unique<
        DevToolsURLLoaderInterceptor::Modifications::HeadersVector>();
    for (auto& entry : headers.value()) {
      if (!ValidateHeaders(entry.get(), callback.get()))
        return;
      request_headers->emplace_back(entry->GetName(), entry->GetValue());
    }
  }
  auto modifications =
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(
          std::move(url), std::move(method), std::move(postData),
          std::move(request_headers), std::move(interceptResponse));
  interceptor_->ContinueInterceptedRequest(requestId, std::move(modifications),
                                           WrapCallback(std::move(callback)));
}

void FetchHandler::ContinueWithAuth(
    const String& requestId,
    std::unique_ptr<protocol::Fetch::AuthChallengeResponse>
        authChallengeResponse,
    std::unique_ptr<ContinueWithAuthCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  using AuthChallengeResponse =
      DevToolsURLLoaderInterceptor::AuthChallengeResponse;
  std::unique_ptr<AuthChallengeResponse> auth_response;
  std::string type = authChallengeResponse->GetResponse();
  if (type == Network::AuthChallengeResponse::ResponseEnum::Default) {
    auth_response = std::make_unique<AuthChallengeResponse>(
        AuthChallengeResponse::kDefault);
  } else if (type == Network::AuthChallengeResponse::ResponseEnum::CancelAuth) {
    auth_response = std::make_unique<AuthChallengeResponse>(
        AuthChallengeResponse::kCancelAuth);
  } else if (type ==
             Network::AuthChallengeResponse::ResponseEnum::ProvideCredentials) {
    auth_response = std::make_unique<AuthChallengeResponse>(
        base::UTF8ToUTF16(authChallengeResponse->GetUsername("")),
        base::UTF8ToUTF16(authChallengeResponse->GetPassword("")));
  } else {
    callback->sendFailure(
        Response::InvalidParams("Unrecognized authChallengeResponse"));
    return;
  }
  auto modifications =
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(
          std::move(auth_response));
  interceptor_->ContinueInterceptedRequest(requestId, std::move(modifications),
                                           WrapCallback(std::move(callback)));
}

void FetchHandler::ContinueResponse(
    const String& requestId,
    Maybe<int> responseCode,
    Maybe<String> responsePhrase,
    Maybe<Array<Fetch::HeaderEntry>> responseHeaders,
    Maybe<Binary> binaryResponseHeaders,
    std::unique_ptr<ContinueResponseCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  if (responseCode.has_value() &&
      (responseHeaders.has_value() || binaryResponseHeaders.has_value())) {
    auto wrapped_callback = std::make_unique<
        CallbackWrapper<ContinueResponseCallback, FulfillRequestCallback>>(
        std::move(callback));
    FulfillRequest(requestId, responseCode.value(), std::move(responseHeaders),
                   std::move(binaryResponseHeaders), {},
                   std::move(responsePhrase), std::move(wrapped_callback));
    return;
  }
  if (!responseCode.has_value() && !responsePhrase.has_value() &&
      !responseHeaders.has_value() && !binaryResponseHeaders.has_value()) {
    interceptor_->ContinueInterceptedRequest(
        requestId,
        std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
        WrapCallback(std::move(callback)));
    return;
  }
  callback->sendFailure(Response::ServerError(
      "Cannot override only status or headers, both should be provided"));
}

void FetchHandler::GetResponseBody(
    const String& requestId,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  auto wrapped_callback = std::make_unique<CallbackWrapper<
      GetResponseBodyCallback,
      DevToolsURLLoaderInterceptor::GetResponseBodyForInterceptionCallback,
      const std::string&, bool>>(std::move(callback));
  interceptor_->GetResponseBody(requestId, std::move(wrapped_callback));
}

void FetchHandler::TakeResponseBodyAsStream(
    const String& requestId,
    std::unique_ptr<TakeResponseBodyAsStreamCallback> callback) {
  if (!interceptor_) {
    callback->sendFailure(Response::ServerError("Fetch domain is not enabled"));
    return;
  }
  interceptor_->TakeResponseBodyPipe(
      requestId,
      base::BindOnce(&FetchHandler::OnResponseBodyPipeTaken,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FetchHandler::OnResponseBodyPipeTaken(
    std::unique_ptr<TakeResponseBodyAsStreamCallback> callback,
    Response response,
    mojo::ScopedDataPipeConsumerHandle pipe,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(response.IsSuccess(), pipe.is_valid());
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }
  // The pipe stream is owned only by io_context after we return.
  bool is_binary = !DevToolsIOContext::IsTextMimeType(mime_type);
  auto stream =
      DevToolsStreamPipe::Create(io_context_, std::move(pipe), is_binary);
  callback->sendSuccess(stream->handle());
}

namespace {

std::unique_ptr<Array<Fetch::HeaderEntry>> ToHeaderEntryArray(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  auto result = std::make_unique<Array<Fetch::HeaderEntry>>();
  size_t iterator = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iterator, &name, &value)) {
    result->emplace_back(
        Fetch::HeaderEntry::Create().SetName(name).SetValue(value).Build());
  }
  return result;
}

}  // namespace

void FetchHandler::RequestIntercepted(
    std::unique_ptr<InterceptedRequestInfo> info) {
  protocol::Maybe<protocol::Network::ErrorReason> error_reason;
  if (info->response_error_code < 0)
    error_reason = NetworkHandler::NetErrorToString(info->response_error_code);

  Maybe<int> status_code;
  Maybe<std::string> status_text;
  Maybe<Array<Fetch::HeaderEntry>> response_headers;
  if (info->response_headers) {
    status_code = info->response_headers->response_code();
    status_text = info->response_headers->GetStatusText();
    response_headers = ToHeaderEntryArray(info->response_headers);
  }

  if (info->auth_challenge) {
    auto auth_challenge =
        Fetch::AuthChallenge::Create()
            .SetSource(info->auth_challenge->is_proxy
                           ? Network::AuthChallenge::SourceEnum::Proxy
                           : Network::AuthChallenge::SourceEnum::Server)
            .SetOrigin(info->auth_challenge->challenger.Serialize())
            .SetScheme(info->auth_challenge->scheme)
            .SetRealm(info->auth_challenge->realm)
            .Build();
    frontend_->AuthRequired(
        info->interception_id, std::move(info->network_request),
        info->frame_id.ToString(),
        NetworkHandler::ResourceTypeToString(info->resource_type),
        std::move(auth_challenge));
    return;
  }
  frontend_->RequestPaused(
      info->interception_id, std::move(info->network_request),
      info->frame_id.ToString(),
      NetworkHandler::ResourceTypeToString(info->resource_type),
      std::move(error_reason), std::move(status_code), std::move(status_text),
      std::move(response_headers), std::move(info->renderer_request_id),
      std::move(info->redirected_request_id));
}

}  // namespace protocol
}  // namespace content
