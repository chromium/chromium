// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame.h"

#include <string>
#include <utility>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/input/frame_input_handler_impl.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

class MockFrameHost : public mojom::FrameHost {
 public:
  MockFrameHost() {}
  ~MockFrameHost() override = default;

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  TakeLastCommitParams() {
    return std::move(last_commit_params_);
  }

  service_manager::mojom::InterfaceProviderRequest
  TakeLastInterfaceProviderRequest() {
    return std::move(last_interface_provider_request_);
  }

  // Holds on to the request end of the InterfaceProvider interface whose client
  // end is bound to the corresponding RenderFrame's |remote_interfaces_| to
  // facilitate retrieving the most recent |interface_provider_request| in
  // tests.
  void PassLastInterfaceProviderRequest(
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request) {
    last_interface_provider_request_ = std::move(interface_provider_request);
  }

 protected:
  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr,
                       CreateNewWindowCallback) override {
    NOTREACHED() << "We should never dispatch to the service side signature.";
  }

  bool CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       mojom::CreateNewWindowStatus* status,
                       mojom::CreateNewWindowReplyPtr* reply) override {
    *status = mojom::CreateNewWindowStatus::kSuccess;
    *reply = mojom::CreateNewWindowReply::New();
    MockRenderThread* mock_render_thread =
        static_cast<MockRenderThread*>(RenderThread::Get());
    mock_render_thread->OnCreateWindow(*params, reply->get());
    return true;
  }

  void IssueKeepAliveHandle(mojom::KeepAliveHandleRequest request) override {}

  void DidCommitProvisionalLoad(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      service_manager::mojom::InterfaceProviderRequest request) override {
    last_commit_params_ = std::move(params);
    last_interface_provider_request_ = std::move(request);
  }

  void DidCommitSameDocumentNavigation(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params)
      override {
    last_commit_params_ = std::move(params);
  }

  void BeginNavigation(const CommonNavigationParams& common_params,
                       mojom::BeginNavigationParamsPtr begin_params,
                       blink::mojom::BlobURLTokenPtr blob_url_token,
                       mojom::NavigationClientAssociatedPtrInfo,
                       blink::mojom::NavigationInitiatorPtr) override {}

  void SubresourceResponseStarted(const GURL& url,
                                  net::CertStatus cert_status) override {}

  void ResourceLoadComplete(
      mojom::ResourceLoadInfoPtr resource_load_info) override {}

  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override {}

  void EnforceInsecureRequestPolicy(
      blink::WebInsecureRequestPolicy policy) override {}
  void EnforceInsecureNavigationsSet(
      const std::vector<uint32_t>& set) override {}

  void DidSetFramePolicyHeaders(
      blink::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& parsed_header) override {}

  void CancelInitialHistoryLoad() override {}

  void UpdateEncoding(const std::string& encoding_name) override {}

  void FrameSizeChanged(const gfx::Size& frame_size) override {}

  void FullscreenStateChanged(bool is_fullscreen) override {}

#if defined(OS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override {}
#endif

 private:
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
      last_commit_params_;
  service_manager::mojom::InterfaceProviderRequest
      last_interface_provider_request_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameHost);
};

// static
RenderFrameImpl* TestRenderFrame::CreateTestRenderFrame(
    RenderFrameImpl::CreateParams params) {
  return new TestRenderFrame(std::move(params));
}

TestRenderFrame::TestRenderFrame(RenderFrameImpl::CreateParams params)
    : RenderFrameImpl(std::move(params)),
      mock_frame_host_(std::make_unique<MockFrameHost>()) {
  MockRenderThread* mock_render_thread =
      static_cast<MockRenderThread*>(RenderThread::Get());
  mock_frame_host_->PassLastInterfaceProviderRequest(
      mock_render_thread->TakeInitialInterfaceProviderRequestForFrame(
          params.routing_id));
}

TestRenderFrame::~TestRenderFrame() {}

void TestRenderFrame::SetURLOverrideForNextWebURLRequest(const GURL& url) {
  next_request_url_override_ = url;
}

void TestRenderFrame::WillSendRequest(blink::WebURLRequest& request) {
  if (next_request_url_override_.has_value())
    request.SetURL(std::move(next_request_url_override_).value());
  RenderFrameImpl::WillSendRequest(request);
}

