// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler/mime_handler_api.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

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
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace extensions {

namespace {

content::RenderFrameHost* AppendChildFrame(content::WebContents* web_contents,
                                           std::string_view name) {
  content::RenderFrameHost* embedder = web_contents->GetPrimaryMainFrame();
  auto* embedder_tester = content::RenderFrameHostTester::For(embedder);
  embedder_tester->InitializeRenderFrameIfNeeded();
  return embedder_tester->AppendChild(std::string(name));
}

}  // namespace

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
  content::RenderFrameHost* extension_rfh =
      AppendChildFrame(web_contents.get(), "extension");
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
      Extension::GetResourceURL(
          Extension::GetBaseURLFromExtensionId(extension()->id()),
          "handler.html"),
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

// Called without a bound RenderFrameHost -- the function has no way
// to identify the calling extension context.
TEST_F(MimeHandlerApiTest, AbortAndFallbackFailsWithoutRenderFrameHost) {
  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  EXPECT_EQ("Must be called from a web frame.",
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Called from a top-level frame: abortAndFallbackToNativeHandler() is
// only meaningful from a MIME-handler child frame inside an embedder.
TEST_F(MimeHandlerApiTest, AbortAndFallbackFailsFromTopLevelFrame) {
  CreateExtensionPage();
  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  EXPECT_EQ("Must be called from a child frame.",
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Called from a child frame whose embedder has no MIME-handler stream:
// the function reports the missing stream rather than silently no-oping.
TEST_F(MimeHandlerApiTest, AbortAndFallbackFailsWithoutStream) {
  const GURL kOriginalUrl("https://example.com/foo.pdf");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(kOriginalUrl);

  content::RenderFrameHost* extension_rfh =
      AppendChildFrame(web_contents.get(), "extension");
  ASSERT_TRUE(extension_rfh);

  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(extension_rfh);
  EXPECT_EQ("No MIME handler stream for this frame.",
            api_test_utils::RunFunctionAndReturnError(function.get(), "[]",
                                                      browser_context()));
}

// Called from a child frame whose embedder is claimed by a different
// extension: the function rejects the call.
TEST_F(MimeHandlerApiTest, AbortAndFallbackFailsForOtherExtension) {
  const GURL kOriginalUrl("https://example.com/foo.pdf");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(kOriginalUrl);

  content::RenderFrameHost* embedder = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_rfh =
      AppendChildFrame(web_contents.get(), "extension");
  ASSERT_TRUE(extension_rfh);

  mime_handler::MimeHandlerStreamManager::Create(web_contents.get());
  auto* manager = mime_handler::MimeHandlerStreamManager::FromWebContents(
      web_contents.get());
  ASSERT_TRUE(manager);

  // Stream belongs to a different extension.
  constexpr char kOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL("stream://pdf");
  transferrable_loader->head = network::mojom::URLResponseHead::New();
  transferrable_loader->head->mime_type = "application/pdf";
  transferrable_loader->head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  auto stream = std::make_unique<StreamContainer>(
      /*tab_id=*/42, /*embedded=*/true,
      Extension::GetResourceURL(
          Extension::GetBaseURLFromExtensionId(kOtherExtensionId),
          "handler.html"),
      kOtherExtensionId, std::move(transferrable_loader), kOriginalUrl);
  manager->AddStreamContainer(
      embedder->GetFrameTreeNodeId(), "internal_id", std::move(stream),
      std::make_unique<
          testing::NiceMock<mime_handler::MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder);

  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(extension_rfh);
  EXPECT_EQ("Stream does not belong to this extension.",
            api_test_utils::RunFunctionAndReturnError(function.get(), "[]",
                                                      browser_context()));
}

// Successful abort from a third-party handler: the function reports
// no error, and the embedder frame is marked pending native fallback.
TEST_F(MimeHandlerApiTest, AbortAndFallbackSuccess) {
  const GURL kOriginalUrl("https://example.com/foo.pdf");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(kOriginalUrl);

  content::RenderFrameHost* embedder = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_rfh =
      AppendChildFrame(web_contents.get(), "extension");
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
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  auto stream = std::make_unique<StreamContainer>(
      /*tab_id=*/42, /*embedded=*/true,
      Extension::GetResourceURL(
          Extension::GetBaseURLFromExtensionId(extension()->id()),
          "handler.html"),
      extension()->id(), std::move(transferrable_loader), kOriginalUrl);
  manager->AddStreamContainer(
      embedder->GetFrameTreeNodeId(), "internal_id", std::move(stream),
      std::make_unique<
          testing::NiceMock<mime_handler::MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder);

  // `AbortAndFallbackToNativeHandler` CHECKs that the extension frame
  // has finished navigating before the abort lands.
  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder);
  ASSERT_TRUE(stream_info);
  stream_info->SetDidExtensionFinishNavigation();

  const content::FrameTreeNodeId embedder_ftn = embedder->GetFrameTreeNodeId();
  ASSERT_FALSE(manager->IsPendingNativeFallback(embedder_ftn));

  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(extension_rfh);
  EXPECT_TRUE(
      api_test_utils::RunFunction(function.get(), "[]", browser_context()));
  EXPECT_TRUE(function->GetError().empty()) << function->GetError();
  EXPECT_TRUE(manager->IsPendingNativeFallback(embedder_ftn));
}

// Built-in MIME handler extensions (e.g. the PDF viewer) are blocked
// from calling abortAndFallbackToNativeHandler -- the API is only
// meaningful for generic third-party handlers that want to fall back
// to the user agent's native handler.
TEST_F(MimeHandlerApiTest, AbortAndFallbackRejectsBuiltInExtension) {
  const std::vector<ExtensionId>& allowlist =
      MimeTypesHandler::GetMIMETypeAllowlist();
  ASSERT_FALSE(allowlist.empty());

  set_extension(
      ExtensionBuilder("Built-in MIME handler").SetID(allowlist[0]).Build());

  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  std::string error = RunFunctionAndReturnError(function.get(), "[]");
  EXPECT_EQ(
      "abortAndFallbackToNativeHandler is not available "
      "for built-in MIME handler extensions.",
      error);
}

}  // namespace extensions
