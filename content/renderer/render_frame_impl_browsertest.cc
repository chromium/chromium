// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/navigation_params_mojom_traits.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/previews_state.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/frame_host_test_interface.mojom.h"
#include "content/test/test_render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/native_theme/native_theme_features.h"

using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

constexpr int32_t kSubframeRouteId = 20;
constexpr int32_t kSubframeWidgetRouteId = 21;
constexpr int32_t kFrameProxyRouteId = 22;
constexpr int32_t kEmbeddedSubframeRouteId = 23;

const char kParentFrameHTML[] = "Parent frame <iframe name='frame'></iframe>";

const char kAutoplayTestOrigin[] = "https://www.google.com";
}  // namespace

// RenderFrameImplTest creates a RenderFrameImpl that is a child of the
// main frame, and has its own RenderWidget. This behaves like an out
// of process frame even though it is in the same process as its parent.
class RenderFrameImplTest : public RenderViewTest {
 public:
  ~RenderFrameImplTest() override {}

  void SetUp() override {
    blink::WebRuntimeFeatures::EnableOverlayScrollbars(
        ui::IsOverlayScrollbarEnabled());
    RenderViewTest::SetUp();
    EXPECT_TRUE(GetMainRenderFrame()->is_main_frame_);

    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

    LoadHTML(kParentFrameHTML);
    LoadChildFrame();
  }

  void LoadChildFrame() {
    mojom::CreateFrameWidgetParams widget_params;
    widget_params.routing_id = kSubframeWidgetRouteId;
    widget_params.visual_properties.new_size = gfx::Size(100, 100);

    FrameReplicationState frame_replication_state;
    frame_replication_state.name = "frame";
    frame_replication_state.unique_name = "frame-uniqueName";

    RenderFrameImpl::FromWebFrame(
        view_->GetMainRenderFrame()->GetWebFrame()->FirstChild())
        ->OnSwapOut(kFrameProxyRouteId, false, frame_replication_state);

    service_manager::mojom::InterfaceProviderPtr stub_interface_provider;
    mojo::MakeRequest(&stub_interface_provider);

    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        stub_browser_interface_broker;
    ignore_result(
        stub_browser_interface_broker.InitWithNewPipeAndPassReceiver());

    RenderFrameImpl::CreateFrame(
        kSubframeRouteId, std::move(stub_interface_provider),
        std::move(stub_browser_interface_broker), MSG_ROUTING_NONE,
        MSG_ROUTING_NONE, kFrameProxyRouteId, MSG_ROUTING_NONE,
        base::UnguessableToken::Create(), frame_replication_state,
        &compositor_deps_, &widget_params, FrameOwnerProperties(),
        /*has_committed_real_load=*/true);

    frame_ = static_cast<TestRenderFrame*>(
        RenderFrameImpl::FromRoutingID(kSubframeRouteId));
    EXPECT_FALSE(frame_->is_main_frame_);
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  TestRenderFrame* GetMainRenderFrame() {
    return static_cast<TestRenderFrame*>(view_->GetMainRenderFrame());
  }

  TestRenderFrame* frame() { return frame_; }

  content::RenderWidget* frame_widget() const { return frame_->render_widget_; }

  static url::Origin GetOriginForFrame(TestRenderFrame* frame) {
    return url::Origin(frame->GetWebFrame()->GetSecurityOrigin());
  }

  static int32_t AutoplayFlagsForFrame(TestRenderFrame* frame) {
    return frame->render_view()->webview()->AutoplayFlagsForTest();
  }

#if defined(OS_ANDROID)
  void ReceiveOverlayRoutingToken(const base::UnguessableToken& token) {
    overlay_routing_token_ = token;
  }

  base::Optional<base::UnguessableToken> overlay_routing_token_;
#endif

 private:
  TestRenderFrame* frame_;
  FakeCompositorDependencies compositor_deps_;
};

class RenderFrameTestObserver : public RenderFrameObserver {
 public:
  explicit RenderFrameTestObserver(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame), visible_(false) {}

  ~RenderFrameTestObserver() override {}

  // RenderFrameObserver implementation.
  void WasShown() override { visible_ = true; }
  void WasHidden() override { visible_ = false; }
  void OnDestruct() override { delete this; }

  bool visible() { return visible_; }

