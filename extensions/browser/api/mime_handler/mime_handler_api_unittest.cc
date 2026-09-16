// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler/mime_handler_api.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/mime_handler/generic_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"
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

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kOriginalUrl[] = "https://example.com/foo.pdf";
constexpr char kStreamUrl[] = "stream://pdf";
constexpr char kHandlerPage[] = "handler.html";
constexpr char kCustomHeaderName[] = "X-Custom";
constexpr char kCustomHeaderValue[] = "bar";
constexpr char kCoepHeaderName[] = "Cross-Origin-Embedder-Policy";
constexpr char kCoepHeaderValue[] = "require-corp";
constexpr char kOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr int kTabId = 42;

content::RenderFrameHost* AppendChildFrame(content::WebContents* web_contents,
                                           std::string_view name) {
  content::RenderFrameHost* embedder = web_contents->GetPrimaryMainFrame();
  auto* embedder_tester = content::RenderFrameHostTester::For(embedder);
  embedder_tester->InitializeRenderFrameIfNeeded();
  return embedder_tester->AppendChild(std::string(name));
}

// A PDF response head. `extra_headers` lets a test add the header names it
// wants the API to expose or hide.
network::mojom::URLResponseHeadPtr CreatePdfResponseHeadWithExtraHeaders(
    const std::map<std::string, std::string>& extra_headers) {
  auto head = network::mojom::URLResponseHead::New();
  head->mime_type = kPdfMimeType;
  auto builder =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK");
  builder.AddHeader("Content-Type", kPdfMimeType);
  for (const auto& [name, value] : extra_headers) {
    builder.AddHeader(name, value);
  }
  head->headers = builder.Build();
  return head;
}

// A claimed MIME handler stream and the frames a chrome.mimeHandler API
// function needs to run against it.
struct ClaimedStreamSetup {
  // Owns the frames below, so it must outlive them.
  std::unique_ptr<content::WebContents> web_contents;
  // Holds the stream. The API finds it from the extension frame's parent.
  raw_ptr<content::RenderFrameHost> embedder;
  // Stands in for the handler page that calls the API.
  raw_ptr<content::RenderFrameHost> extension_rfh;
  raw_ptr<mime_handler::MimeHandlerStreamManager> manager;
  // The URL the stream was intercepted for.
  GURL original_url;
};

ClaimedStreamSetup CreateAndSetUpClaimedStream(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    network::mojom::URLResponseHeadPtr response_head,
    std::unique_ptr<MimeHandlerStreamDelegate> delegate) {
  ClaimedStreamSetup result;
  result.original_url = GURL(kOriginalUrl);
  const GURL& original_url = result.original_url;
  result.web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context, content::SiteInstance::Create(browser_context));
  content::WebContentsTester::For(result.web_contents.get())
      ->NavigateAndCommit(original_url);
  result.embedder = result.web_contents->GetPrimaryMainFrame();
  result.extension_rfh =
      AppendChildFrame(result.web_contents.get(), "extension");

  mime_handler::MimeHandlerStreamManager::Create(result.web_contents.get());
  result.manager = mime_handler::MimeHandlerStreamManager::FromWebContents(
      result.web_contents.get());

  auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL(kStreamUrl);
  transferrable_loader->head = std::move(response_head);
  auto stream = std::make_unique<StreamContainer>(
      kTabId, /*embedded=*/true,
      Extension::GetResourceURL(
          Extension::GetBaseURLFromExtensionId(extension_id), kHandlerPage),
      extension_id, std::move(transferrable_loader), original_url);
  result.manager->AddStreamContainer(result.embedder->GetFrameTreeNodeId(),
                                     "internal_id", std::move(stream),
                                     std::move(delegate));
  result.manager->ClaimStreamInfoForTesting(result.embedder);
  return result;
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
  ClaimedStreamSetup setup = CreateAndSetUpClaimedStream(
      browser_context(), extension()->id(),
      CreatePdfResponseHeadWithExtraHeaders(
          {{kCustomHeaderName, kCustomHeaderValue}}),
      std::make_unique<
          testing::NiceMock<mime_handler::MockMimeHandlerStreamDelegate>>());

  auto function = base::MakeRefCounted<MimeHandlerGetStreamInfoFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(setup.extension_rfh);
  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       browser_context());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  const base::DictValue& info = result->GetDict();
  EXPECT_EQ(*info.FindString("mimeType"), kPdfMimeType);
  EXPECT_EQ(*info.FindString("originalUrl"), kOriginalUrl);
  EXPECT_EQ(*info.FindString("streamUrl"), kStreamUrl);
  EXPECT_EQ(info.FindInt("tabId"), kTabId);
  EXPECT_EQ(info.FindBool("embedded"), true);
  const base::DictValue* headers = info.FindDict("responseHeaders");
  ASSERT_TRUE(headers);
  EXPECT_EQ(*headers->FindString("Content-Type"), kPdfMimeType);
  EXPECT_EQ(*headers->FindString(kCustomHeaderName), kCustomHeaderValue);
}

