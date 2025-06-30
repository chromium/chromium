// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/fetch.h"
#include "mojo/public/cpp/system/data_pipe.h"

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
               UpdateLoaderFactoriesCallback update_loader_factories_callback,
               base::OnceClosure cleanup_after_modifications_callback =
                   base::OnceClosure());

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
  void Enable(std::unique_ptr<Array<Fetch::RequestPattern>> patterns,
              std::optional<bool> handleAuth,
              std::unique_ptr<EnableCallback> callback) override;

  void FailRequest(const String& fetchId,
                   const String& errorReason,
                   std::unique_ptr<FailRequestCallback> callback) override;
  void FulfillRequest(
      const String& fetchId,
      int responseCode,
      std::unique_ptr<Array<Fetch::HeaderEntry>> responseHeaders,
      std::optional<Binary> binaryResponseHeaders,
      std::optional<Binary> body,
      std::optional<String> responsePhrase,
      std::unique_ptr<FulfillRequestCallback> callback) override;
  void ContinueRequest(
      const String& fetchId,
      std::optional<String> url,
      std::optional<String> method,
      std::optional<protocol::Binary> postData,
      std::unique_ptr<Array<Fetch::HeaderEntry>> headers,
      std::optional<bool> interceptResponse,
      std::unique_ptr<ContinueRequestCallback> callback) override;
  void ContinueWithAuth(
      const String& fetchId,
      std::unique_ptr<protocol::Fetch::AuthChallengeResponse>
          authChallengeResponse,
      std::unique_ptr<ContinueWithAuthCallback> callback) override;
  void ContinueResponse(
      const String& fetchId,
      std::optional<int> responseCode,
      std::optional<String> responsePhrase,
      std::unique_ptr<Array<Fetch::HeaderEntry>> responseHeaders,
      std::optional<Binary> binaryResponseHeaders,
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
  bool did_modifications_ = false;
  base::OnceClosure cleanup_after_modifications_callback_;
  base::WeakPtrFactory<FetchHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
