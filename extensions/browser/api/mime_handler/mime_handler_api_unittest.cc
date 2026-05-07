// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler/mime_handler_api.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/browser/mime_handler/mock_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace extensions {

using MimeHandlerApiTest = ApiUnitTest;

// Called without any render frame host bound to the function: the function
// cannot know which extension context is calling it.
TEST_F(MimeHandlerApiTest, GetStreamInfoFailsWithoutRenderFrameHost) {
  auto function = base::MakeRefCounted<MimeHandlerGetStreamInfoFunction>();
  EXPECT_EQ("Must be called from a web frame.",
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Called from a top-level frame: getStreamInfo() is only meaningful from a
// MIME-handler child frame hosted inside an embedder.
TEST_F(MimeHandlerApiTest, GetStreamInfoFailsFromTopLevelFrame) {
  CreateExtensionPage();
  auto function = base::MakeRefCounted<MimeHandlerGetStreamInfoFunction>();
  EXPECT_EQ("Must be called from a child frame.",
            RunFunctionAndReturnError(function.get(), "[]"));
}

TEST_F(MimeHandlerApiTest, GetStreamInfoSuccess) {
  const GURL kOriginalUrl("https://example.com/foo.pdf");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(kOriginalUrl);

  content::RenderFrameHost* embedder = web_contents->GetPrimaryMainFrame();
  auto* embedder_tester = content::RenderFrameHostTester::For(embedder);
  embedder_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* extension_rfh =
      embedder_tester->AppendChild("extension");
  ASSERT_TRUE(extension_rfh);

  mime_handler::MimeHandlerStreamManager::Create(web_contents.get());
  auto* manager = mime_handler::MimeHandlerStreamManager::FromWebContents(
      web_contents.get());
  ASSERT_TRUE(manager);

  auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL("stream://pdf");
  transferrable_loader->head = network::mojom::URLResponseHead::New();
  transferrable_loader->head->mime_type = "application/pdf";
  transferrable_loader->head->headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
          .AddHeader("Content-Type", "application/pdf")
          .AddHeader("X-Custom", "bar")
          .Build();

  auto stream = std::make_unique<StreamContainer>(
      /*tab_id=*/42, /*embedded=*/true,
      GURL("chrome-extension://" + extension()->id() + "/handler.html"),
      extension()->id(), std::move(transferrable_loader), kOriginalUrl);

  manager->AddStreamContainer(
      embedder->GetFrameTreeNodeId(), "internal_id", std::move(stream),
      std::make_unique<
          testing::NiceMock<mime_handler::MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder);

  auto function = base::MakeRefCounted<MimeHandlerGetStreamInfoFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(extension_rfh);
  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       browser_context());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  const base::DictValue& info = result->GetDict();
  EXPECT_EQ(*info.FindString("mimeType"), "application/pdf");
  EXPECT_EQ(*info.FindString("originalUrl"), "https://example.com/foo.pdf");
  EXPECT_EQ(*info.FindString("streamUrl"), "stream://pdf");
  EXPECT_EQ(info.FindInt("tabId"), 42);
  EXPECT_EQ(info.FindBool("embedded"), true);
  const base::DictValue* headers = info.FindDict("responseHeaders");
  ASSERT_TRUE(headers);
  EXPECT_EQ(*headers->FindString("Content-Type"), "application/pdf");
  EXPECT_EQ(*headers->FindString("X-Custom"), "bar");
}

}  // namespace extensions