 private:
  bool visible_;
};

// Verify that a frame with a RenderFrameProxy as a parent has its own
// RenderWidget.
TEST_F(RenderFrameImplTest, SubframeWidget) {
  EXPECT_TRUE(frame_widget());
  EXPECT_NE(frame_widget(), static_cast<RenderViewImpl*>(view_)->GetWidget());
}

// Verify a subframe RenderWidget properly processes its viewport being
// resized.
TEST_F(RenderFrameImplTest, FrameResize) {
  // Make an update where the widget's size and the visible_viewport_size
  // are not the same.
  VisualProperties visual_properties;
  gfx::Size widget_size(400, 200);
  gfx::Size visible_size(350, 170);
  visual_properties.new_size = widget_size;
  visual_properties.compositor_viewport_pixel_rect = gfx::Rect(widget_size);
  visual_properties.visible_viewport_size = visible_size;

  RenderWidget* main_frame_widget =
      GetMainRenderFrame()->GetLocalRootRenderWidget();

  // The main frame's widget will receive the resize message before the
  // subframe's widget, and it will set the size for the WebView.
  {
    WidgetMsg_UpdateVisualProperties resize_message(
        main_frame_widget->routing_id(), visual_properties);
    main_frame_widget->OnMessageReceived(resize_message);
  }
  // The main frame widget's size is the "widget size", not the visible viewport
  // size, which is given to blink separately.
  EXPECT_EQ(gfx::Size(view_->GetWebView()->MainFrameWidget()->Size()),
            widget_size);
  EXPECT_EQ(gfx::SizeF(view_->GetWebView()->VisualViewportSize()),
            gfx::SizeF(visible_size));
  // The main frame doesn't change other local roots directly.
  EXPECT_NE(gfx::Size(frame_widget()->GetWebWidget()->Size()), visible_size);

  // A subframe in the same process does not modify the WebView.
  {
    WidgetMsg_UpdateVisualProperties resize_message_subframe(
        frame_widget()->routing_id(), visual_properties);
    frame_widget()->OnMessageReceived(resize_message_subframe);
  }
  EXPECT_EQ(gfx::Size(frame_widget()->GetWebWidget()->Size()), widget_size);

  // A subframe in another process would use the |visible_viewport_size| as its
  // size.
}

// Verify a subframe RenderWidget properly processes a WasShown message.
TEST_F(RenderFrameImplTest, FrameWasShown) {
  RenderFrameTestObserver observer(frame());

  WidgetMsg_WasShown was_shown_message(
      0, base::TimeTicks(), false /* was_evicted */,
      base::nullopt /* tab_switch_start_state */);
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

// Verify that a local subframe of a frame with a RenderWidget processes a
// WasShown message.
TEST_F(RenderFrameImplTest, LocalChildFrameWasShown) {
  service_manager::mojom::InterfaceProviderPtr stub_interface_provider;
  mojo::MakeRequest(&stub_interface_provider);
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      stub_browser_interface_broker;
  ignore_result(stub_browser_interface_broker.InitWithNewPipeAndPassReceiver());

  // Create and initialize a local child frame of the simulated OOPIF, which
  // is a grandchild of the remote main frame.
  RenderFrameImpl* grandchild =
      RenderFrameImpl::Create(frame()->render_view(), kEmbeddedSubframeRouteId,
                              std::move(stub_interface_provider),
                              std::move(stub_browser_interface_broker),
                              base::UnguessableToken::Create());
  blink::WebLocalFrame* parent_web_frame = frame()->GetWebFrame();

  parent_web_frame->CreateLocalChild(
      blink::WebTreeScopeType::kDocument, grandchild,
      grandchild->blink_interface_registry_.get());
  grandchild->in_frame_tree_ = true;
  grandchild->Initialize();

  EXPECT_EQ(grandchild->GetLocalRootRenderWidget(),
            frame()->GetLocalRootRenderWidget());

  RenderFrameTestObserver observer(grandchild);

  WidgetMsg_WasShown was_shown_message(
      0, base::TimeTicks(), false /* was_evicted */,
      base::nullopt /* tab_switch_start_state */);
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

TEST_F(RenderFrameImplTest, SaveImageFromDataURL) {
  const IPC::Message* msg1 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_FALSE(msg1);
  render_thread_->sink().ClearMessages();

  const std::string image_data_url =
      "data:image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";

  frame()->SaveImageFromDataURL(WebString::FromUTF8(image_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg2 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_TRUE(msg2);

  FrameHostMsg_DownloadUrl::Param param1;
  FrameHostMsg_DownloadUrl::Read(msg2, &param1);
  EXPECT_EQ(std::get<0>(param1).url, GURL());

  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const std::string large_data_url(1024 * 1024 * 20, 'd');

  frame()->SaveImageFromDataURL(WebString::FromUTF8(large_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg3 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_TRUE(msg3);

  FrameHostMsg_DownloadUrl::Param param2;
  FrameHostMsg_DownloadUrl::Read(msg3, &param2);
  EXPECT_EQ(std::get<0>(param2).url, GURL());

  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();
}

// Tests that url download are throttled when reaching the limit.
TEST_F(RenderFrameImplTest, DownloadUrlLimit) {
  const IPC::Message* msg1 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_FALSE(msg1);
  render_thread_->sink().ClearMessages();

  WebURLRequest request;
  request.SetUrl(GURL("http://test/test.pdf"));
  request.SetRequestorOrigin(
      blink::WebSecurityOrigin::Create(GURL("http://test")));

  for (int i = 0; i < 10; ++i) {
    frame()->DownloadURL(request, network::mojom::RedirectMode::kManual,
                         mojo::ScopedMessagePipeHandle());
    base::RunLoop().RunUntilIdle();
    const IPC::Message* msg2 = render_thread_->sink().GetFirstMessageMatching(
        FrameHostMsg_DownloadUrl::ID);
    EXPECT_TRUE(msg2);
    base::RunLoop().RunUntilIdle();
    render_thread_->sink().ClearMessages();
  }

  frame()->DownloadURL(request, network::mojom::RedirectMode::kManual,
                       mojo::ScopedMessagePipeHandle());
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg3 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_FALSE(msg3);
}

// Regression test for crbug.com/692557. It shouldn't crash if we inititate a
// text finding, and then delete the frame immediately before the text finding
// returns any text match.
TEST_F(RenderFrameImplTest, NoCrashWhenDeletingFrameDuringFind) {
  frame()->GetWebFrame()->FindForTesting(
      1, "foo", true /* match_case */, true /* forward */,
      false /* find_next */, true /* force */, false /* wrap_within_frame */);

  UnfreezableFrameMsg_Delete delete_message(
      0, FrameDeleteIntention::kNotMainFrame);
  frame()->OnMessageReceived(delete_message);
}

#if defined(OS_ANDROID)
// Verify that RFI defers token requests if the token hasn't arrived yet.
TEST_F(RenderFrameImplTest, TestOverlayRoutingTokenSendsLater) {
  ASSERT_FALSE(overlay_routing_token_.has_value());

  frame()->RequestOverlayRoutingToken(
      base::BindOnce(&RenderFrameImplTest::ReceiveOverlayRoutingToken,
                     base::Unretained(this)));
  ASSERT_FALSE(overlay_routing_token_.has_value());

  // The host should receive a request for it sent to the frame.
  const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_RequestOverlayRoutingToken::ID);
  EXPECT_TRUE(msg);

  // Send a token.
  base::UnguessableToken token = base::UnguessableToken::Create();
  FrameMsg_SetOverlayRoutingToken token_message(0, token);
  frame()->OnMessageReceived(token_message);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(overlay_routing_token_.has_value());
  ASSERT_EQ(overlay_routing_token_.value(), token);
}

// Verify that RFI sends tokens if they're already available.
TEST_F(RenderFrameImplTest, TestOverlayRoutingTokenSendsNow) {
  ASSERT_FALSE(overlay_routing_token_.has_value());
  base::UnguessableToken token = base::UnguessableToken::Create();
  FrameMsg_SetOverlayRoutingToken token_message(0, token);
  frame()->OnMessageReceived(token_message);

  // The frame now has a token.  We don't care if it sends the token before
  // returning or posts a message.
  base::RunLoop().RunUntilIdle();
  frame()->RequestOverlayRoutingToken(
      base::BindOnce(&RenderFrameImplTest::ReceiveOverlayRoutingToken,
                     base::Unretained(this)));
  ASSERT_TRUE(overlay_routing_token_.has_value());
  ASSERT_EQ(overlay_routing_token_.value(), token);

  // Since the token already arrived, a request for it shouldn't be sent.
  const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_RequestOverlayRoutingToken::ID);
  EXPECT_FALSE(msg);
}
#endif

TEST_F(RenderFrameImplTest, AutoplayFlags) {
  // Add autoplay flags to the page.
  GetMainRenderFrame()->AddAutoplayFlags(
      url::Origin::Create(GURL(kAutoplayTestOrigin)),
      blink::mojom::kAutoplayFlagHighMediaEngagement);

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, kAutoplayTestOrigin);

  // Check the flags have been set correctly.
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(GetMainRenderFrame()));

  // Navigate the child frame.
  LoadChildFrame();

  // Check the flags are set on both frames.
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(GetMainRenderFrame()));
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(frame()));

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, "https://www.example.com");
  LoadChildFrame();

