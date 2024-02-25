// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_H_

#include <memory>
#include <optional>

#include "content/common/frame.mojom-forward.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"

namespace content {

class MockFrameHost;

// A test class to use in RenderViewTests.
class TestRenderFrame : public RenderFrameImpl {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params);

  TestRenderFrame(const TestRenderFrame&) = delete;
  TestRenderFrame& operator=(const TestRenderFrame&) = delete;

  ~TestRenderFrame() override;

  // Overrides the content in the next navigation originating from the frame.
  // This will also short-circuit browser-side navigation,
  // FrameLoader will always carry out the load renderer-side.
  void SetHTMLOverrideForNextNavigation(const std::string& html);

  void Navigate(network::mojom::URLResponseHeadPtr head,
                blink::mojom::CommonNavigationParamsPtr common_params,
                blink::mojom::CommitNavigationParamsPtr commit_params);
  void Navigate(blink::mojom::CommonNavigationParamsPtr common_params,
                blink::mojom::CommitNavigationParamsPtr commit_params);
  void NavigateWithError(blink::mojom::CommonNavigationParamsPtr common_params,
                         blink::mojom::CommitNavigationParamsPtr request_params,
                         int error_code,
                         const net::ResolveErrorInfo& resolve_error_info,
                         const std::optional<std::string>& error_page_content);
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;

  mojom::DidCommitProvisionalLoadParamsPtr TakeLastCommitParams();

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  TakeLastBrowserInterfaceBrokerReceiver();

  void SimulateBeforeUnload(bool is_reload);

  bool IsPageStateUpdated() const;

  bool IsURLOpened() const;

  // Returns a pending Frame receiver that represents a renderer-side connection
  // from a non-existent browser, so no messages would ever be received on it.
  static mojo::PendingAssociatedReceiver<mojom::Frame>
  CreateStubFrameReceiver();

  // Returns a pending BrowserInterfaceBroker remote that represents a
  // connection to a non-existent browser, where all messages will go into the
  // void.
  static mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
  CreateStubBrowserInterfaceBrokerRemote();

  // Returns a pending `AssociatedInterfaceProvider` remote that represents a
  // connection to a non-existent browser, where all messages will go into the
  // void.
  static mojo::PendingAssociatedRemote<
      blink::mojom::AssociatedInterfaceProvider>
  CreateStubAssociatedInterfaceProviderRemote();

 protected:
  explicit TestRenderFrame(RenderFrameImpl::CreateParams params);

 private:
  void BindToFrame(blink::WebNavigationControl* frame) override;
  mojom::FrameHost* GetFrameHost() override;

  std::unique_ptr<MockFrameHost> mock_frame_host_;
  std::optional<std::string> next_navigation_html_override_;

  mojo::AssociatedRemote<mojom::NavigationClient> mock_navigation_client_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_H_
