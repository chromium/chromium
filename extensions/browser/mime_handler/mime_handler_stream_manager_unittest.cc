// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"
#include "extensions/browser/mime_handler/mock_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/browser/mime_handler/stream_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace extensions::mime_handler {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

constexpr char kOriginalUrl1[] = "https://original_url1";
constexpr char kOriginalUrl2[] = "https://original_url2";

}  // namespace

class MimeHandlerStreamManagerTest : public content::RenderViewHostTestHarness {
 protected:
  void TearDown() override {
    content::RenderViewHostTestHarness::web_contents()->RemoveUserData(
        MimeHandlerStreamManager::UserDataKey());
    content::RenderViewHostTestHarness::TearDown();
  }

  MimeHandlerStreamManager* mime_handler_stream_manager() {
    return MimeHandlerStreamManager::FromWebContents(
        content::RenderViewHostTestHarness::web_contents());
  }

  // Simulate a navigation and commit on `host`. The last committed URL will be
  // `original_url`.
  content::RenderFrameHost* NavigateAndCommit(content::RenderFrameHost* host,
                                              const GURL& original_url) {
    content::RenderFrameHost* new_host =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            original_url, host);

    // Create `MimeHandlerStreamManager` if it doesn't exist already. If `host`
    // is the primary main frame, then the previous `MimeHandlerStreamManager`
    // may have been deleted as part of the above navigation.
    MimeHandlerStreamManager::Create(
        content::RenderViewHostTestHarness::web_contents());
    return new_host;
  }

  content::RenderFrameHost* CreateChildRenderFrameHost(
      content::RenderFrameHost* parent_host,
      const std::string& frame_name) {
    auto* parent_host_tester = content::RenderFrameHostTester::For(parent_host);
    parent_host_tester->InitializeRenderFrameIfNeeded();
    return parent_host_tester->AppendChild(frame_name);
  }
};

// Verify adding and getting an `extensions::StreamContainer`.
TEST_F(MimeHandlerStreamManagerTest, AddAndGetStreamContainer) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  content::FrameTreeNodeId frame_tree_node_id =
      embedder_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      frame_tree_node_id, "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  EXPECT_TRUE(manager->ContainsUnclaimedStreamInfo(frame_tree_node_id));
  manager->ClaimStreamInfoForTesting(embedder_host);

  base::WeakPtr<extensions::StreamContainer> result =
      manager->GetStreamContainer(embedder_host);

  ASSERT_TRUE(result);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 1);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url1"));
  EXPECT_EQ(result->extension_id(), "extension_id1");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url1"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url1"));
  EXPECT_TRUE(mime_handler_stream_manager());
}

// Verify adding an `extensions::StreamContainer` under the same frame tree node
// ID replaces the original unclaimed `extensions::StreamContainer`.
TEST_F(MimeHandlerStreamManagerTest,
       AddStreamContainerSameFrameTreeNodeIdUnclaimed) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl2));
  content::FrameTreeNodeId frame_tree_node_id =
      embedder_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      frame_tree_node_id, "internal_id1",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->AddStreamContainer(
      frame_tree_node_id, "internal_id2",
      extensions::mime_handler::GenerateSampleStreamContainer(2),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);

  base::WeakPtr<extensions::StreamContainer> result =
      manager->GetStreamContainer(main_rfh());

  ASSERT_TRUE(result);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 2);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url2"));
  EXPECT_EQ(result->extension_id(), "extension_id2");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url2"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url2"));
  EXPECT_TRUE(mime_handler_stream_manager());
}

// Verify getting a `StreamContainer` with a non-matching URL returns nullptr;
TEST_F(MimeHandlerStreamManagerTest, AddAndGetStreamInvalidURL) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL("https://nonmatching_url"));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);

  EXPECT_FALSE(manager->GetStreamContainer(embedder_host));
  EXPECT_TRUE(mime_handler_stream_manager());
}