  // Check the flags have been cleared.
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(GetMainRenderFrame()));
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone, AutoplayFlagsForFrame(frame()));
}

TEST_F(RenderFrameImplTest, AutoplayFlags_WrongOrigin) {
  // Add autoplay flags to the page.
  GetMainRenderFrame()->AddAutoplayFlags(
      url::Origin(), blink::mojom::kAutoplayFlagHighMediaEngagement);

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, kAutoplayTestOrigin);

  // Check the flags have been not been set.
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(GetMainRenderFrame()));
}

TEST_F(RenderFrameImplTest, FileUrlPathAlias) {
  const struct {
    const char* original;
    const char* transformed;
  } kTestCases[] = {
      {"file:///alias", "file:///replacement"},
      {"file:///alias/path/to/file", "file:///replacement/path/to/file"},
      {"file://alias/path/to/file", "file://alias/path/to/file"},
      {"file:///notalias/path/to/file", "file:///notalias/path/to/file"},
      {"file:///root/alias/path/to/file", "file:///root/alias/path/to/file"},
      {"file:///", "file:///"},
  };
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFileUrlPathAlias, "/alias=/replacement");

  for (const auto& test_case : kTestCases) {
    WebURLRequest request;
    request.SetUrl(GURL(test_case.original));
    GetMainRenderFrame()->WillSendRequest(request);
    EXPECT_EQ(test_case.transformed, request.Url().GetString().Utf8());
  }
}

// Used to annotate the source of an interface request.
struct SourceAnnotation {
  // The URL of the active document in the frame, at the time the interface was
  // requested by the RenderFrame.
  GURL document_url;

  // The RenderFrameObserver event in response to which the interface is
  // requested by the RenderFrame.
  std::string render_frame_event;

  bool operator==(const SourceAnnotation& rhs) const {
    return document_url == rhs.document_url &&
           render_frame_event == rhs.render_frame_event;
  }
};

