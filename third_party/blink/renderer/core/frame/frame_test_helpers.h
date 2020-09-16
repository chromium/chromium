/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_TEST_HELPERS_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/web_fake_thread_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/loader/testing/web_url_loader_factory_with_mock.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#define EXPECT_FLOAT_POINT_EQ(expected, actual)    \
  do {                                             \
    EXPECT_FLOAT_EQ((expected).X(), (actual).X()); \
    EXPECT_FLOAT_EQ((expected).Y(), (actual).Y()); \
  } while (false)

#define EXPECT_FLOAT_SIZE_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_FLOAT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

#define EXPECT_FLOAT_RECT_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).X(), (actual).X());           \
    EXPECT_FLOAT_EQ((expected).Y(), (actual).Y());           \
    EXPECT_FLOAT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_FLOAT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

namespace base {
class TickClock;
}

namespace blink {
class WebFrame;
class WebLocalFrameImpl;
struct WebNavigationParams;
class WebRemoteFrameImpl;
class WebSettings;

namespace frame_test_helpers {
class TestWebFrameClient;
class TestWebRemoteFrameClient;
class TestWebWidgetClient;
class TestWebViewClient;
class TestWidgetInputHandlerHost;
class WebViewHelper;

cc::LayerTreeSettings GetSynchronousSingleThreadLayerTreeSettings();

// Loads a url into the specified WebLocalFrame for testing purposes.
void LoadFrameDontWait(WebLocalFrame*, const WebURL& url);
// Same as above, but also pumps any pending resource requests,
// as well as waiting for the threaded parser to finish, before returning.
void LoadFrame(WebLocalFrame*, const std::string& url);
// Same as above, but for WebLocalFrame::LoadHTMLString().
void LoadHTMLString(WebLocalFrame*,
                    const std::string& html,
                    const WebURL& base_url,
                    const base::TickClock* clock = nullptr);
// Same as above, but for WebLocalFrame::RequestFromHistoryItem/Load.
void LoadHistoryItem(WebLocalFrame*,
                     const WebHistoryItem&,
                     mojom::FetchCacheMode);
// Same as above, but for WebLocalFrame::Reload().
void ReloadFrame(WebLocalFrame*);
void ReloadFrameBypassingCache(WebLocalFrame*);

// Fills navigation params if needed. Params should have the proper url set up.
void FillNavigationParamsResponse(WebNavigationParams*);

// Pumps pending resource requests while waiting for a frame to load. Consider
// using one of the above helper methods whenever possible.
void PumpPendingRequestsForFrameToLoad(WebLocalFrame*);

WebMouseEvent CreateMouseEvent(WebInputEvent::Type,
                               WebMouseEvent::Button,
                               const IntPoint&,
                               int modifiers);

// Helpers for creating frames for test purposes. All methods that accept raw
// pointer client arguments allow nullptr as a valid argument; if a client
// pointer is null, the test framework will automatically create and manage the
// lifetime of that client interface. Otherwise, the caller is responsible for
// ensuring that non-null clients outlive the created frame.

// Helper for creating a local child frame of a local parent frame.
WebLocalFrameImpl* CreateLocalChild(WebLocalFrame& parent,
                                    blink::mojom::blink::TreeScopeType,
                                    TestWebFrameClient* = nullptr);

// Similar, but unlike the overload which takes the client as a raw pointer,
// ownership of the TestWebFrameClient is transferred to the test framework.
// TestWebFrameClient may not be null.
WebLocalFrameImpl* CreateLocalChild(WebLocalFrame& parent,
                                    blink::mojom::blink::TreeScopeType,
                                    std::unique_ptr<TestWebFrameClient>);

// Helper for creating a provisional local frame that can replace a remote
// frame.
WebLocalFrameImpl* CreateProvisional(WebRemoteFrame& old_frame,
                                     TestWebFrameClient* = nullptr);

// Helper for creating a remote frame. Generally used when creating a remote
// frame to swap into the frame tree.
// TODO(dcheng): Consider allowing security origin to be passed here once the
// frame tree moves back to core.
WebRemoteFrameImpl* CreateRemote(TestWebRemoteFrameClient* = nullptr);

// Helper for creating a local child frame of a remote parent frame.
WebLocalFrameImpl* CreateLocalChild(
    WebRemoteFrame& parent,
    const WebString& name = WebString(),
    const WebFrameOwnerProperties& = WebFrameOwnerProperties(),
    WebFrame* previous_sibling = nullptr,
    TestWebFrameClient* = nullptr,
    TestWebWidgetClient* = nullptr);

// Helper for creating a remote child frame of a remote parent frame.
WebRemoteFrameImpl* CreateRemoteChild(WebRemoteFrame& parent,
                                      const WebString& name = WebString(),
                                      scoped_refptr<SecurityOrigin> = nullptr,
                                      TestWebRemoteFrameClient* = nullptr);

class TestWebWidgetClient : public WebWidgetClient,
                            public mojom::blink::WidgetHost {
 public:
  TestWebWidgetClient();
  ~TestWebWidgetClient() override = default;

  // This method must be called just after the allocation of |widget| and
  // before usage of this class occurs.
  void SetFrameWidget(
      WebFrameWidget* widget,
      mojo::AssociatedRemote<mojom::blink::Widget> widget_remote);

  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_; }
  const cc::LayerTreeHost* layer_tree_host() const { return layer_tree_host_; }