// Verify adding multiple `extensions::StreamContainer`s for different
// FrameTreeNodes at once.
TEST_F(MimeHandlerStreamManagerTest, AddMultipleStreamContainers) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* child_host = CreateChildRenderFrameHost(embedder_host, "child host");
  child_host = NavigateAndCommit(child_host, GURL(kOriginalUrl2));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id1",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->AddStreamContainer(
      child_host->GetFrameTreeNodeId(), "internal_id2",
      extensions::mime_handler::GenerateSampleStreamContainer(2),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->ClaimStreamInfoForTesting(child_host);

  base::WeakPtr<extensions::StreamContainer> result =
      manager->GetStreamContainer(embedder_host);

  ASSERT_TRUE(result);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 1);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url1"));
  EXPECT_EQ(result->extension_id(), "extension_id1");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url1"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url1"));

  result = manager->GetStreamContainer(child_host);

  ASSERT_TRUE(result);
  transferrable_loader = result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 2);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url2"));
  EXPECT_EQ(result->extension_id(), "extension_id2");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url2"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url2"));
  EXPECT_TRUE(mime_handler_stream_manager());
}

// `MimeHandlerStreamManager::IsExtensionHost()` should correctly identify
// the extension hosts.
TEST_F(MimeHandlerStreamManagerTest, IsExtensionHost) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));

  // During a load, there's an RFH for the extension frame for the initial
  // about:blank navigation. This RFH will always be replaced by
  // `extension_host`.
  auto* about_blank_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* other_host = CreateChildRenderFrameHost(embedder_host, "other host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `about_blank_host` and `extension_host` should have the same frame tree
  // node ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the extension frame tree node ID to the
  // initial RFH.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, about_blank_host->GetFrameTreeNodeId());

  // `about_blank_host` should be considered an extension host, even if it isn't
  // navigating to the original URL.
  EXPECT_TRUE(manager->IsExtensionHost(about_blank_host));

  // Now, set the extension frame tree node ID to the actual extension frame
  // tree node ID, which will be the same ID as `about_blank_host` in real
  // situations.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  EXPECT_TRUE(manager->IsExtensionHost(extension_host));

  // Unrelated hosts shouldn't be considered extension hosts.
  EXPECT_FALSE(manager->IsExtensionHost(other_host));
}

// `MimeHandlerStreamManager::IsContentHost()` should correctly identify the
// content hosts.
TEST_F(MimeHandlerStreamManagerTest, IsContentHost) {
  const GURL pdf_url = GURL(kOriginalUrl1);

  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, pdf_url);
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  // During a load, there's an RFH for the content frame for the initial
  // stream URL navigation. This RFH will always be replaced by
  // `content_host`.
  auto* stream_url_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  content_host = NavigateAndCommit(content_host, pdf_url);
  auto* other_host = CreateChildRenderFrameHost(extension_host, "other host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `stream_url_host` and `content_host` should have the same frame tree node
  // ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the content frame tree node ID to the
  // initial RFH.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, stream_url_host->GetFrameTreeNodeId());

  // `stream_url_host` should be considered a content host, even if it isn't
  // navigating to the original URL.
  EXPECT_TRUE(manager->IsContentHost(stream_url_host));

  // Now, set the content frame tree node ID to the actual content frame tree
  // node ID, which will be the same as `stream_url_host` in real situations.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, content_host->GetFrameTreeNodeId());

  EXPECT_TRUE(manager->IsContentHost(content_host));

  // Unrelated hosts shouldn't be considered content hosts.
  EXPECT_FALSE(manager->IsContentHost(other_host));
}

// If multiple `extensions::StreamContainer`s exist, then deleting one stream
// shouldn't delete the other stream.
TEST_F(MimeHandlerStreamManagerTest, DeleteWithMultipleStreamContainers) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* child_host = CreateChildRenderFrameHost(embedder_host, "child host");
  child_host = NavigateAndCommit(child_host, GURL(kOriginalUrl2));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id1",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->AddStreamContainer(
      child_host->GetFrameTreeNodeId(), "internal_id2",
      extensions::mime_handler::GenerateSampleStreamContainer(2),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->ClaimStreamInfoForTesting(child_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));
  ASSERT_TRUE(manager->GetStreamContainer(child_host));

  // `MimeHandlerStreamManager::RenderFrameDeleted()` should cause the stream
  // associated with `child_host` to be deleted.
  manager->RenderFrameDeleted(child_host);

  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
  EXPECT_FALSE(manager->GetStreamContainer(child_host));
  EXPECT_TRUE(mime_handler_stream_manager());
}