// RenderFrameRemoteInterfacesTest ------------------------------------

namespace {

constexpr char kTestFirstURL[] = "http://foo.com/1";
constexpr char kTestSecondURL[] = "http://foo.com/2";
// constexpr char kTestCrossOriginURL[] = "http://bar.com/";
constexpr char kAboutBlankURL[] = "about:blank";

constexpr char kFrameEventDidCreateNewFrame[] = "did-create-new-frame";
constexpr char kFrameEventDidCreateNewDocument[] = "did-create-new-document";
constexpr char kFrameEventDidCreateDocumentElement[] =
    "did-create-document-element";
constexpr char kFrameEventReadyToCommitNavigation[] =
    "ready-to-commit-navigation";
constexpr char kFrameEventDidCommitProvisionalLoad[] =
    "did-commit-provisional-load";
constexpr char kFrameEventDidCommitSameDocumentLoad[] =
    "did-commit-same-document-load";
constexpr char kFrameEventAfterCommit[] = "after-commit";

constexpr char kNoDocumentMarkerURL[] = "data:,No document.";

// A simple testing implementation of mojom::InterfaceProvider that binds
// interface requests only for one hard-coded kind of interface.
class TestSimpleInterfaceProviderImpl
    : public service_manager::mojom::InterfaceProvider {
 public:
  using BinderCallback =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

  // Incoming interface requests for |interface_name| will invoke |binder|.
  // Everything else is ignored.
  TestSimpleInterfaceProviderImpl(const std::string& interface_name,
                                  BinderCallback binder_callback)
      : binding_(this),
        interface_name_(interface_name),
        binder_callback_(binder_callback) {}

  void BindAndFlush(service_manager::mojom::InterfaceProviderRequest request) {
    ASSERT_FALSE(binding_.is_bound());
    binding_.Bind(std::move(request));
    binding_.FlushForTesting();
  }

 private:
  // mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (interface_name == interface_name_)
      binder_callback_.Run(std::move(handle));
  }

  mojo::Binding<service_manager::mojom::InterfaceProvider> binding_;

  std::string interface_name_;
  BinderCallback binder_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSimpleInterfaceProviderImpl);
};

class TestSimpleBrowserInterfaceBrokerImpl
    : public blink::mojom::BrowserInterfaceBroker {
 public:
  using BinderCallback =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

  // Incoming interface requests for |interface_name| will invoke |binder|.
  // Everything else is ignored.
  TestSimpleBrowserInterfaceBrokerImpl(const std::string& interface_name,
                                       BinderCallback binder_callback)
      : receiver_(this),
        interface_name_(interface_name),
        binder_callback_(binder_callback) {}

  void BindAndFlush(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
    ASSERT_FALSE(receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
    receiver_.FlushForTesting();
  }

 private:
  // blink::mojom::BrowserInterfaceBroker:
  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    if (receiver.interface_name().value() == interface_name_)
      binder_callback_.Run(receiver.PassPipe());
  }

  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> receiver_;

  std::string interface_name_;
  BinderCallback binder_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSimpleBrowserInterfaceBrokerImpl);
};

class FrameHostTestInterfaceImpl : public mojom::FrameHostTestInterface {
 public:
  FrameHostTestInterfaceImpl() = default;
  ~FrameHostTestInterfaceImpl() override {}

  void BindAndFlush(
      mojo::PendingReceiver<mojom::FrameHostTestInterface> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.WaitForIncomingCall();
  }

  const base::Optional<SourceAnnotation>& ping_source() const {
    return ping_source_;
  }

 protected:
  void Ping(const GURL& url, const std::string& event) override {
    ping_source_ = SourceAnnotation{url, event};
  }

 private:
  mojo::Receiver<mojom::FrameHostTestInterface> receiver_{this};
  base::Optional<SourceAnnotation> ping_source_;

  DISALLOW_COPY_AND_ASSIGN(FrameHostTestInterfaceImpl);
};

// RenderFrameObserver that issues FrameHostTestInterface interface requests
// through the RenderFrame's |remote_interfaces_| in response to observing
// important milestones in a frame's lifecycle.
class FrameHostTestInterfaceRequestIssuer : public RenderFrameObserver {
 public:
  explicit FrameHostTestInterfaceRequestIssuer(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame) {}

  void RequestTestInterfaceOnFrameEvent(const std::string& event) {
    mojo::Remote<mojom::FrameHostTestInterface> remote;
    render_frame()->GetRemoteInterfaces()->GetInterface(
        remote.BindNewPipeAndPassReceiver());

    blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
    remote->Ping(
        !document.IsNull() ? GURL(document.Url()) : GURL(kNoDocumentMarkerURL),
        event);

    remote.reset();
    render_frame()->GetBrowserInterfaceBroker()->GetInterface(
        remote.BindNewPipeAndPassReceiver());
    remote->Ping(
        !document.IsNull() ? GURL(document.Url()) : GURL(kNoDocumentMarkerURL),
        event);
  }

 private:
  // RenderFrameObserver:
  void OnDestruct() override {}

  void DidCreateDocumentElement() override {
    RequestTestInterfaceOnFrameEvent(kFrameEventDidCreateDocumentElement);
  }