void TestRenderFrame::Navigate(const CommonNavigationParams& common_params,
                               const RequestNavigationParams& request_params) {
  CommitNavigation(
      network::ResourceResponseHead(), common_params, request_params,
      network::mojom::URLLoaderClientEndpointsPtr(),
      std::make_unique<URLLoaderFactoryBundleInfo>(), base::nullopt,
      mojom::ControllerServiceWorkerInfoPtr(),
      network::mojom::URLLoaderFactoryPtr(), base::UnguessableToken::Create(),
      CommitNavigationCallback());
}

void TestRenderFrame::SwapOut(
    int proxy_routing_id,
    bool is_loading,
    const FrameReplicationState& replicated_frame_state) {
  OnSwapOut(proxy_routing_id, is_loading, replicated_frame_state);
}

void TestRenderFrame::SetEditableSelectionOffsets(int start, int end) {
  GetFrameInputHandler()->SetEditableSelectionOffsets(start, end);
}

void TestRenderFrame::ExtendSelectionAndDelete(int before, int after) {
  GetFrameInputHandler()->ExtendSelectionAndDelete(before, after);
}

void TestRenderFrame::DeleteSurroundingText(int before, int after) {
  GetFrameInputHandler()->DeleteSurroundingText(before, after);
}

void TestRenderFrame::DeleteSurroundingTextInCodePoints(int before, int after) {
  GetFrameInputHandler()->DeleteSurroundingTextInCodePoints(before, after);
}

void TestRenderFrame::CollapseSelection() {
  GetFrameInputHandler()->CollapseSelection();
}

void TestRenderFrame::SetAccessibilityMode(ui::AXMode new_mode) {
  OnSetAccessibilityMode(new_mode);
}

void TestRenderFrame::SetCompositionFromExistingText(
    int start,
    int end,
    const std::vector<ui::ImeTextSpan>& ime_text_spans) {
  GetFrameInputHandler()->SetCompositionFromExistingText(start, end,
                                                         ime_text_spans);
}

blink::WebNavigationPolicy TestRenderFrame::DecidePolicyForNavigation(
    const blink::WebLocalFrameClient::NavigationPolicyInfo& info) {
  if (info.default_policy == blink::kWebNavigationPolicyCurrentTab &&
      ((GetWebFrame()->Parent() && info.form.IsNull()) ||
       next_request_url_override_.has_value())) {
    // RenderViewTest::LoadHTML immediately commits navigation for the main
    // frame. However if the loaded html has a subframe,
    // DecidePolicyForNavigation will be called from Blink and we should avoid
    // going through browser process in this case.
    return blink::kWebNavigationPolicyCurrentTab;
  }
  return RenderFrameImpl::DecidePolicyForNavigation(info);
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
TestRenderFrame::TakeLastCommitParams() {
  return mock_frame_host_->TakeLastCommitParams();
}

service_manager::mojom::InterfaceProviderRequest
TestRenderFrame::TakeLastInterfaceProviderRequest() {
  return mock_frame_host_->TakeLastInterfaceProviderRequest();
}

mojom::FrameHost* TestRenderFrame::GetFrameHost() {
  // Need to mock this interface directly without going through a binding,
  // otherwise calling its sync methods could lead to a deadlock.
  //
  // Imagine the following sequence of events take place:
  //
  //   1.) GetFrameHost() called for the first time
  //   1.1.) GetRemoteAssociatedInterfaces()->GetInterface(&frame_host_ptr_)
  //   1.1.1) ... plumbing ...
  //   1.1.2) Task posted to bind the request end to the Mock implementation
  //   1.2) The interface pointer end is returned to the caller
  //   2.) GetFrameHost()->CreateNewWindow(...) sync method invoked
  //   2.1.) Mojo sync request sent
  //   2.2.) Waiting for sync response while dispatching incoming sync requests
  //
  // Normally the sync Mojo request would be processed in 2.2. However, the
  // implementation is not yet bound at that point, and will never be, because
  // only sync IPCs are dispatched by 2.2, not posted tasks. So the sync request
  // is never dispatched, the response never arrives.
  //
  // Because the first invocation to GetFrameHost() may come while we are inside
  // a message loop already, pumping messags before 1.2 would constitute a
  // nested message loop and is therefore undesired.
  return mock_frame_host_.get();
}

mojom::FrameInputHandler* TestRenderFrame::GetFrameInputHandler() {
  if (!frame_input_handler_) {
    mojom::FrameInputHandlerRequest frame_input_handler_request =
        mojo::MakeRequest(&frame_input_handler_);
    FrameInputHandlerImpl::CreateMojoService(
        weak_factory_.GetWeakPtr(), std::move(frame_input_handler_request));
  }
  return frame_input_handler_.get();
}

}  // namespace content