  bool AnimationScheduled() const { return animation_scheduled_; }
  void ClearAnimationScheduled() { animation_scheduled_ = false; }

  bool HaveScrollEventHandlers() const;
  const Vector<std::unique_ptr<blink::WebCoalescedInputEvent>>&
  GetInjectedScrollEvents() const {
    return injected_scroll_events_;
  }

  cc::TaskGraphRunner* task_graph_runner() { return &test_task_graph_runner_; }

  scheduler::WebThreadScheduler* main_thread_scheduler() {
    return &fake_thread_scheduler_;
  }

  void set_layer_tree_host(cc::LayerTreeHost* layer_tree_host) {
    layer_tree_host_ = layer_tree_host;
  }

  virtual ScreenInfo GetInitialScreenInfo();

  mojo::PendingAssociatedRemote<mojom::blink::WidgetHost> BindNewWidgetHost();

 protected:
  // Allow subclasses to provide their own input handler host.
  virtual TestWidgetInputHandlerHost* GetInputHandlerHost();

  // WebWidgetClient overrides;
  void ScheduleAnimation() override { animation_scheduled_ = true; }
  viz::FrameSinkId GetFrameSinkId() override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void WillQueueSyntheticEvent(const WebCoalescedInputEvent& event) override;
  bool ShouldAutoDetermineCompositingToLCDTextSetting() override {
    return false;
  }

  // mojom::blink::WidgetHost overrides:
  void SetCursor(const ui::Cursor& cursor) override;
  void SetToolTipText(const String& tooltip_text,
                      base::i18n::TextDirection text_direction_hint) override;
  void TextInputStateChanged(
      ui::mojom::blink::TextInputStatePtr state) override;
  void SelectionBoundsChanged(const gfx::Rect& anchor_rect,
                              base::i18n::TextDirection anchor_dir,
                              const gfx::Rect& focus_rect,
                              base::i18n::TextDirection focus_dir,
                              bool is_anchor_first) override;

 private:
  WebFrameWidget* frame_widget_ = nullptr;
  cc::LayerTreeHost* layer_tree_host_ = nullptr;
  cc::TestTaskGraphRunner test_task_graph_runner_;
  blink::scheduler::WebFakeThreadScheduler fake_thread_scheduler_;
  Vector<std::unique_ptr<blink::WebCoalescedInputEvent>>
      injected_scroll_events_;
  std::unique_ptr<TestWidgetInputHandlerHost> widget_input_handler_host_;
  bool animation_scheduled_ = false;
  viz::FrameSinkId frame_sink_id_;
  mojo::AssociatedReceiver<mojom::blink::WidgetHost> receiver_{this};
};

class TestWebViewClient : public WebViewClient {
 public:
  TestWebViewClient() = default;
  ~TestWebViewClient() override = default;

  void DestroyChildViews();

  // WebViewClient overrides.
  bool CanHandleGestureEvent() override { return true; }
  bool CanUpdateLayout() override { return true; }
  WebView* CreateView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      network::mojom::blink::WebSandboxFlags,
                      const FeaturePolicyFeatureState&,
                      const SessionStorageNamespaceId&) override;

 private:
  WTF::Vector<std::unique_ptr<WebViewHelper>> child_web_views_;
};

// Convenience class for handling the lifetime of a WebView and its associated
// mainframe in tests.
class WebViewHelper : public ScopedMockOverlayScrollbars {
  USING_FAST_MALLOC(WebViewHelper);