  void DidCreateNewDocument() override {
    RequestTestInterfaceOnFrameEvent(kFrameEventDidCreateNewDocument);
  }

  void ReadyToCommitNavigation(blink::WebDocumentLoader* loader) override {
    RequestTestInterfaceOnFrameEvent(kFrameEventReadyToCommitNavigation);
  }

  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    RequestTestInterfaceOnFrameEvent(is_same_document_navigation
                                         ? kFrameEventDidCommitSameDocumentLoad
                                         : kFrameEventDidCommitProvisionalLoad);
  }

  DISALLOW_COPY_AND_ASSIGN(FrameHostTestInterfaceRequestIssuer);
};

// RenderFrameObserver that can be used to wait for the next commit in a frame.
class FrameCommitWaiter : public RenderFrameObserver {
 public:
  explicit FrameCommitWaiter(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame) {}

  void Wait() {
    if (did_commit_)
      return;
    run_loop_.Run();
  }

 private:
  // RenderFrameObserver:
  void OnDestruct() override {}

  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    did_commit_ = true;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool did_commit_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameCommitWaiter);
};

// Testing ContentRendererClient implementation that fires the |callback|
// whenever a new frame is created.
class FrameCreationObservingRendererClient : public ContentRendererClient {
 public:
  using FrameCreatedCallback = base::RepeatingCallback<void(TestRenderFrame*)>;

  FrameCreationObservingRendererClient() {}
  ~FrameCreationObservingRendererClient() override {}

  void set_callback(FrameCreatedCallback callback) {
    callback_ = std::move(callback);
  }

  void reset_callback() { callback_.Reset(); }

 protected:
  void RenderFrameCreated(RenderFrame* render_frame) override {
    ContentRendererClient::RenderFrameCreated(render_frame);
    if (callback_)
      callback_.Run(static_cast<TestRenderFrame*>(render_frame));
  }

 private:
  FrameCreatedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(FrameCreationObservingRendererClient);
};

// Expects observing the creation of a new frame, and creates an instance of
// FrameHostTestInterfaceRequestIssuerRenderFrame for that new frame to exercise
// its RemoteInterfaceProvider interface.
class ScopedNewFrameInterfaceProviderExerciser {
 public:
  explicit ScopedNewFrameInterfaceProviderExerciser(
      FrameCreationObservingRendererClient* frame_creation_observer,
      const base::Optional<std::string>& html_override_for_first_load =
          base::nullopt)
      : frame_creation_observer_(frame_creation_observer),
        html_override_for_first_load_(html_override_for_first_load) {
    frame_creation_observer_->set_callback(base::BindRepeating(
        &ScopedNewFrameInterfaceProviderExerciser::OnFrameCreated,
        base::Unretained(this)));
  }

  ~ScopedNewFrameInterfaceProviderExerciser() {
    frame_creation_observer_->reset_callback();
  }

  void ExpectNewFrameAndWaitForLoad(const GURL& expected_loaded_url) {
    ASSERT_NE(nullptr, frame_);
    frame_commit_waiter_->Wait();

    ASSERT_FALSE(frame_->current_history_item().IsNull());
    ASSERT_FALSE(frame_->GetWebFrame()->GetDocument().IsNull());
    EXPECT_EQ(expected_loaded_url,
              GURL(frame_->GetWebFrame()->GetDocument().Url()));

    interface_request_for_first_document_ =
        frame_->TakeLastInterfaceProviderRequest();

    browser_interface_broker_receiver_for_first_document_ =
        frame_->TakeLastBrowserInterfaceBrokerReceiver();
  }

  service_manager::mojom::InterfaceProviderRequest
  interface_request_for_initial_empty_document() {
    return std::move(interface_request_for_initial_empty_document_);
  }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  browser_interface_broker_receiver_for_initial_empty_document() {
    return std::move(
        browser_interface_broker_receiver_for_initial_empty_document_);
  }

  service_manager::mojom::InterfaceProviderRequest
  interface_request_for_first_document() {
    return std::move(interface_request_for_first_document_);
  }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  browser_interface_broker_receiver_for_first_document() {
    return std::move(browser_interface_broker_receiver_for_first_document_);
  }

 private:
  void OnFrameCreated(TestRenderFrame* frame) {
    ASSERT_EQ(nullptr, frame_);
    frame_ = frame;
    frame_commit_waiter_.emplace(frame);

    if (html_override_for_first_load_.has_value()) {
      frame_->SetHTMLOverrideForNextNavigation(
          std::move(html_override_for_first_load_).value());
    }

    // The FrameHostTestInterfaceRequestIssuer needs to stay alive even after
    // this method returns, so that it continues to observe RenderFrame
    // lifecycle events and request test interfaces in response.
    test_request_issuer_.emplace(frame);
    test_request_issuer_->RequestTestInterfaceOnFrameEvent(
        kFrameEventDidCreateNewFrame);

    interface_request_for_initial_empty_document_ =
        frame->TakeLastInterfaceProviderRequest();
    browser_interface_broker_receiver_for_initial_empty_document_ =
        frame_->TakeLastBrowserInterfaceBrokerReceiver();
    EXPECT_TRUE(frame->current_history_item().IsNull());
  }

