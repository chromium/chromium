// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockSurfaceEmbedConnectorDelegate
    : public SurfaceEmbedConnector::Delegate {
 public:
  MockSurfaceEmbedConnectorDelegate() = default;
  ~MockSurfaceEmbedConnectorDelegate() = default;

  MOCK_METHOD(void, SetFrameSinkId, (const viz::FrameSinkId&), (override));
  MOCK_METHOD(void,
              UpdateLocalSurfaceIdFromChild,
              (const viz::LocalSurfaceId&),
              (override));
  MOCK_METHOD(void, DetachedByHost, (), (override));
  MOCK_METHOD(bool, IsAttachedForTesting, (), (const, override));
};

}  // namespace

class SurfaceEmbedConnectorImplBrowserTest : public ContentBrowserTest {
 public:
  SurfaceEmbedConnectorImplBrowserTest() = default;
  ~SurfaceEmbedConnectorImplBrowserTest() override = default;

 protected:
  WebContentsImpl* GetParentWebContents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, BasicConnection) {
  // Create the child WebContents.
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);

  MockSurfaceEmbedConnectorDelegate delegate;

  // Create the connector.
  auto connector = std::make_unique<SurfaceEmbedConnectorImpl>(
      child_web_contents.get(), GetParentWebContents(), &delegate);

  // Verify initial state and basic getters.
  EXPECT_EQ(connector->GetParentWebContentsView(),
            GetParentWebContents()->GetView());
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(),
            GetParentWebContents()->GetDelegateView());
  EXPECT_EQ(connector->GetInputEventRouter(),
            GetParentWebContents()->GetInputEventRouter());

  // Verify delegate access.
  EXPECT_EQ(connector->GetDelegate(), &delegate);

  // Verify TextInputManager is forwarded.
  EXPECT_EQ(connector->GetTextInputManager(),
            GetParentWebContents()->GetTextInputManager());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       ParentDestruction) {
  // Create a separate parent WebContents so we can destroy it during the
  // test.
  WebContents::CreateParams parent_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> parent = WebContents::Create(parent_params);
  WebContentsImpl* parent_impl = static_cast<WebContentsImpl*>(parent.get());

  // Create the child WebContents.
  WebContents::CreateParams child_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child = WebContents::Create(child_params);

  MockSurfaceEmbedConnectorDelegate delegate;
  auto connector = std::make_unique<SurfaceEmbedConnectorImpl>(
      child.get(), parent_impl, &delegate);

  EXPECT_EQ(connector->GetParentWebContentsView(), parent_impl->GetView());

  // Destroy the parent.
  parent.reset();

  // Verify connector handles missing parent gracefully where checks exist.
  EXPECT_EQ(connector->GetParentWebContentsView(), nullptr);
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(), nullptr);

  // Note: GetInputEventRouter() and GetTextInputManager() in
  // SurfaceEmbedConnectorImpl currently do not check for null parent, so we
  // don't test them here to avoid crash.
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, ConstGetters) {
  // Create the child WebContents.
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);

  MockSurfaceEmbedConnectorDelegate delegate;

  // Create the connector.
  auto connector = std::make_unique<SurfaceEmbedConnectorImpl>(
      child_web_contents.get(), GetParentWebContents(), &delegate);

  const SurfaceEmbedConnectorImpl& const_connector = *connector;

  // Verify getters can be called on a const reference.
  EXPECT_EQ(const_connector.GetParentWebContentsView(),
            GetParentWebContents()->GetView());
  EXPECT_EQ(const_connector.GetParentRenderViewHostDelegateView(),
            GetParentWebContents()->GetDelegateView());
}

}  // namespace content
