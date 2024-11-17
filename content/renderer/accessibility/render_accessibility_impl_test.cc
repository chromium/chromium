// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl_test.h"

#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/test/test_render_frame.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-test-utils.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_updates_and_events.h"

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace content {

using blink::WebAXObject;
using blink::WebDocument;

namespace {

class RenderAccessibilityHostInterceptor
    : public blink::mojom::RenderAccessibilityHostInterceptorForTesting {
 public:
  explicit RenderAccessibilityHostInterceptor(
      const blink::BrowserInterfaceBrokerProxy& broker) {
    broker.GetInterface(local_frame_host_remote_.BindNewPipeAndPassReceiver());
    broker.SetBinderForTesting(
        blink::mojom::RenderAccessibilityHost::Name_,
        base::BindRepeating(&RenderAccessibilityHostInterceptor::
                                BindRenderAccessibilityHostReceiver,
                            base::Unretained(this)));
  }
  ~RenderAccessibilityHostInterceptor() override = default;

  blink::mojom::RenderAccessibilityHost* GetForwardingInterface() override {
    return local_frame_host_remote_.get();
  }

  void BindRenderAccessibilityHostReceiver(
      mojo::ScopedMessagePipeHandle handle) {
    receiver_.Add(this,
                  mojo::PendingReceiver<blink::mojom::RenderAccessibilityHost>(
                      std::move(handle)));

    receiver_.set_disconnect_handler(base::BindRepeating(
        [](mojo::ReceiverSet<blink::mojom::RenderAccessibilityHost>* receiver) {
        },
        base::Unretained(&receiver_)));
  }

  void HandleAXEvents(
      const ui::AXUpdatesAndEvents& updates_and_events,
      const ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
      uint32_t reset_token,
      HandleAXEventsCallback callback) override {
    NOTREACHED();
  }
  void HandleAXEvents(
      ui::AXUpdatesAndEvents& updates_and_events,
      ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
      uint32_t reset_token,
      HandleAXEventsCallback callback) override {
    handled_updates_.insert(handled_updates_.end(),
                            updates_and_events.updates.begin(),
                            updates_and_events.updates.end());
    for (auto& change : location_and_scroll_updates.location_changes) {
      location_changes_.emplace_back(std::move(change));
    }
    std::move(callback).Run();
  }

  void HandleAXLocationChanges(ui::AXLocationAndScrollUpdates& changes,
                               uint32_t reset_token) override {
    for (auto& change : changes.location_changes) {
      location_changes_.emplace_back(std::move(change));
    }
  }

  void HandleAXLocationChanges(const ui::AXLocationAndScrollUpdates& changes,
                               uint32_t reset_token) override {
    NOTREACHED();
  }

  ui::AXTreeUpdate& last_update() {
    CHECK_GE(handled_updates_.size(), 1U);
    return handled_updates_.back();
  }

  const std::vector<ui::AXTreeUpdate>& handled_updates() const {
    return handled_updates_;
  }

  std::vector<ui::AXLocationChange>& location_changes() {
    return location_changes_;
  }

  void ClearHandledUpdates() { handled_updates_.clear(); }

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::ReceiverSet<blink::mojom::RenderAccessibilityHost> receiver_;
  mojo::Remote<blink::mojom::RenderAccessibilityHost> local_frame_host_remote_;

  std::vector<::ui::AXTreeUpdate> handled_updates_;
  std::vector<ui::AXLocationChange> location_changes_;
};

class RenderAccessibilityTestRenderFrame : public TestRenderFrame {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params) {
    return new RenderAccessibilityTestRenderFrame(std::move(params));
  }

  ~RenderAccessibilityTestRenderFrame() override = default;

  ui::AXTreeUpdate& LastUpdate() {
    return render_accessibility_host_->last_update();
  }

  const std::vector<ui::AXTreeUpdate>& HandledUpdates() {
    return render_accessibility_host_->handled_updates();
  }

  void ClearHandledUpdates() {
    render_accessibility_host_->ClearHandledUpdates();
  }

  std::vector<ui::AXLocationChange>& LocationChanges() {
    return render_accessibility_host_->location_changes();
  }

  void InstallAccessibilityHost() {
    render_accessibility_host_ =
        std::make_unique<RenderAccessibilityHostInterceptor>(
            GetBrowserInterfaceBroker());
  }

 private:
  explicit RenderAccessibilityTestRenderFrame(
      RenderFrameImpl::CreateParams params)
      : TestRenderFrame(std::move(params)) {}

  std::unique_ptr<RenderAccessibilityHostInterceptor>
      render_accessibility_host_;
};

}  // namespace

