// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/fetch.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {
class DevToolsAgentHostImpl;
class DevToolsIOContext;
class DevToolsURLLoaderInterceptor;
class RenderProcessHost;
struct InterceptedRequestInfo;

namespace protocol {

class FetchHandler : public DevToolsDomainHandler, public Fetch::Backend {
 public:
  using UpdateLoaderFactoriesCallback =
      base::RepeatingCallback<void(base::OnceClosure)>;

  FetchHandler(DevToolsIOContext* io_context,
               UpdateLoaderFactoriesCallback update_loader_factories_callback);
  ~FetchHandler() override;

  static std::vector<FetchHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  bool MaybeCreateProxyForInterception(
      RenderProcessHost* rph,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          target_factory_receiver);

 private:
  // DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Protocol methods.
  void Enable(Maybe<Array<Fetch::RequestPattern>> patterns,
              Maybe<bool> handleAuth,
              std::unique_ptr<EnableCallback> callback) override;

  void FailRequest(const String& fetchId,
                   const String& errorReason,
                   std::unique_ptr<FailRequestCallback> callback) override;
  void FulfillRequest(
      const String& fetchId,
      int responseCode,
      Maybe<Array<Fetch::HeaderEntry>> responseHeaders,
      Maybe<Binary> binaryResponseHeaders,
      Maybe<Binary> body,
      Maybe<String> responsePhrase,
      std::unique_ptr<FulfillRequestCallback> callback) override;
  void ContinueRequest(
      const String& fetchId,
      Maybe<String> url,
      Maybe<String> method,
      Maybe<String> postData,
      Maybe<Array<Fetch::HeaderEntry>> headers,
      std::unique_ptr<ContinueRequestCallback> callback) override;
  void ContinueWithAuth(
      const String& fetchId,
      std::unique_ptr<protocol::Fetch::AuthChallengeResponse>
          authChallengeResponse,
      std::unique_ptr<ContinueWithAuthCallback> callback) override;
  void GetResponseBody(
      const String& fetchId,
      std::unique_ptr<GetResponseBodyCallback> callback) override;
  void TakeResponseBodyAsStream(
      const String& fetchId,
      std::unique_ptr<TakeResponseBodyAsStreamCallback> callback) override;

  void OnResponseBodyPipeTaken(
      std::unique_ptr<TakeResponseBodyAsStreamCallback> callback,
      Response response,
      mojo::ScopedDataPipeConsumerHandle pipe,
      const std::string& mime_type);

  void RequestIntercepted(std::unique_ptr<InterceptedRequestInfo> info);

  DevToolsIOContext* const io_context_;
  std::unique_ptr<Fetch::Frontend> frontend_;
  std::unique_ptr<DevToolsURLLoaderInterceptor> interceptor_;
  UpdateLoaderFactoriesCallback update_loader_factories_callback_;
  base::WeakPtrFactory<FetchHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FetchHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
