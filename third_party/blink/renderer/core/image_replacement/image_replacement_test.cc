// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockImageReplacementHost : public mojom::blink::ImageReplacementHost {
 public:
  void ReplacementFrameAttached(
      const blink::LocalFrameToken& frame_token) override {
    frame_token_ = frame_token;
  }

  const std::optional<blink::LocalFrameToken>& frame_token() const {
    return frame_token_;
  }

  mojo::Receiver<mojom::blink::ImageReplacementHost>& receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<mojom::blink::ImageReplacementHost> receiver_{this};
  std::optional<blink::LocalFrameToken> frame_token_;
};

class ImageReplacementSimTest : public SimTest {};

TEST_F(ImageReplacementSimTest, ImageReplacementLifecycle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());
  ASSERT_TRUE(img->GetLayoutObject());
  EXPECT_TRUE(img->GetLayoutObject()->IsLayoutImage());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  // Verify replacement state
  EXPECT_FALSE(img->GetLayoutObject()->IsLayoutImage());
  EXPECT_TRUE(img->GetLayoutObject()->IsLayoutBlockFlow());
  ASSERT_TRUE(img->UserAgentShadowRoot());

  auto* iframe =
      DynamicTo<HTMLIFrameElement>(img->UserAgentShadowRoot()->firstChild());
  ASSERT_TRUE(iframe);
  ASSERT_TRUE(iframe->ContentFrame());
  EXPECT_TRUE(mock_host.frame_token().has_value());
  EXPECT_EQ(iframe->ContentFrame()->GetFrameToken(), mock_host.frame_token());

  // Attempt to create another replacement should fail
  auto result2 = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result2.has_value());

  // Verify reset on disconnect
  replacement_remote.reset();
  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_TRUE(img->GetLayoutObject()->IsLayoutImage());
  EXPECT_FALSE(img->UserAgentShadowRoot()->HasChildren());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest, ImageReplacementFailsIfNotLoaded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"></img>
  )");
  // Image not loaded yet.

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  // Replacement should not have started since the image is not loaded.
  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot());
  EXPECT_FALSE(mock_host.frame_token().has_value());

  // Finish the load (with invalid data to cause an error).
  image_resource.Complete("invalid data");
  test::RunPendingTasks();
  EXPECT_TRUE(img->CachedImage()->ErrorOccurred());

  // Replacement should not have started since the image load failed.
  mock_host.receiver().reset();
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote());
  test::RunPendingTasks();
  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(replacement_remote.is_connected());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest, ImageReplacementRemovedFromDocument) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_TRUE(img->UserAgentShadowRoot()->HasChildren());

  img->remove();
  test::RunPendingTasks();

  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot()->HasChildren());
  EXPECT_FALSE(replacement_remote.is_connected());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest,
       ImageReplacementCreateOnDisconnectedImageFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());
  img->remove();
  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageReplacementSimTest, ImageReplacementCreateWithEmptySrcFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="" id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageReplacementSimTest, ImageReplacementMovedToNewDocument) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <iframe id="iframe"></iframe>
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_TRUE(img->UserAgentShadowRoot()->HasChildren());

  HTMLIFrameElement* iframe = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  ASSERT_TRUE(iframe);
  Document* new_document = iframe->contentDocument();
  ASSERT_TRUE(new_document);
  ASSERT_TRUE(new_document->body());

  new_document->body()->AppendChild(img);
  test::RunPendingTasks();

  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot()->HasChildren());
  EXPECT_FALSE(replacement_remote.is_connected());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(*new_document));

  // Verify that the replacement can be created in the new document.
  result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(DocumentImageReplacements::FromIfExists(*new_document)
                  ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest, ImageReplacementCreateWithFailedLoadFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"></img>
  )");

  image_resource.Complete("invalid data");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->CachedImage()->ErrorOccurred());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result.has_value());
}

}  // namespace blink