// Verify that unclaimed stream infos can be deleted.
TEST_F(MimeHandlerStreamManagerTest, DeleteUnclaimedStreamInfo) {
  content::RenderFrameHost* unclaimed_embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  content::FrameTreeNodeId frame_tree_node_id =
      unclaimed_embedder_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      frame_tree_node_id, "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  EXPECT_FALSE(manager->GetStreamContainer(unclaimed_embedder_host));

  manager->DeleteUnclaimedStreamInfo(frame_tree_node_id);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

// If the embedder render frame is deleted, the stream should be deleted.
TEST_F(MimeHandlerStreamManagerTest, RenderFrameDeletedWithClaimedStream) {
  auto* actual_host = CreateChildRenderFrameHost(main_rfh(), "actual host");
  actual_host = NavigateAndCommit(actual_host, GURL(kOriginalUrl1));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      actual_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(actual_host);
  ASSERT_TRUE(manager->GetStreamContainer(actual_host));

  // Unrelated hosts should be ignored.
  manager->RenderFrameDeleted(main_rfh());
  ASSERT_EQ(manager, mime_handler_stream_manager());

  // `MimeHandlerStreamManager::RenderFrameDeleted()` should cause the stream
  // associated with `actual_host` to be deleted.
  manager->RenderFrameDeleted(actual_host);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

TEST_F(MimeHandlerStreamManagerTest, RenderFrameDeletedWithUnclaimedStream) {
  auto* actual_host = CreateChildRenderFrameHost(main_rfh(), "actual host");
  actual_host = NavigateAndCommit(actual_host, GURL(kOriginalUrl1));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      actual_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());

  // The stream hasn't been claimed, so the stream container can't be retrieved.
  ASSERT_FALSE(manager->GetStreamContainer(actual_host));

  // Unrelated hosts should be ignored.
  manager->RenderFrameDeleted(main_rfh());
  ASSERT_EQ(manager, mime_handler_stream_manager());

  // `MimeHandlerStreamManager::RenderFrameDeleted()` should cause the stream
  // associated with `actual_host` to be deleted.
  manager->RenderFrameDeleted(actual_host);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

// If the `content::RenderFrameHost` for the stream changes, then the stream
// should be deleted.
TEST_F(MimeHandlerStreamManagerTest, EmbedderRenderFrameHostChanged) {
  content::RenderFrameHost* old_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* new_host = CreateChildRenderFrameHost(old_host, "new host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      old_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(old_host);
  ASSERT_TRUE(manager->GetStreamContainer(old_host));

  // If the first parameter to RenderFrameHostChanged() is null, then it means a
  // subframe is being created and should be ignored.
  manager->RenderFrameHostChanged(nullptr, old_host);
  EXPECT_TRUE(manager->GetStreamContainer(old_host));

  // Unrelated hosts should be ignored.
  manager->RenderFrameHostChanged(new_host, new_host);
  EXPECT_TRUE(manager->GetStreamContainer(old_host));

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  manager->RenderFrameHostChanged(old_host, new_host);
  EXPECT_FALSE(mime_handler_stream_manager());
}

// If the extension host changes to a different host, the stream should be
// deleted.
TEST_F(MimeHandlerStreamManagerTest, ExtensionRenderFrameHostChanged) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));

  // During a load, there's an RFH for the extension frame for the initial
  // about:blank navigation. This RFH will always be replaced by
  // `extension_host` and shouldn't trigger stream deletion. Both hosts should
  // share the same frame name.
  auto* about_blank_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  about_blank_host = NavigateAndCommit(about_blank_host, GURL("about:blank"));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* new_host = CreateChildRenderFrameHost(embedder_host, "new host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `about_blank_host` and `extension_host` should have the same frame tree
  // node ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the extension frame tree node ID to the
  // initial RFH.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, about_blank_host->GetFrameTreeNodeId());

  // Changing `about_blank_host` to `extension_host` shouldn't delete the
  // stream.
  manager->RenderFrameHostChanged(about_blank_host, extension_host);

  ASSERT_TRUE(mime_handler_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));

  // Now, set the extension frame tree node ID to the actual extension frame
  // tree node ID, which will be the same ID as `about_blank_host` in real
  // situations.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  // Changing the extension host should delete the stream.
  manager->RenderFrameHostChanged(extension_host, new_host);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

// If the content host changes to a different host, the stream should be
// deleted.
TEST_F(MimeHandlerStreamManagerTest, ContentRenderFrameHostChanged) {
  const GURL pdf_url = GURL(kOriginalUrl1);

  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, pdf_url);
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  // During a load, there's an RFH for the content frame for the initial
  // stream URL navigation. This RFH will always be replaced by
  // `content_host` and shouldn't trigger stream deletion.
  auto* stream_url_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  content_host = NavigateAndCommit(content_host, pdf_url);
  auto* new_host = CreateChildRenderFrameHost(extension_host, "new host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // The extension host needs to have the PDF extension origin so that
  // `IsContentHost()` can walk up to the embedder via the extension frame.
  content::OverrideLastCommittedOrigin(
      extension_host,
      url::Origin::Create(GURL(
          "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html")));

  // `stream_url_host` and `content_host` should have the same frame tree node
  // ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the content frame tree node ID to the
  // initial RFH.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, stream_url_host->GetFrameTreeNodeId());

  // Changing `stream_url_host` to `content_host` shouldn't delete the stream.
  manager->RenderFrameHostChanged(stream_url_host, content_host);

  ASSERT_TRUE(mime_handler_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));

  // Now, set the content frame tree node ID to the actual content frame tree
  // node ID, which will be the same as `stream_url_host` in real situations.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, content_host->GetFrameTreeNodeId());

  // Changing the content host should delete the stream.
  manager->RenderFrameHostChanged(content_host, new_host);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

TEST_F(MimeHandlerStreamManagerTest,
       ContentHostChangedCallsValidateContentFrameHost) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  auto* new_host = CreateChildRenderFrameHost(extension_host, "new host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, content_host->GetFrameTreeNodeId());

  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(stream_info);

  EXPECT_CALL(*delegate_ptr,
              ValidateContentFrameHost(content_host, stream_info));

  // Trigger content host change. The content host has an empty last committed
  // URL, so the stream should NOT be deleted (initial RFH changes are ignored).
  manager->RenderFrameHostChanged(content_host, new_host);

  ASSERT_TRUE(mime_handler_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
}

// If the `content::RenderFrameHost` for the stream is deleted, then the stream
// should be deleted.
TEST_F(MimeHandlerStreamManagerTest, EmbedderFrameDeleted) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  content::FrameTreeNodeId frame_tree_node_id =
      embedder_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      frame_tree_node_id, "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  manager->FrameDeleted(frame_tree_node_id);
  EXPECT_FALSE(mime_handler_stream_manager());
}

// If the extension frame is deleted, the stream should be deleted.
TEST_F(MimeHandlerStreamManagerTest, ExtensionFrameDeleted) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "actual host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  content::FrameTreeNodeId frame_tree_node_id =
      extension_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Set the extension frame tree node ID so the stream can be deleted when the
  // extension host is deleted.
  manager->SetExtensionFrameTreeNodeIdForTesting(embedder_host,
                                                 frame_tree_node_id);

  // Deleting the extension host should cause the stream to be deleted.
  manager->FrameDeleted(frame_tree_node_id);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

// If the content frame is deleted, the stream should be deleted.
TEST_F(MimeHandlerStreamManagerTest, ContentFrameDeleted) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  content::FrameTreeNodeId frame_tree_node_id =
      content_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Set the content frame tree node ID so the stream can be deleted when the
  // content host is deleted.
  manager->SetContentFrameTreeNodeIdForTesting(embedder_host,
                                               frame_tree_node_id);

  // Deleting the content host should cause the stream to be deleted.
  manager->FrameDeleted(frame_tree_node_id);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

// The initial load should claim the stream.
TEST_F(MimeHandlerStreamManagerTest,
       ReadyToCommitNavigationCallsOnStreamClaimed) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  MimeHandlerStreamManager* manager = mime_handler_stream_manager();

  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  extensions::StreamInfo* captured_stream_info = nullptr;
  EXPECT_CALL(*delegate_ptr, OnStreamClaimed(embedder_host, _))
      .WillOnce(SaveArg<1>(&captured_stream_info));
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));

  NiceMock<content::MockNavigationHandle> navigation_handle;
  navigation_handle.set_render_frame_host(embedder_host);
  manager->ReadyToCommitNavigation(&navigation_handle);

  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(stream_info);
  EXPECT_EQ(stream_info, captured_stream_info);
}

