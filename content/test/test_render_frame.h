// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "content/common/frame.mojom-forward.h"
#include "content/common/navigation_params.mojom-forward.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace base {
class UnguessableToken;
}

namespace blink {
class WebHistoryItem;
}

namespace content {

class MockFrameHost;

// A test class to use in RenderViewTests.
class TestRenderFrame : public RenderFrameImpl {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params);
  ~TestRenderFrame() override;

  const blink::WebHistoryItem& current_history_item() {
    return current_history_item_;
  }

  // Overrides the content in the next navigation originating from the frame.
  // This will also short-circuit browser-side navigation,
  // FrameLoader will always carry out the load renderer-side.
  void SetHTMLOverrideForNextNavigation(const std::string& html);

  void Navigate(network::mojom::URLResponseHeadPtr head,
                mojom::CommonNavigationParamsPtr common_params,
                mojom::CommitNavigationParamsPtr commit_params);
  void Navigate(mojom::CommonNavigationParamsPtr common_params,
                mojom::CommitNavigationParamsPtr commit_params);
  void NavigateWithError(mojom::CommonNavigationParamsPtr common_params,
                         mojom::CommitNavigationParamsPtr request_params,
                         int error_code,
                         const net::ResolveErrorInfo& resolve_error_info,
                         const base::Optional<std::string>& error_page_content);
  void Unload(int proxy_routing_id,
              bool is_loading,
              const FrameReplicationState& replicated_frame_state,
              const base::UnguessableToken& frame_token);
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  TakeLastCommitParams();

  // Sets a callback to be run the next time DidAddMessageToConsole
  // is called (e.g. window.console.log() is called).
  void SetDidAddMessageToConsoleCallback(
      base::OnceCallback<void(const base::string16& msg)> callback);

  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
  TakeLastInterfaceProviderReceiver();

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  TakeLastBrowserInterfaceBrokerReceiver();

  void SimulateBeforeUnload(bool is_reload);

  bool IsPageStateUpdated() const;

  bool IsURLOpened() const;

 protected:
  explicit TestRenderFrame(RenderFrameImpl::CreateParams params);

 private:
  mojom::FrameHost* GetFrameHost() override;

  std::unique_ptr<MockFrameHost> mock_frame_host_;
  base::Optional<std::string> next_navigation_html_override_;

  mojo::AssociatedRemote<mojom::NavigationClient> mock_navigation_client_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderFrame);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_H_
