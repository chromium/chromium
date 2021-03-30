// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace content {

class DirectSocketsUnitTest : public RenderViewHostTestHarness {
 public:
  DirectSocketsUnitTest() {
    feature_list_.InitAndEnableFeature(features::kDirectSockets);
  }
  ~DirectSocketsUnitTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    direct_sockets_service_ =
        std::make_unique<DirectSocketsServiceImpl>(*main_rfh());
    SimulateUserActivation();
  }

  DirectSocketsServiceImpl& direct_sockets_service() {
    return *direct_sockets_service_;
  }

  net::Error ValidateOptions(const blink::mojom::DirectSocketOptions& options) {
    return direct_sockets_service().ValidateOptions(options);
  }

  base::Optional<net::IPEndPoint> GetLocalAddr(
      const blink::mojom::DirectSocketOptions& options) {
    return DirectSocketsServiceImpl::GetLocalAddrForTesting(options);
  }

 private:
  void SimulateUserActivation() {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->UpdateUserActivationState(
            blink::mojom::UserActivationUpdateType::kNotifyActivation,
            blink::mojom::UserActivationNotificationType::kTest);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DirectSocketsServiceImpl> direct_sockets_service_;
};

TEST_F(DirectSocketsUnitTest, RenderFrameDeleted) {
  direct_sockets_service().RenderFrameDeleted(main_rfh());

  blink::mojom::DirectSocketOptions options;
  EXPECT_EQ(ValidateOptions(options), net::ERR_CONTEXT_SHUT_DOWN);
}

TEST_F(DirectSocketsUnitTest, WebContentsDestroyed) {
  direct_sockets_service().WebContentsDestroyed();

  blink::mojom::DirectSocketOptions options;
  EXPECT_EQ(ValidateOptions(options), net::ERR_CONTEXT_SHUT_DOWN);
}

// TODO(crbug.com/1119597): Allow the user to enter the address.
TEST_F(DirectSocketsUnitTest, RemoteAddressCurrentlyRequired) {
// Mark as not enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
#endif

  blink::mojom::DirectSocketOptions options;
  EXPECT_EQ(ValidateOptions(options), net::ERR_NAME_NOT_RESOLVED);
}

TEST_F(DirectSocketsUnitTest, PopulateLocalAddr) {
  blink::mojom::DirectSocketOptions options;

  // Test for default condition.
  base::Optional<net::IPEndPoint> local_addr = GetLocalAddr(options);
  EXPECT_EQ(local_addr, base::nullopt);

  // Test with IPv4 address and default port(0) provided.
  options.local_hostname = "12.34.56.78";
  local_addr = GetLocalAddr(options);
  const uint8_t ipv4[net::IPAddress::kIPv4AddressSize] = {12, 34, 56, 78};
  EXPECT_EQ(local_addr, net::IPEndPoint(net::IPAddress(ipv4), 0));

  // Test with IPv6 address and default port(0) provided.
  options.local_hostname = "fedc:ba98:7654:3210:fedc:ba98:7654:3210";
  local_addr = GetLocalAddr(options);
  const uint8_t ipv6[net::IPAddress::kIPv6AddressSize] = {
      0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
      0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
  EXPECT_EQ(local_addr, net::IPEndPoint(net::IPAddress(ipv6), 0));

  // Test with IPv6 address and port(12345) provided.
  options.local_port = 12345;
  local_addr = GetLocalAddr(options);
  EXPECT_EQ(local_addr, net::IPEndPoint(net::IPAddress(ipv6), 12345));
}

}  // namespace content
