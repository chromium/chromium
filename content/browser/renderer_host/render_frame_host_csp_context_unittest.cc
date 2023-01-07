// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_csp_context.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/source_location.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(RenderFrameHostCSPContextTest, SanitizeDataForUseInCspViolation) {
  RenderFrameHostCSPContext context(/*render_frame_host=*/nullptr);

  GURL blocked_url("http://a.com/login?password=1234");
  auto source_location =
      network::mojom::SourceLocation::New("http://a.com/login", 10u, 20u);

  context.SanitizeDataForUseInCspViolation(
      network::mojom::CSPDirectiveName::FencedFrameSrc, &blocked_url,
      source_location.get());

  EXPECT_EQ(blocked_url, blocked_url.DeprecatedGetOriginAsURL());
  EXPECT_EQ(source_location->url, "http://a.com/");
  EXPECT_EQ(source_location->line, 0u);
  EXPECT_EQ(source_location->column, 0u);
}

}  // namespace content