TEST_F(MimeHandlerStreamManagerTest,
       OnStreamClaimedNotCalledForUnrelatedNavigation) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* unrelated_host =
      CreateChildRenderFrameHost(embedder_host, "unrelated host");
  unrelated_host = NavigateAndCommit(unrelated_host, GURL("https://unrelated"));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr, OnStreamClaimed(_, _)).Times(0);
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));

  NiceMock<content::MockNavigationHandle> navigation_handle;
  navigation_handle.set_render_frame_host(unrelated_host);
  manager->ReadyToCommitNavigation(&navigation_handle);

  EXPECT_TRUE(manager->ContainsUnclaimedStreamInfo(
      embedder_host->GetFrameTreeNodeId()));
  EXPECT_FALSE(manager->GetClaimedStreamInfoForTesting(embedder_host));
}

TEST_F(MimeHandlerStreamManagerTest, ReadyToCommitNavigationClaimAndReplace) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  EXPECT_FALSE(manager->GetStreamContainer(embedder_host));

  NiceMock<content::MockNavigationHandle> navigation_handle1;
  navigation_handle1.set_render_frame_host(embedder_host);

  // The initial load should cause the embedder host to claim the stream.
  manager->ReadyToCommitNavigation(&navigation_handle1);
  base::WeakPtr<extensions::StreamContainer> original_stream =
      manager->GetStreamContainer(embedder_host);
  EXPECT_TRUE(original_stream);
  EXPECT_TRUE(mime_handler_stream_manager());

  NiceMock<content::MockNavigationHandle> navigation_handle2;
  navigation_handle2.set_render_frame_host(embedder_host);

  // Committing a navigation again shouldn't try to claim a stream again if
  // there isn't a new stream. The stream should remain the same. This can occur
  // if a page contains an embed to a handled URL, and the embed later navigates
  // to another URL.
  manager->ReadyToCommitNavigation(&navigation_handle2);
  base::WeakPtr<extensions::StreamContainer> same_stream =
      manager->GetStreamContainer(embedder_host);
  ASSERT_TRUE(original_stream);
  ASSERT_TRUE(same_stream);
  EXPECT_EQ(original_stream.get(), same_stream.get());
  EXPECT_TRUE(mime_handler_stream_manager());

  // Re-add a duplicate stream.
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());

  NiceMock<content::MockNavigationHandle> navigation_handle3;
  navigation_handle3.set_render_frame_host(embedder_host);

  // If a new stream exists for the same frame tree node ID, allow claiming the
  // new stream. This can occur if a full page MIME handler refreshes.
  manager->ReadyToCommitNavigation(&navigation_handle3);
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
  EXPECT_FALSE(original_stream);
  EXPECT_TRUE(mime_handler_stream_manager());
}