RenderAccessibilityImplTest::RenderAccessibilityImplTest()
    : RenderViewTest(/*hook_render_frame_creation=*/false) {
  RenderFrameImpl::InstallCreateHook(
      &RenderAccessibilityTestRenderFrame::CreateTestRenderFrame);
}

RenderFrameImpl* RenderAccessibilityImplTest::frame() {
  return static_cast<RenderFrameImpl*>(RenderViewTest::GetMainRenderFrame());
}

RenderAccessibilityImpl*
RenderAccessibilityImplTest::GetRenderAccessibilityImpl() {
  auto* accessibility_manager = frame()->GetRenderAccessibilityManager();
  DCHECK(accessibility_manager);
  return accessibility_manager->GetRenderAccessibilityImpl();
}

void RenderAccessibilityImplTest::MarkSubtreeDirty(const WebAXObject& obj) {
  unsigned num_children = obj.ChildCount();
  for (unsigned child_index = 0; child_index < num_children; child_index++) {
    const WebAXObject& child = obj.ChildAt(child_index);
    MarkSubtreeDirty(child);
  }
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(obj);
}

void RenderAccessibilityImplTest::LoadHTMLAndRefreshAccessibilityTree(
    const char* html) {
  LoadHTML(html);
  ClearHandledUpdates();
  WebDocument document = GetMainFrame()->GetDocument();
  EXPECT_FALSE(document.IsNull());
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  EXPECT_FALSE(root_obj.IsNull());
  SendPendingAccessibilityEvents();
}

void RenderAccessibilityImplTest::SetUp() {
  blink::WebTestingSupport::SaveRuntimeFeatures();
  blink::WebRuntimeFeatures::EnableExperimentalFeatures(false);
  blink::WebRuntimeFeatures::EnableTestOnlyFeatures(false);

  RenderViewTest::SetUp();
  static_cast<RenderAccessibilityTestRenderFrame*>(frame())
      ->InstallAccessibilityHost();

  // Ensure that a valid RenderAccessibilityImpl object is created and
  // associated to the RenderFrame, so that calls from tests to methods of
  // RenderAccessibilityImpl will work.
  frame()->SetAccessibilityModeForTest(ui::kAXModeWebContentsOnly.flags());
}

void RenderAccessibilityImplTest::TearDown() {
#if defined(LEAK_SANITIZER)
  // Do this before shutting down V8 in RenderViewTest::TearDown().
  // http://crbug.com/328552
  __lsan_do_leak_check();
#endif
  RenderViewTest::TearDown();
  blink::WebTestingSupport::ResetRuntimeFeatures();
}

void RenderAccessibilityImplTest::SetMode(ui::AXMode mode) {
  frame()->GetRenderAccessibilityManager()->SetMode(mode, 1);
}

ui::AXTreeUpdate RenderAccessibilityImplTest::GetLastAccUpdate() {
  return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
      ->LastUpdate();
}

const std::vector<ui::AXTreeUpdate>&
RenderAccessibilityImplTest::GetHandledAccUpdates() {
  return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
      ->HandledUpdates();
}

void RenderAccessibilityImplTest::ClearHandledUpdates() {
  return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
      ->ClearHandledUpdates();
}

std::vector<ui::AXLocationChange>&
RenderAccessibilityImplTest::GetLocationChanges() {
  return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
      ->LocationChanges();
}

int RenderAccessibilityImplTest::CountAccessibilityNodesSentToBrowser() {
  ui::AXTreeUpdate update = GetLastAccUpdate();
  return update.nodes.size();
}

void RenderAccessibilityImplTest::SendPendingAccessibilityEvents() {
  // Ensure there are no pending events before sending accessibility events to
  // be able to properly check later on the nodes that have been updated, and
  // also wait for the mojo messages to be processed once they are sent.
  task_environment_.RunUntilIdle();
  GetRenderAccessibilityImpl()->ScheduleImmediateAXUpdate();
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
  task_environment_.RunUntilIdle();
}

}  // namespace content