// A generic (third-party) handler sees only the CORS-safelisted response
// header names through getStreamInfo().
TEST_F(MimeHandlerApiTest, GetStreamInfoFiltersHeadersForGenericHandler) {
  ClaimedStreamSetup setup = CreateAndSetUpClaimedStream(
      browser_context(), extension()->id(),
      CreatePdfResponseHeadWithExtraHeaders(
          {{kCoepHeaderName, kCoepHeaderValue}}),
      std::make_unique<mime_handler::GenericMimeHandlerStreamDelegate>());

  auto function = base::MakeRefCounted<MimeHandlerGetStreamInfoFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(setup.extension_rfh);
  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       browser_context());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  const base::DictValue* headers =
      result->GetDict().FindDict("responseHeaders");
  ASSERT_TRUE(headers);
  EXPECT_TRUE(headers->FindString("Content-Type"));
  EXPECT_FALSE(headers->FindString(kCoepHeaderName));
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
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL(kOriginalUrl));

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
  // Stream belongs to a different extension.
  ClaimedStreamSetup setup = CreateAndSetUpClaimedStream(
      browser_context(), kOtherExtensionId,
      CreatePdfResponseHeadWithExtraHeaders(
          {{kCoepHeaderName, kCoepHeaderValue}}),
      std::make_unique<
          testing::NiceMock<mime_handler::MockMimeHandlerStreamDelegate>>());

  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(setup.extension_rfh);
  EXPECT_EQ("Stream does not belong to this extension.",
            api_test_utils::RunFunctionAndReturnError(function.get(), "[]",
                                                      browser_context()));
}

// Successful abort from a third-party handler: the function reports
// no error, and the embedder frame is marked pending native fallback.
TEST_F(MimeHandlerApiTest, AbortAndFallbackSuccess) {
  ClaimedStreamSetup setup = CreateAndSetUpClaimedStream(
      browser_context(), extension()->id(),
      CreatePdfResponseHeadWithExtraHeaders(
          {{kCoepHeaderName, kCoepHeaderValue}}),
      std::make_unique<
          testing::NiceMock<mime_handler::MockMimeHandlerStreamDelegate>>());

  // `AbortAndFallbackToNativeHandler` CHECKs that the extension frame
  // has finished navigating before the abort lands.
  auto* stream_info =
      setup.manager->GetClaimedStreamInfoForTesting(setup.embedder);
  ASSERT_TRUE(stream_info);
  stream_info->SetDidExtensionFinishNavigation();

  const content::FrameTreeNodeId embedder_ftn =
      setup.embedder->GetFrameTreeNodeId();
  ASSERT_FALSE(
      setup.manager->IsPendingNativeFallback(embedder_ftn, setup.original_url));

  auto function = base::MakeRefCounted<
      MimeHandlerAbortAndFallbackToNativeHandlerFunction>();
  function->set_extension(extension());
  function->SetRenderFrameHost(setup.extension_rfh);
  EXPECT_TRUE(
      api_test_utils::RunFunction(function.get(), "[]", browser_context()));
  EXPECT_TRUE(function->GetError().empty()) << function->GetError();
  EXPECT_TRUE(
      setup.manager->IsPendingNativeFallback(embedder_ftn, setup.original_url));
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

TEST_F(MimeHandlerApiTest, GetMimeHandlerOptionsDefaultsToEnabled) {
  // With no prior setMimeHandlerOptions call, getMimeHandlerOptions
  // must return enabled=true by default.
  auto function =
      base::MakeRefCounted<MimeHandlerGetMimeHandlerOptionsFunction>();
  std::optional<base::Value> result =
      RunFunctionAndReturnValue(function.get(), R"(["application/pdf"])");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, base::test::IsJson(R"({"enabled": true})"));
}

TEST_F(MimeHandlerApiTest, SetThenGetMimeHandlerOptionsRoundTrip) {
  // Persist enabled=false via setMimeHandlerOptions.
  auto set_function =
      base::MakeRefCounted<MimeHandlerSetMimeHandlerOptionsFunction>();
  RunFunction(set_function.get(), R"(["application/pdf", {"enabled": false}])");

  // Read it back via getMimeHandlerOptions and verify the value survived.
  auto get_function =
      base::MakeRefCounted<MimeHandlerGetMimeHandlerOptionsFunction>();
  std::optional<base::Value> result =
      RunFunctionAndReturnValue(get_function.get(), R"(["application/pdf"])");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, base::test::IsJson(R"({"enabled": false})"));
}

}  // namespace extensions