  FrameCreationObservingRendererClient* frame_creation_observer_;
  TestRenderFrame* frame_ = nullptr;
  base::Optional<std::string> html_override_for_first_load_;
  GURL first_committed_url_;

  base::Optional<FrameCommitWaiter> frame_commit_waiter_;
  base::Optional<FrameHostTestInterfaceRequestIssuer> test_request_issuer_;

  service_manager::mojom::InterfaceProviderRequest
      interface_request_for_initial_empty_document_;
  service_manager::mojom::InterfaceProviderRequest
      interface_request_for_first_document_;

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver_for_initial_empty_document_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver_for_first_document_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNewFrameInterfaceProviderExerciser);
};

// Extracts all interface requests for FrameHostTestInterface pending on the
// specified |interface_provider_request|, and returns a list of the source
// annotations that are provided in the pending Ping() call for each of these
// FrameHostTestInterface requests.
void ExpectPendingInterfaceRequestsFromSources(
    service_manager::mojom::InterfaceProviderRequest interface_provider_request,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    std::vector<SourceAnnotation> expected_sources) {
  std::vector<SourceAnnotation> sources;
  ASSERT_TRUE(interface_provider_request.is_pending());
  TestSimpleInterfaceProviderImpl provider(
      mojom::FrameHostTestInterface::Name_,
      base::BindLambdaForTesting(
          [&sources](mojo::ScopedMessagePipeHandle handle) {
            FrameHostTestInterfaceImpl impl;
            impl.BindAndFlush(
                mojo::PendingReceiver<mojom::FrameHostTestInterface>(
                    std::move(handle)));
            ASSERT_TRUE(impl.ping_source().has_value());
            sources.push_back(impl.ping_source().value());
          }));
  provider.BindAndFlush(std::move(interface_provider_request));
  EXPECT_THAT(sources, ::testing::ElementsAreArray(expected_sources));

  std::vector<SourceAnnotation> browser_interface_broker_sources;
  ASSERT_TRUE(browser_interface_broker_receiver.is_valid());
  TestSimpleBrowserInterfaceBrokerImpl browser_broker(
      mojom::FrameHostTestInterface::Name_,
      base::BindLambdaForTesting([&browser_interface_broker_sources](
                                     mojo::ScopedMessagePipeHandle handle) {
        FrameHostTestInterfaceImpl impl;
        impl.BindAndFlush(mojo::PendingReceiver<mojom::FrameHostTestInterface>(
            std::move(handle)));
        ASSERT_TRUE(impl.ping_source().has_value());
        browser_interface_broker_sources.push_back(impl.ping_source().value());
      }));
  browser_broker.BindAndFlush(std::move(browser_interface_broker_receiver));
  EXPECT_THAT(browser_interface_broker_sources,
              ::testing::ElementsAreArray(expected_sources));
}

}  // namespace

class RenderFrameRemoteInterfacesTest : public RenderViewTest {
 public:
  RenderFrameRemoteInterfacesTest() {}
  ~RenderFrameRemoteInterfacesTest() override {}

 protected:
  void SetUp() override {
    RenderViewTest::SetUp();
    LoadHTML("Nothing to see here.");
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
    // Do this before shutting down V8 in RenderViewTest::TearDown().
    // http://crbug.com/328552
    __lsan_do_leak_check();
#endif
    RenderViewTest::TearDown();
  }

  FrameCreationObservingRendererClient* frame_creation_observer() {
    DCHECK(frame_creation_observer_);
    return frame_creation_observer_;
  }

  TestRenderFrame* GetMainRenderFrame() {
    return static_cast<TestRenderFrame*>(view_->GetMainRenderFrame());
  }

  ContentRendererClient* CreateContentRendererClient() override {
    frame_creation_observer_ = new FrameCreationObservingRendererClient();
    return frame_creation_observer_;
  }

 private:
  // Owned by RenderViewTest.
  FrameCreationObservingRendererClient* frame_creation_observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameRemoteInterfacesTest);
};

// Expect that |remote_interfaces_| is bound before the first committed load in
// a child frame, and then re-bound on the first commit.
// TODO(crbug.com/718652): when all clients are converted to use
// BrowserInterfaceBroker, InterfaceProviderRequest-related code will be
// removed.
TEST_F(RenderFrameRemoteInterfacesTest, ChildFrameAtFirstCommittedLoad) {
  ScopedNewFrameInterfaceProviderExerciser child_frame_exerciser(
      frame_creation_observer());
  const std::string html = base::StringPrintf("<iframe src=\"%s\"></iframe>",
                                              "data:text/html,Child");
  LoadHTMLWithUrlOverride(html.c_str(), kTestSecondURL);

  const GURL child_frame_url("data:text/html,Child");
  ASSERT_NO_FATAL_FAILURE(
      child_frame_exerciser.ExpectNewFrameAndWaitForLoad(child_frame_url));

  // TODO(https://crbug.com/792410): It is unfortunate how many internal
  // details of frame/document creation this encodes. Need to decouple.
  const GURL initial_empty_url(kAboutBlankURL);
  ExpectPendingInterfaceRequestsFromSources(
      child_frame_exerciser.interface_request_for_initial_empty_document(),
      child_frame_exerciser
          .browser_interface_broker_receiver_for_initial_empty_document(),
      {{GURL(kNoDocumentMarkerURL), kFrameEventDidCreateNewFrame},
       {initial_empty_url, kFrameEventDidCreateNewDocument},
       {initial_empty_url, kFrameEventDidCreateDocumentElement},
       {child_frame_url, kFrameEventReadyToCommitNavigation},
       // TODO(https://crbug.com/555773): It seems strange that the new
       // document is created and DidCreateNewDocument is invoked *before* the
       // provisional load would have even committed.
       {child_frame_url, kFrameEventDidCreateNewDocument}});
  ExpectPendingInterfaceRequestsFromSources(
      child_frame_exerciser.interface_request_for_first_document(),
      child_frame_exerciser
          .browser_interface_broker_receiver_for_first_document(),
      {{child_frame_url, kFrameEventDidCommitProvisionalLoad},
       {child_frame_url, kFrameEventDidCreateDocumentElement}});
}

