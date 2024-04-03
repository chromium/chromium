// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_TEST_UTILS_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_TEST_UTILS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/test/test_network_context_with_host_resolver.h"
#include "services/network/test/test_restricted_udp_socket.h"
#include "services/network/test/test_udp_socket.h"

namespace url {
class Origin;
}  // namespace url

namespace content::test {

// Mock UDP Socket for Direct Sockets browsertests.
class MockUDPSocket : public network::TestUDPSocket {
 public:
  explicit MockUDPSocket(
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

  ~MockUDPSocket() override;

  // network::TestUDPSocket:
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override;
  void ReceiveMore(uint32_t) override {}
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;

  // Sends some data to the remote.
  void MockSend(int32_t result,
                const std::optional<base::span<uint8_t>>& data = {});

  mojo::Remote<network::mojom::UDPSocketListener>& get_listener() {
    return listener_;
  }

  // Sets an additional callback to be run when a call to Send() arrives.
  void SetAdditionalSendCallback(base::OnceClosure additional_send_callback) {
    additional_send_callback_ = std::move(additional_send_callback);
  }

  // Sets the value to run the callback supplied in Send() with.
  // If not specified, callback will be stored and run with net::OK on class
  // destruction.
  void SetNextSendResult(int result) { next_send_result_ = result; }

 protected:
  mojo::Remote<network::mojom::UDPSocketListener> listener_;

  std::optional<int> next_send_result_;

  SendCallback callback_;
  base::OnceClosure additional_send_callback_;
};

class MockRestrictedUDPSocket : public network::TestRestrictedUDPSocket {
 public:
  MockRestrictedUDPSocket(
      std::unique_ptr<network::TestUDPSocket> udp_socket,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver);
  ~MockRestrictedUDPSocket() override;

 private:
  mojo::Receiver<network::mojom::RestrictedUDPSocket> receiver_;
};

// Mock Network Context for Direct Sockets browsertests.
class MockNetworkContext : public network::TestNetworkContextWithHostResolver {
 public:
  MockNetworkContext();
  explicit MockNetworkContext(std::string_view host_mapping_rules);

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  ~MockNetworkContext() override;

  // network::TestNetworkContext:
  void CreateRestrictedUDPSocket(
      const net::IPEndPoint& addr,
      network::mojom::RestrictedUDPSocketMode mode,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::RestrictedUDPSocketParamsPtr params,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      CreateRestrictedUDPSocketCallback callback) override;

  MockUDPSocket* get_udp_socket() {
    return static_cast<MockUDPSocket*>(restricted_udp_socket_->udp_socket());
  }

 protected:
  virtual std::unique_ptr<MockUDPSocket> CreateMockUDPSocket(
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

  std::unique_ptr<MockRestrictedUDPSocket> restricted_udp_socket_;
};

// A wrapper class that allows running javascript asynchronously.
//
//    * RunScript(...) returns a unique pointer to
//      base::test::TestFuture<std::string>. Call
//      Get(...) on the future pointer to wait for
//      the script to complete.
//    * Note that the observer expects exactly one message per script
//      invocation:
//      DCHECK(...) will fire if more than one message arrives.
//    * Can be reused. The following sketch is totally valid:
//
//      IN_PROC_BROWSER_TEST_F(MyTestFixture, MyTest) {
//        auto runner =
//        std::make_unique<AsyncJsRunner>(shell()->web_contents());
//
//        const std::string script_template = "return $1;";
//
//        const std::string script_a = JsReplace(script_template, "MessageA");
//        auto future_a = runner->RunScript(WrapAsync(script_a));
//        EXPECT_EQ(future_a->Get(), "\"MessageA\"");
//
//        const std::string script_b = JsReplace(script_template, "MessageB");
//        auto future_b = runner->RunScript(WrapAsync(script_b));
//        EXPECT_EQ(future_b->Get(), "\"MessageB\"");
//      }
//
// Make sure to pass async functions to RunScript(...) (see WrapAsync(...)
// below).

class AsyncJsRunner : public WebContentsObserver {
 public:
  explicit AsyncJsRunner(content::WebContents* web_contents);
  ~AsyncJsRunner() override;

  std::unique_ptr<base::test::TestFuture<std::string>> RunScript(
      const std::string& script);

  // WebContentsObserver:
  void DomOperationResponse(RenderFrameHost* render_frame_host,
                            const std::string& json_string) override;

 private:
  std::string MakeScriptSendResultToDomQueue(const std::string& script) const;

  base::OnceCallback<void(std::string)> future_callback_;
  base::Token token_;
};

std::string WrapAsync(const std::string& script);

// Mock ContentBrowserClient that enableds direct sockets via permissions policy
// for isolated apps.
class IsolatedWebAppContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit IsolatedWebAppContentBrowserClient(
      const url::Origin& isolated_app_origin);

  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override;

  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(WebContents* web_contents,
                                        const url::Origin& app_origin) override;

 private:
  url::Origin isolated_app_origin_;
};

}  // namespace content::test

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_TEST_UTILS_H_
