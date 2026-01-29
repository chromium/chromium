// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/document_metadata.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class DocumentMetadataBrowserTest : public ContentBrowserTest {
 public:
  DocumentMetadataBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(DocumentMetadataBrowserTest, RetrieveLoginActions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a page with JSON-LD.
  std::string html_content = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Test Page</title>
      <script type="application/ld+json">
      {
        "@context": "https://schema.org",
        "@type": "LoginAction",
        "federation": {
          "providers": [{
            "@type": "FederatedLoginProvider",
            "configURL": "https://idp.example/config.json",
            "clientId": "1234",
            "nonce": "4567",
            "fields": ["email", "name", "picture"]
          }]
        }
      }
      </script>
    </head>
    <body>
      <h1>Login to Test Page</h1>
    </body>
    </html>
  )";

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html," + html_content)));

  webid::DocumentMetadata metadata(
      shell()->web_contents()->GetPrimaryMainFrame());

  base::test::TestFuture<
      std::vector<blink::mojom::IdentityProviderGetParametersPtr>>
      future;
  metadata.GetLoginActions(future.GetCallback());

  auto actions = future.Take();
  ASSERT_EQ(actions.size(), 1u);
  const auto& params = actions[0];
  ASSERT_EQ(params->providers.size(), 1u);
  const auto& provider = params->providers[0];

  EXPECT_EQ(provider->config->config_url,
            GURL("https://idp.example/config.json"));
  EXPECT_EQ(provider->config->client_id, "1234");
  EXPECT_EQ(provider->nonce, "4567");
  ASSERT_TRUE(provider->fields.has_value());
  EXPECT_THAT(*provider->fields,
              testing::ElementsAre("email", "name", "picture"));
}

}  // namespace content