// Expect that |remote_interfaces_| is bound before the first committed load in
// the main frame of an opened window, and then re-bound on the first commit.
// TODO(crbug.com/718652): when all clients are converted to use
// BrowserInterfaceBroker, InterfaceProviderRequest-related code will be
// removed.
TEST_F(RenderFrameRemoteInterfacesTest,
       MainFrameOfOpenedWindowAtFirstCommittedLoad) {
  const GURL new_window_url("data:text/html,NewWindow");
  ScopedNewFrameInterfaceProviderExerciser main_frame_exerciser(
      frame_creation_observer(), std::string("foo"));
  const std::string html =
      base::StringPrintf("<script>window.open(\"%s\", \"_blank\")</script>",
                         "data:text/html,NewWindow");
  LoadHTMLWithUrlOverride(html.c_str(), kTestSecondURL);
  ASSERT_NO_FATAL_FAILURE(
      main_frame_exerciser.ExpectNewFrameAndWaitForLoad(new_window_url));

  // The URL of the initial empty document is "" for opened windows, in
  // contrast to child frames, where it is "about:blank". See
  // Document::Document and Document::SetURL for more details.
  //
  // Furthermore, for main frames, InitializeCoreFrame is invoked first, and
  // RenderFrameImpl::Initialize is invoked second, in contrast to child
  // frames where it is vice versa. ContentRendererClient::RenderFrameCreated
  // is invoked from RenderFrameImpl::Initialize, so we miss the events
  // related to initial empty document that is created from
  // InitializeCoreFrame, and there is already a document when
  // RenderFrameCreated is invoked.
  //
  // TODO(https://crbug.com/792410): It is unfortunate how many internal
  // details of frame/document creation this encodes. Need to decouple.
  const GURL initial_empty_url;
  ExpectPendingInterfaceRequestsFromSources(
      main_frame_exerciser.interface_request_for_initial_empty_document(),
      main_frame_exerciser
          .browser_interface_broker_receiver_for_initial_empty_document(),
      {{initial_empty_url, kFrameEventDidCreateNewFrame},
       {new_window_url, kFrameEventReadyToCommitNavigation},
       {new_window_url, kFrameEventDidCreateNewDocument}});
  ExpectPendingInterfaceRequestsFromSources(
      main_frame_exerciser.interface_request_for_first_document(),
      main_frame_exerciser
          .browser_interface_broker_receiver_for_first_document(),
      {{new_window_url, kFrameEventDidCommitProvisionalLoad},
       {new_window_url, kFrameEventDidCreateDocumentElement}});
}