 public:
  WebViewHelper();
  ~WebViewHelper();

  // Helpers for creating the main frame. All methods that accept raw
  // pointer client arguments allow nullptr as a valid argument; if a client
  // pointer is null, the test framework will automatically create and manage
  // the lifetime of that client interface. Otherwise, the caller is responsible
  // for ensuring that non-null clients outlive the created frame.

  // Creates and initializes the WebView with a main WebLocalFrame.
  WebViewImpl* InitializeWithOpener(
      WebFrame* opener,
      TestWebFrameClient* = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr,
      void (*update_settings_func)(WebSettings*) = nullptr);

  // Same as InitializeWithOpener(), but always sets the opener to null.
  WebViewImpl* Initialize(TestWebFrameClient* = nullptr,
                          TestWebViewClient* = nullptr,
                          TestWebWidgetClient* = nullptr,
                          void (*update_settings_func)(WebSettings*) = nullptr);

  // Same as InitializeWithOpener(), but passes null for everything but the
  // settings function.
  WebViewImpl* InitializeWithSettings(
      void (*update_settings_func)(WebSettings*));

  // Same as Initialize() but also performs the initial load of the url. Only
  // returns once the load is complete.
  WebViewImpl* InitializeAndLoad(
      const std::string& url,
      TestWebFrameClient* = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr,
      void (*update_settings_func)(WebSettings*) = nullptr);

  // Same as InitializeRemoteWithOpener(), but always sets the opener to null.
  WebViewImpl* InitializeRemote(TestWebRemoteFrameClient* = nullptr,
                                scoped_refptr<SecurityOrigin> = nullptr,
                                TestWebViewClient* = nullptr,
                                TestWebWidgetClient* = nullptr);

  // Creates and initializes the WebView with a main WebRemoteFrame. Passing
  // nullptr as the SecurityOrigin results in a frame with a unique security
  // origin.
  WebViewImpl* InitializeRemoteWithOpener(
      WebFrame* opener,
      TestWebRemoteFrameClient* = nullptr,
      scoped_refptr<SecurityOrigin> = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr);

  // Load the 'Ahem' font to this WebView.
  // The 'Ahem' font is the only font whose font metrics is consistent across
  // platforms, but it's not guaranteed to be available.
  // See external/wpt/css/fonts/ahem/README for more about the 'Ahem' font.
  void LoadAhem();

  void Resize(WebSize);

  void Reset();

  WebViewImpl* GetWebView() const { return web_view_; }
  cc::LayerTreeHost* GetLayerTreeHost() const {
    return test_web_widget_client_->layer_tree_host();
  }
  TestWebWidgetClient* GetWebWidgetClient() const {
    return test_web_widget_client_;
  }

  WebLocalFrameImpl* LocalMainFrame() const;
  WebRemoteFrameImpl* RemoteMainFrame() const;

  void set_viewport_enabled(bool viewport) {
    DCHECK(!web_view_)
        << "set_viewport_enabled() should be called before Initialize.";
    viewport_enabled_ = viewport;
  }

 private:
  void InitializeWebView(TestWebViewClient*,
                         class WebView* opener);

  bool viewport_enabled_ = false;

  WebViewImpl* web_view_;

  std::unique_ptr<TestWebViewClient> owned_test_web_view_client_;
  TestWebViewClient* test_web_view_client_ = nullptr;
  std::unique_ptr<TestWebWidgetClient> owned_test_web_widget_client_;
  TestWebWidgetClient* test_web_widget_client_ = nullptr;

  // The Platform should not change during the lifetime of the test!
  Platform* const platform_;

  DISALLOW_COPY_AND_ASSIGN(WebViewHelper);
};

// Minimal implementation of WebLocalFrameClient needed for unit tests that load
// frames. Tests that load frames and need further specialization of
// WebLocalFrameClient behavior should subclass this.
class TestWebFrameClient : public WebLocalFrameClient {
 public:
  TestWebFrameClient();
  ~TestWebFrameClient() override;

  static bool IsLoading() { return loads_in_progress_ > 0; }
  Vector<String>& ConsoleMessages() { return console_messages_; }

  WebNavigationControl* Frame() const { return frame_; }
  // Pass ownership of the TestWebFrameClient to |self_owned| here if the
  // TestWebFrameClient should delete itself on frame detach.
  void Bind(WebLocalFrame*,
            std::unique_ptr<TestWebFrameClient> self_owned = nullptr);
  // Note: only needed for local roots.
  void BindWidgetClient(std::unique_ptr<WebWidgetClient>);

