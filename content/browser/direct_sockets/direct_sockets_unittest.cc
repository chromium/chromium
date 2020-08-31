// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

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
  }

  DirectSocketsServiceImpl& direct_sockets_service() {
    return *direct_sockets_service_;
  }

  net::Error EnsurePermission(
      const blink::mojom::DirectSocketOptions& options) {
    return direct_sockets_service().EnsurePermission(options);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DirectSocketsServiceImpl> direct_sockets_service_;
};

TEST_F(DirectSocketsUnitTest, RenderFrameDeleted) {
  direct_sockets_service().RenderFrameDeleted(main_rfh());

  blink::mojom::DirectSocketOptions options;
  EXPECT_EQ(EnsurePermission(options), net::ERR_CONTEXT_SHUT_DOWN);
}

TEST_F(DirectSocketsUnitTest, WebContentsDestroyed) {
  direct_sockets_service().WebContentsDestroyed();

  blink::mojom::DirectSocketOptions options;
  EXPECT_EQ(EnsurePermission(options), net::ERR_CONTEXT_SHUT_DOWN);
}

}  // namespace content