// Expect that |remote_interfaces_| is not bound to a new pipe if the first
// committed load in the child frame has the same security origin as that of the
// initial empty document.
//
// In this case, the LocalDOMWindow object associated with the initial empty
// document will be re-used for the newly committed document. Here, we must
// continue using the InterfaceProvider connection created for the initial empty
// document to support the following use-case:
//  1) Parent frame dynamically injects an <iframe>.
//  2) The parent frame calls `child.contentDocument.write(...)` to inject
//     Javascript that may stash objects on the child frame's global object
//     (LocalDOMWindow). Internally, these objects may be using Mojo services
//     exposed by the RenderFrameHost. The InterfaceRequests for these services
//     might still be en-route to the RemnderFrameHost's InterfaceProvider.
//  3) The `child` frame commits the first real load, and it is same-origin.
//  4) The global object in the child frame's browsing context is re-used.
//  5) Javascript objects stashed on the global object should continue to work.
//
// TODO(japhet?): javascript: urls are untestable here, because they don't
// go through the normal commit pipeline. If we were to give javascript: urls
// their own DocumentLoader in blink and model them as a real navigation, we
// should add a test case here.
// TODO(crbug.com/718652): when all clients are converted to use
// BrowserInterfaceBroker, InterfaceProviderRequest-related code will be
// removed.
TEST_F(RenderFrameRemoteInterfacesTest,
       ChildFrameReusingWindowOfInitialDocument) {
  const GURL main_frame_url(kTestFirstURL);
  const GURL initial_empty_url(kAboutBlankURL);

  constexpr const char* kTestCases[] = {kTestSecondURL, kAboutBlankURL};

  for (const char* test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << "child_frame_url = " << test_case);

    // Override the URL for the first navigation in the newly created frame to
    // |child_frame_url|.
    const GURL child_frame_url(test_case);
    ScopedNewFrameInterfaceProviderExerciser child_frame_exerciser(
        frame_creation_observer(), std::string("foo"));

    std::string html = "<iframe src='" + child_frame_url.spec() + "'></iframe>";
    LoadHTMLWithUrlOverride(html.c_str(), main_frame_url.spec().c_str());

    ASSERT_NO_FATAL_FAILURE(
        child_frame_exerciser.ExpectNewFrameAndWaitForLoad(child_frame_url));

    ExpectPendingInterfaceRequestsFromSources(
        child_frame_exerciser.interface_request_for_initial_empty_document(),
        child_frame_exerciser
            .browser_interface_broker_receiver_for_initial_empty_document(),
        {{GURL(kNoDocumentMarkerURL), kFrameEventDidCreateNewFrame},
         {initial_empty_url, kFrameEventDidCreateNewDocument},
         {initial_empty_url, kFrameEventDidCreateDocumentElement},
         {child_frame_url, kFrameEventReadyToCommitNavigation},
         {child_frame_url, kFrameEventDidCreateNewDocument},
         {child_frame_url, kFrameEventDidCommitProvisionalLoad},
         {child_frame_url, kFrameEventDidCreateDocumentElement}});

    auto request = child_frame_exerciser.interface_request_for_first_document();
    ASSERT_FALSE(request.is_pending());
    auto browser_interface_broker_receiver =
        child_frame_exerciser
            .browser_interface_broker_receiver_for_first_document();
    ASSERT_FALSE(browser_interface_broker_receiver.is_valid());
  }
}

// Expect that |remote_interfaces_| is bound to a new pipe on cross-document
// navigations.
// TODO(crbug.com/718652): when all clients are converted to use
// BrowserInterfaceBroker, InterfaceProviderRequest-related code will be
// removed.
TEST_F(RenderFrameRemoteInterfacesTest, ReplacedOnNonSameDocumentNavigation) {
  LoadHTMLWithUrlOverride("", kTestFirstURL);

  auto interface_provider_request_for_first_document =
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest();

  auto browser_interface_broker_receiver_for_first_document =
      GetMainRenderFrame()->TakeLastBrowserInterfaceBrokerReceiver();

  FrameHostTestInterfaceRequestIssuer requester(GetMainRenderFrame());
  requester.RequestTestInterfaceOnFrameEvent(kFrameEventAfterCommit);

  LoadHTMLWithUrlOverride("", kTestSecondURL);

  auto interface_provider_request_for_second_document =
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest();

  auto browser_interface_broker_receiver_for_second_document =
      GetMainRenderFrame()->TakeLastBrowserInterfaceBrokerReceiver();

  ASSERT_TRUE(interface_provider_request_for_first_document.is_pending());
  ASSERT_TRUE(browser_interface_broker_receiver_for_first_document.is_valid());

  ExpectPendingInterfaceRequestsFromSources(
      std::move(interface_provider_request_for_first_document),
      std::move(browser_interface_broker_receiver_for_first_document),
      {{GURL(kTestFirstURL), kFrameEventAfterCommit},
       {GURL(kTestSecondURL), kFrameEventReadyToCommitNavigation},
       {GURL(kTestSecondURL), kFrameEventDidCreateNewDocument}});

  ASSERT_TRUE(interface_provider_request_for_second_document.is_pending());
  ASSERT_TRUE(browser_interface_broker_receiver_for_second_document.is_valid());

  ExpectPendingInterfaceRequestsFromSources(
      std::move(interface_provider_request_for_second_document),
      std::move(browser_interface_broker_receiver_for_second_document),
      {{GURL(kTestSecondURL), kFrameEventDidCommitProvisionalLoad},
       {GURL(kTestSecondURL), kFrameEventDidCreateDocumentElement}});
}

// Expect that |remote_interfaces_| is not bound to a new pipe on same-document
// navigations, i.e. the existing InterfaceProvider connection is continued to
// be used.
// TODO(crbug.com/718652): when all clients are converted to use
// BrowserInterfaceBroker, InterfaceProviderRequest-related code will be
// removed.
TEST_F(RenderFrameRemoteInterfacesTest, ReusedOnSameDocumentNavigation) {
  LoadHTMLWithUrlOverride("", kTestFirstURL);

  auto interface_provider_request =
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest();

  auto browser_interface_broker_receiver =
      GetMainRenderFrame()->TakeLastBrowserInterfaceBrokerReceiver();

  FrameHostTestInterfaceRequestIssuer requester(GetMainRenderFrame());
  OnSameDocumentNavigation(GetMainFrame(), true /* is_new_navigation */);

  EXPECT_FALSE(
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest().is_pending());

  ASSERT_TRUE(interface_provider_request.is_pending());
  ASSERT_TRUE(browser_interface_broker_receiver.is_valid());

  ExpectPendingInterfaceRequestsFromSources(
      std::move(interface_provider_request),
      std::move(browser_interface_broker_receiver),
      {{GURL(kTestFirstURL), kFrameEventDidCommitSameDocumentLoad}});
}

}  // namespace content
