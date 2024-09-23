// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/fetch.h"

namespace network {
namespace mojom {
class URLLoaderFactoryOverride;
}
}  // namespace network

namespace content {
class DevToolsAgentHostImpl;
class DevToolsIOContext;
class DevToolsURLLoaderInterceptor;
class StoragePartition;
struct InterceptedRequestInfo;

namespace protocol {

class FetchHandler : public DevToolsDomainHandler, public Fetch::Backend {
 public:
  using UpdateLoaderFactoriesCallback =
      base::RepeatingCallback<void(base::OnceClosure)>;

  FetchHandler(DevToolsIOContext* io_context,
               UpdateLoaderFactoriesCallback update_loader_factories_callback);

  FetchHandler(const FetchHandler&) = delete;
  FetchHandler& operator=(const FetchHandler&) = delete;

  ~FetchHandler() override;

  static std::vector<FetchHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  bool MaybeCreateProxyForInterception(
      int process_id,
      StoragePartition* storage_partition,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      network::mojom::URLLoaderFactoryOverride* intercepting_factory);

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
      Maybe<protocol::Binary> postData,
      Maybe<Array<Fetch::HeaderEntry>> headers,
      Maybe<bool> interceptResponse,
      std::unique_ptr<ContinueRequestCallback> callback) override;
  void ContinueWithAuth(
      const String& fetchId,
      std::unique_ptr<protocol::Fetch::AuthChallengeResponse>
          authChallengeResponse,
      std::unique_ptr<ContinueWithAuthCallback> callback) override;
  void ContinueResponse(
      const String& fetchId,
      Maybe<int> responseCode,
      Maybe<String> responsePhrase,
      Maybe<Array<Fetch::HeaderEntry>> responseHeaders,
      Maybe<Binary> binaryResponseHeaders,
      std::unique_ptr<ContinueResponseCallback> callback) override;
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

  const raw_ptr<DevToolsIOContext> io_context_;
  std::unique_ptr<Fetch::Frontend> frontend_;
  std::unique_ptr<DevToolsURLLoaderInterceptor> interceptor_;
  UpdateLoaderFactoriesCallback update_loader_factories_callback_;
  base::WeakPtrFactory<FetchHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
