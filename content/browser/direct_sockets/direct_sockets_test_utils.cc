// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_test_utils.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "url/origin.h"

namespace content::test {

// MockUDPSocket implementation

MockUDPSocket::MockUDPSocket(
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  listener_.Bind(std::move(listener));
}

MockUDPSocket::~MockUDPSocket() {
  if (callback_) {
    std::move(callback_).Run(net::OK);
  }
}

void MockUDPSocket::Connect(const net::IPEndPoint& remote_addr,
                            network::mojom::UDPSocketOptionsPtr socket_options,
                            ConnectCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), net::OK,
                     net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}));
}

void MockUDPSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  if (additional_send_callback_) {
    std::move(additional_send_callback_).Run();
  }
  if (next_send_result_) {
    std::move(callback_).Run(*next_send_result_);
    next_send_result_.reset();
  }
}

void MockUDPSocket::MockSend(int32_t result,
                             const std::optional<base::span<uint8_t>>& data) {
  listener_->OnReceived(result, {}, data);
}

MockRestrictedUDPSocket::MockRestrictedUDPSocket(
    std::unique_ptr<network::TestUDPSocket> udp_socket,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver)
    : network::TestRestrictedUDPSocket(std::move(udp_socket)),
      receiver_(this, std::move(receiver)) {}

MockRestrictedUDPSocket::~MockRestrictedUDPSocket() = default;

// MockNetworkContext implementation

MockNetworkContext::MockNetworkContext()
    : MockNetworkContext(/*host_mapping_rules=*/"") {}

MockNetworkContext::MockNetworkContext(std::string_view host_mapping_rules)
    : network::TestNetworkContextWithHostResolver(
          net::HostResolver::CreateStandaloneResolver(
              net::NetLog::Get(),
              /*options=*/std::nullopt,
              host_mapping_rules,
              /*enable_caching=*/false)) {}

MockNetworkContext::~MockNetworkContext() = default;

void MockNetworkContext::CreateRestrictedUDPSocket(
    const net::IPEndPoint& addr,
    network::mojom::RestrictedUDPSocketMode mode,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    network::mojom::RestrictedUDPSocketParamsPtr params,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    CreateRestrictedUDPSocketCallback callback) {
  auto socket = CreateMockUDPSocket(std::move(listener));
  DCHECK_EQ(mode, network::mojom::RestrictedUDPSocketMode::CONNECTED);
  socket->Connect(addr, params ? std::move(params->socket_options) : nullptr,
                  std::move(callback));
  restricted_udp_socket_ = std::make_unique<MockRestrictedUDPSocket>(
      std::move(socket), std::move(receiver));
}

std::unique_ptr<MockUDPSocket> MockNetworkContext::CreateMockUDPSocket(
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  return std::make_unique<MockUDPSocket>(std::move(listener));
}

// AsyncJsRunner implementation

AsyncJsRunner::AsyncJsRunner(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

AsyncJsRunner::~AsyncJsRunner() = default;

std::unique_ptr<base::test::TestFuture<std::string>> AsyncJsRunner::RunScript(
    const std::string& async_script) {
  // Do not leave behind hanging futures from previous invocations.
  DCHECK(!future_callback_);
  auto future = std::make_unique<base::test::TestFuture<std::string>>();

  token_ = base::Token::CreateRandom();
  future_callback_ = future->GetCallback();
  const std::string wrapped_script =
      MakeScriptSendResultToDomQueue(async_script);
  ExecuteScriptAsync(web_contents(), wrapped_script);

  return future;
}

void AsyncJsRunner::DomOperationResponse(RenderFrameHost* render_frame_host,
                                         const std::string& json_string) {
  // Check that future is valid and not yet fulfilled.
  DCHECK(future_callback_);

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(
      json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(parsed.has_value());
  DCHECK_EQ(parsed->type(), base::Value::Type::LIST);

  const auto& list = parsed->GetList();
  DCHECK_EQ(list.size(), 2U);
  DCHECK(list[0].is_string());
  DCHECK(list[1].is_string());

  if (list[1].GetString() == token_.ToString()) {
    std::move(future_callback_).Run(list[0].GetString());
  }
}

std::string AsyncJsRunner::MakeScriptSendResultToDomQueue(
    const std::string& script) const {
  DCHECK(!script.empty());
  return WrapAsync(base::StringPrintf(
      R"(
        let result = await %s;
        window.domAutomationController.send([result, '%s']);
      )",
      script.c_str(), token_.ToString().c_str()));
}

IsolatedWebAppContentBrowserClient::IsolatedWebAppContentBrowserClient(
    const url::Origin& isolated_app_origin)
    : isolated_app_origin_(isolated_app_origin) {}

bool IsolatedWebAppContentBrowserClient::ShouldUrlUseApplicationIsolationLevel(
    BrowserContext* browser_context,
    const GURL& url) {
  return isolated_app_origin_ == url::Origin::Create(url);
}

std::optional<blink::ParsedPermissionsPolicy>
IsolatedWebAppContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    WebContents* web_contents,
    const url::Origin& app_origin) {
  blink::ParsedPermissionsPolicyDeclaration coi_decl(
      blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
      /*allowed_origins=*/{},
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/true, /*matches_opaque_src=*/false);

  blink::ParsedPermissionsPolicyDeclaration sockets_decl(
      blink::mojom::PermissionsPolicyFeature::kDirectSockets,
      /*allowed_origins=*/{},
      /*self_if_matches=*/app_origin,
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);

  blink::ParsedPermissionsPolicyDeclaration sockets_pna_decl(
      blink::mojom::PermissionsPolicyFeature::kDirectSocketsPrivate,
      /*allowed_origins=*/{},
      /*self_if_matches=*/app_origin,
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);

  return {{coi_decl, sockets_decl, sockets_pna_decl}};
}

// misc
std::string WrapAsync(const std::string& script) {
  DCHECK(!script.empty());
  return base::StringPrintf(R"((async () => {%s})())", script.c_str());
}

}  // namespace content::test