TEST_F(MimeHandlerStreamManagerTest,
       DidFinishNavigationDelegateExtensionFinished) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(stream_info);

  NiceMock<content::MockNavigationHandle> navigation_handle(
      stream_info->stream()->handler_url(), extension_host);
  navigation_handle.set_has_committed(true);

  EXPECT_CALL(*delegate_ptr,
              OnExtensionFrameFinished(&navigation_handle, stream_info));
  manager->DidFinishNavigation(&navigation_handle);
}

class MimeHandlerStreamManagerPostMessageTest
    : public MimeHandlerStreamManagerTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(MimeHandlerStreamManagerPostMessageTest,
       OnPostMessageSetUpFiresWhenDelegateOptsIn) {
  const bool should_set_up_post_message = GetParam();

  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, content_host->GetFrameTreeNodeId());

  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(stream_info);

  NiceMock<content::MockNavigationHandle> navigation_handle(
      stream_info->stream()->original_url(), content_host);

  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(true));
  ON_CALL(*delegate_ptr, ShouldSetUpPostMessage())
      .WillByDefault(Return(should_set_up_post_message));
  EXPECT_CALL(*delegate_ptr, OnPostMessageSetUp(_))
      .Times(should_set_up_post_message ? 1 : 0);

  manager->DidFinishNavigation(&navigation_handle);
}