  // WebLocalFrameClient:
  void FrameDetached() override;
  WebLocalFrame* CreateChildFrame(WebLocalFrame* parent,
                                  blink::mojom::blink::TreeScopeType,
                                  const WebString& name,
                                  const WebString& fallback_name,
                                  const FramePolicy&,
                                  const WebFrameOwnerProperties&,
                                  mojom::blink::FrameOwnerElementType) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory()
      override {
    return std::make_unique<WebURLLoaderFactoryWithMock>(
        WebURLLoaderMockFactory::GetSingletonInstance());
  }
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override;
  WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void SetEffectiveConnectionTypeForTesting(
      WebEffectiveConnectionType) override;
  void DidAddMessageToConsole(const WebConsoleMessage&,
                              const WebString& source_name,
                              unsigned source_line,
                              const WebString& stack_trace) override;
  WebPlugin* CreatePlugin(const WebPluginParams& params) override;
  AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces()
      override;
  void DidMeaningfulLayout(WebMeaningfulLayout) override;

  int VisuallyNonEmptyLayoutCount() const {
    return visually_non_empty_layout_count_;
  }
  int FinishedParsingLayoutCount() const {
    return finished_parsing_layout_count_;
  }
  int FinishedLoadingLayoutCount() const {
    return finished_loading_layout_count_;
  }

 private:
  void CommitNavigation(std::unique_ptr<WebNavigationInfo>);

  static int loads_in_progress_;

  // If set to a non-null value, self-deletes on frame detach.
  std::unique_ptr<TestWebFrameClient> self_owned_;

  std::unique_ptr<AssociatedInterfaceProvider> associated_interface_provider_;

  // This is null from when the client is created until it is initialized with
  // Bind().
  WebNavigationControl* frame_ = nullptr;

  base::CancelableOnceCallback<void()> navigation_callback_;
  std::unique_ptr<WebWidgetClient> owned_widget_client_;
  WebEffectiveConnectionType effective_connection_type_;
  Vector<String> console_messages_;
  int visually_non_empty_layout_count_ = 0;
  int finished_parsing_layout_count_ = 0;
  int finished_loading_layout_count_ = 0;

  base::WeakPtrFactory<TestWebFrameClient> weak_factory_{this};
};

// Minimal implementation of WebRemoteFrameClient needed for unit tests that
// load remote frames. Tests that load frames and need further specialization
// of WebLocalFrameClient behavior should subclass this.
class TestWebRemoteFrameClient : public WebRemoteFrameClient {
 public:
  TestWebRemoteFrameClient();
  ~TestWebRemoteFrameClient() override = default;

  WebRemoteFrame* Frame() const { return frame_; }
  // Pass ownership of the TestWebFrameClient to |self_owned| here if the
  // TestWebRemoteFrameClient should delete itself on frame detach.
  void Bind(WebRemoteFrame*,
            std::unique_ptr<TestWebRemoteFrameClient> self_owned = nullptr);

  // WebRemoteFrameClient:
  void FrameDetached(DetachType) override;
  AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override {
    return associated_interface_provider_.get();
  }

 private:
  // If set to a non-null value, self-deletes on frame detach.
  std::unique_ptr<TestWebRemoteFrameClient> self_owned_;

  std::unique_ptr<AssociatedInterfaceProvider> associated_interface_provider_;

  // This is null from when the client is created until it is initialized with
  // Bind().
  WebRemoteFrame* frame_ = nullptr;
};

class TestWidgetInputHandlerHost : public mojom::blink::WidgetInputHandlerHost {
 public:
  mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> BindNewRemote();

  void SetTouchActionFromMain(cc::TouchAction touch_action) override;
  void DidOverscroll(mojom::blink::DidOverscrollParamsPtr params) override;
  void DidStartScrollingViewport() override;
  void ImeCancelComposition() override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const WTF::Vector<gfx::Rect>& bounds) override;
  void SetMouseCapture(bool capture) override;
  void RequestMouseLock(bool from_user_gesture,
                        bool privileged,
                        bool unadjusted_movement,
                        RequestMouseLockCallback callback) override;

 private:
  mojo::Receiver<mojom::blink::WidgetInputHandlerHost> receiver_{this};
};

}  // namespace frame_test_helpers
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_TEST_HELPERS_H_