INSTANTIATE_TEST_SUITE_P(ShouldSetUpPostMessage,
                         MimeHandlerStreamManagerPostMessageTest,
                         ::testing::Bool());

TEST_F(MimeHandlerStreamManagerTest,
       DidFinishNavigationDelegateExtensionFinishedIgnoresNonMatchingUrl) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  NiceMock<content::MockNavigationHandle> navigation_handle(GURL(kOriginalUrl2),
                                                            extension_host);
  navigation_handle.set_has_committed(true);
  manager->DidFinishNavigation(&navigation_handle);

  EXPECT_FALSE(manager->DidExtensionFrameFinishNavigation(embedder_host));
}

// Verify `MimeHandlerStreamManager::PluginCanSave()` defaults to false
// and `MimeHandlerStreamManager::SetPluginCanSave()` updates the value.
TEST_F(MimeHandlerStreamManagerTest, PluginCanSave) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  bool plugin_can_save = false;
  ON_CALL(*delegate, PluginCanSave).WillByDefault([&]() {
    return plugin_can_save;
  });
  ON_CALL(*delegate, SetPluginCanSave).WillByDefault([&](bool can_save) {
    plugin_can_save = can_save;
  });
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `MimeHandlerStreamManager::PluginCanSave()` defaults to false.
  EXPECT_FALSE(manager->PluginCanSave(embedder_host));

  // Set to true.
  manager->SetPluginCanSave(embedder_host, true);
  EXPECT_TRUE(manager->PluginCanSave(embedder_host));

  // Set back to false.
  manager->SetPluginCanSave(embedder_host, false);
  EXPECT_FALSE(manager->PluginCanSave(embedder_host));
}

// Verify `MimeHandlerStreamManager::PluginCanSave()` returns false for an
// unknown embedder host, and `MimeHandlerStreamManager::SetPluginCanSave()` on
// an unknown host is a no-op.
TEST_F(MimeHandlerStreamManagerTest, PluginCanSaveUnknownHost) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* other_host = CreateChildRenderFrameHost(embedder_host, "other host");
  other_host = NavigateAndCommit(other_host, GURL(kOriginalUrl2));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  bool plugin_can_save = false;
  ON_CALL(*delegate, PluginCanSave).WillByDefault([&]() {
    return plugin_can_save;
  });
  ON_CALL(*delegate, SetPluginCanSave).WillByDefault([&](bool can_save) {
    plugin_can_save = can_save;
  });
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Unknown host should return false (no claimed stream info).
  EXPECT_FALSE(manager->PluginCanSave(other_host));

  // `MimeHandlerStreamManager::SetPluginCanSave()` on unknown host is a
  // no-op -- no crash, and the real host is unaffected.
  manager->SetPluginCanSave(other_host, true);
  EXPECT_FALSE(manager->PluginCanSave(other_host));
  EXPECT_FALSE(manager->PluginCanSave(embedder_host));
}

// Verify PluginCanSave / SetPluginCanSave work through the delegate.
TEST_F(MimeHandlerStreamManagerTest, PluginCanSaveViaDelegate) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  bool plugin_can_save = false;
  ON_CALL(*delegate, PluginCanSave).WillByDefault([&]() {
    return plugin_can_save;
  });
  ON_CALL(*delegate, SetPluginCanSave).WillByDefault([&](bool can_save) {
    plugin_can_save = can_save;
  });
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  EXPECT_FALSE(manager->PluginCanSave(embedder_host));

  manager->SetPluginCanSave(embedder_host, true);
  EXPECT_TRUE(manager->PluginCanSave(embedder_host));

  manager->SetPluginCanSave(embedder_host, false);
  EXPECT_FALSE(manager->PluginCanSave(embedder_host));
}

// Verify that AddStreamContainer stores the delegate on the stream info.
TEST_F(MimeHandlerStreamManagerTest, AddStreamContainerWithDelegate) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);

  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(stream_info);
  EXPECT_EQ(stream_info->delegate(), delegate_ptr);
}

// `ReadyToCommitNavigation` dispatches to the delegate's
// `OnExtensionFrameReadyToCommit` hook if and only if the committing
// frame is the claimed stream's extension host. Exercise the gating
// conditions with several navigation commits in one flow.
TEST_F(MimeHandlerStreamManagerTest,
       ExtensionFrameReadyToCommitDispatchesToDelegate) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  MimeHandlerStreamManager* manager = mime_handler_stream_manager();

  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  auto* delegate_ptr = delegate.get();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));

  // The embedder's own commit (top-level, no parent frame) claims the
  // stream and must not dispatch to the extension-frame hook.
  EXPECT_CALL(*delegate_ptr, OnExtensionFrameReadyToCommit(_, _)).Times(0);
  NiceMock<content::MockNavigationHandle> claim_handle;
  claim_handle.set_render_frame_host(embedder_host);
  manager->ReadyToCommitNavigation(&claim_handle);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Create the extension child frame but do not mark its FrameTreeNodeId
  // on the stream yet -- simulates the transient about:blank commit
  // before `NavigateToExtensionUrl()` runs. `DidExtensionStartNavigation()`
  // returns false, so the dispatch must not fire.
  content::RenderFrameHost* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  EXPECT_CALL(*delegate_ptr, OnExtensionFrameReadyToCommit(_, _)).Times(0);
  NiceMock<content::MockNavigationHandle> about_blank_handle;
  about_blank_handle.set_render_frame_host(extension_host);
  about_blank_handle.set_is_in_primary_main_frame(false);
  manager->ReadyToCommitNavigation(&about_blank_handle);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Mark the extension child frame's FrameTreeNodeId on the stream,
  // simulating `NavigateToExtensionUrl()`.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  // A sibling iframe under the same embedder has a different
  // FrameTreeNodeId, so its commit must not dispatch.
  content::RenderFrameHost* sibling_host =
      CreateChildRenderFrameHost(embedder_host, "sibling host");
  EXPECT_CALL(*delegate_ptr, OnExtensionFrameReadyToCommit(_, _)).Times(0);
  NiceMock<content::MockNavigationHandle> sibling_handle;
  sibling_handle.set_render_frame_host(sibling_host);
  sibling_handle.set_is_in_primary_main_frame(false);
  manager->ReadyToCommitNavigation(&sibling_handle);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // The extension frame's commit satisfies all gating conditions and
  // must dispatch exactly once.
  EXPECT_CALL(*delegate_ptr, OnExtensionFrameReadyToCommit(_, _));
  NiceMock<content::MockNavigationHandle> extension_handle;
  extension_handle.set_render_frame_host(extension_host);
  extension_handle.set_is_in_primary_main_frame(false);
  manager->ReadyToCommitNavigation(&extension_handle);
}

}  // namespace extensions::mime_handler
