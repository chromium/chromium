// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_impl.h"

#include <stdint.h>

#include <optional>
#include <tuple>
#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/protected_memory_buildflags.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/features.h"
#include "content/common/renderer.mojom.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/local_frame_host_interceptor.h"
#include "content/public/test/policy_container_utils.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/document_state.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/navigation_state.h"
#include "content/test/frame_host_test_interface.mojom.h"
#include "content/test/test_render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/navigation/navigation_params_mojom_traits.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_v8_features.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/point.h"
#include "ui/native_theme/native_theme_features.h"

using blink::WebURLRequest;

namespace content {

namespace {

constexpr int32_t kSubframeRouteId = 20;
constexpr int32_t kSubframeWidgetRouteId = 21;

const char kParentFrameHTML[] = "Parent frame <iframe name='frame'></iframe>";
const char kSimpleScriptHtml[] = "<script>var x = 1;</script>";

const char kAutoplayTestOrigin[] = "https://www.google.com";

}  // namespace

// RenderFrameImplTest creates a RenderFrameImpl that is a child of the
// main frame, and has its own RenderWidget. This behaves like an out
// of process frame even though it is in the same process as its parent.
class RenderFrameImplTest : public RenderViewTest {
 public:
  explicit RenderFrameImplTest(
      RenderFrameImpl::CreateRenderFrameImplFunction hook_function = nullptr)
      : RenderViewTest(/*hook_render_frame_creation=*/!hook_function) {
    if (hook_function) {
      RenderFrameImpl::InstallCreateHook(hook_function);
    }
  }
  ~RenderFrameImplTest() override = default;

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
    child_frame_token_ = blink::LocalFrameToken();
    mojom::CreateFrameWidgetParamsPtr widget_params =
        mojom::CreateFrameWidgetParams::New();
    widget_params->routing_id = kSubframeWidgetRouteId;
    widget_params->visual_properties.new_size = gfx::Size(100, 100);
    widget_params->visual_properties.screen_infos =
        display::ScreenInfos(display::ScreenInfo());

    widget_remote_.reset();
    mojo::PendingAssociatedReceiver<blink::mojom::Widget>
        blink_widget_receiver =
            widget_remote_.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
        blink_widget_host_receiver =
            blink_widget_host.BindNewEndpointAndPassDedicatedReceiver();

    widget_params->widget = std::move(blink_widget_receiver);
    widget_params->widget_host = blink_widget_host.Unbind();

    auto frame_replication_state = blink::mojom::FrameReplicationState::New();
    frame_replication_state->name = "frame";
    frame_replication_state->unique_name = "frame-uniqueName";

    auto remote_frame_interfaces =
        blink::mojom::RemoteFrameInterfacesFromBrowser::New();
    mojo::AssociatedRemote<blink::mojom::RemoteFrame> frame;
    remote_frame_interfaces->frame_receiver =
        frame.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::RemoteFrameHost> frame_host;
    std::ignore = frame_host.BindNewEndpointAndPassDedicatedReceiver();
    remote_frame_interfaces->frame_host = frame_host.Unbind();

    auto remote_main_frame_interfaces =
        blink::mojom::RemoteMainFrameInterfaces::New();
    mojo::AssociatedRemote<blink::mojom::RemoteMainFrame> main_frame;
    remote_main_frame_interfaces->main_frame =
        main_frame.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::RemoteMainFrameHost> main_frame_host;
    std::ignore = main_frame_host.BindNewEndpointAndPassDedicatedReceiver();
    remote_main_frame_interfaces->main_frame_host = main_frame_host.Unbind();

    blink::RemoteFrameToken remote_child_token = blink::RemoteFrameToken();
    RenderFrameImpl::FromWebFrame(
        GetMainRenderFrame()->GetWebFrame()->FirstChild())
        ->Unload(false, frame_replication_state->Clone(), remote_child_token,
                 std::move(remote_frame_interfaces),
                 std::move(remote_main_frame_interfaces));
    MockPolicyContainerHost mock_policy_container_host;
    RenderFrameImpl::CreateFrame(
        *agent_scheduling_group_, child_frame_token_, kSubframeRouteId,
        TestRenderFrame::CreateStubFrameReceiver(),
        TestRenderFrame::CreateStubBrowserInterfaceBrokerRemote(),
        TestRenderFrame::CreateStubAssociatedInterfaceProviderRemote(),
        /*web_view=*/nullptr,
        /*previous_frame_token=*/std::nullopt,
        /*opener_frame_token=*/std::nullopt,
        /*parent_frame_token=*/remote_child_token,
        /*previous_sibling_frame_token=*/std::nullopt,
        base::UnguessableToken::Create(),
        blink::mojom::TreeScopeType::kDocument,
        std::move(frame_replication_state), std::move(widget_params),
        blink::mojom::FrameOwnerProperties::New(),
        /*has_committed_real_load=*/true, blink::DocumentToken(),
        blink::mojom::PolicyContainer::New(
            blink::mojom::PolicyContainerPolicies::New(),
            mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote()),
        /*is_for_nested_main_frame=*/false);

    EXPECT_FALSE(child_frame().is_main_frame_);
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
    return static_cast<TestRenderFrame*>(RenderViewTest::GetMainRenderFrame());
  }

  TestRenderFrame& child_frame() const {
    return CHECK_DEREF(
        static_cast<TestRenderFrame*>(RenderFrameImpl::FromWebFrame(
            blink::WebLocalFrame::FromFrameToken(child_frame_token_))));
  }

  blink::WebFrameWidget* frame_widget() const {
    return child_frame().GetLocalRootWebFrameWidget();
  }

  mojo::AssociatedRemote<blink::mojom::Widget>& widget_remote() {
    return widget_remote_;
  }

  static url::Origin GetOriginForFrame(TestRenderFrame* frame) {
    return url::Origin(frame->GetWebFrame()->GetSecurityOrigin());
  }

  static int32_t AutoplayFlagsForFrame(const TestRenderFrame& frame) {
    return frame.GetWebView()->AutoplayFlagsForTest();
  }

 private:
  mojo::AssociatedRemote<blink::mojom::Widget> widget_remote_;
  blink::LocalFrameToken child_frame_token_;
};

class RenderFrameTestObserver : public RenderFrameObserver {
 public:
  explicit RenderFrameTestObserver(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame),
        visible_(false),
        last_intersection_rect_(-1, -1, -1, -1) {}

  ~RenderFrameTestObserver() override {}

  // RenderFrameObserver implementation.
  void WasShown() override { visible_ = true; }
  void WasHidden() override { visible_ = false; }
  void OnDestruct() override { delete this; }
  void OnMainFrameIntersectionChanged(
      const gfx::Rect& intersection_rect) override {
    last_intersection_rect_ = intersection_rect;
  }
  void OnMainFrameViewportRectangleChanged(
      const gfx::Rect& viewport_rect) override {
    last_viewport_rect_ = viewport_rect;
  }

  bool visible() const { return visible_; }
  gfx::Rect last_intersection_rect() const { return last_intersection_rect_; }
  gfx::Rect last_viewport_rect() const { return last_viewport_rect_; }

 private:
  bool visible_;
  gfx::Rect last_intersection_rect_;
  gfx::Rect last_viewport_rect_;
};

// Verify that a frame with a WebRemoteFrame as a parent has its own
// RenderWidget.
TEST_F(RenderFrameImplTest, SubframeWidget) {
  EXPECT_TRUE(frame_widget());

  RenderFrameImpl* main_frame = GetMainRenderFrame();
  blink::WebFrameWidget* main_frame_widget =
      main_frame->GetLocalRootWebFrameWidget();
  EXPECT_NE(frame_widget(), main_frame_widget);
}

// Verify a subframe RenderWidget properly processes its viewport being
// resized.
TEST_F(RenderFrameImplTest, FrameResize) {
  // Make an update where the widget's size and the visible_viewport_size
  // are not the same.
  blink::VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());
  gfx::Size widget_size(400, 200);
  gfx::Size visible_size(350, 170);
  visual_properties.new_size = widget_size;
  visual_properties.compositor_viewport_pixel_rect = gfx::Rect(widget_size);
  visual_properties.visible_viewport_size = visible_size;

  blink::WebFrameWidget* main_frame_widget =
      GetMainRenderFrame()->GetLocalRootWebFrameWidget();

  // The main frame's widget will receive the resize message before the
  // subframe's widget, and it will set the size for the WebView.
  main_frame_widget->ApplyVisualProperties(visual_properties);
  // The main frame widget's size is the "widget size", not the visible viewport
  // size, which is given to blink separately.
  EXPECT_EQ(gfx::Size(web_view_->MainFrameWidget()->Size()), widget_size);
  EXPECT_EQ(gfx::SizeF(web_view_->VisualViewportSize()),
            gfx::SizeF(visible_size));
  // The main frame doesn't change other local roots directly.
  EXPECT_NE(gfx::Size(frame_widget()->Size()), visible_size);

  // A subframe in the same process does not modify the WebView.
  frame_widget()->ApplyVisualProperties(visual_properties);
  EXPECT_EQ(gfx::Size(frame_widget()->Size()), widget_size);

  // A subframe in another process would use the |visible_viewport_size| as its
  // size.
}

// Verify a subframe RenderWidget properly processes a WasShown message.
TEST_F(RenderFrameImplTest, FrameWasShown) {
  RenderFrameTestObserver observer(&child_frame());

  widget_remote()->WasShown(
      /* was_evicted=*/false,
      blink::mojom::RecordContentToVisibleTimeRequestPtr());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(frame_widget()->IsHidden());
  EXPECT_TRUE(observer.visible());
}

namespace {
class DownloadURLMockLocalFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit DownloadURLMockLocalFrameHost(
      blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}

  MOCK_METHOD3(RunModalAlertDialog,
               void(const std::u16string& alert_message,
                    bool disable_third_party_subframe_suppresion,
                    RunModalAlertDialogCallback callback));
  MOCK_METHOD1(DownloadURL, void(blink::mojom::DownloadURLParamsPtr params));
};

class DownloadURLTestRenderFrame : public TestRenderFrame {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params) {
    return new DownloadURLTestRenderFrame(std::move(params));
  }

  ~DownloadURLTestRenderFrame() override = default;

  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override {
    blink::AssociatedInterfaceProvider* associated_interface_provider =
        RenderFrameImpl::GetRemoteAssociatedInterfaces();

    // Attach our fake local frame host at the very first call to
    // GetRemoteAssociatedInterfaces.
    if (!local_frame_host_) {
      local_frame_host_ = std::make_unique<DownloadURLMockLocalFrameHost>(
          associated_interface_provider);
    }
    return associated_interface_provider;
  }

  DownloadURLMockLocalFrameHost* download_url_mock_local_frame_host() {
    return local_frame_host_.get();
  }

 private:
  explicit DownloadURLTestRenderFrame(RenderFrameImpl::CreateParams params)
      : TestRenderFrame(std::move(params)) {}

  std::unique_ptr<DownloadURLMockLocalFrameHost> local_frame_host_;
};
}  // namespace

class RenderViewImplDownloadURLTest : public RenderFrameImplTest {
 public:
  RenderViewImplDownloadURLTest()
      : RenderFrameImplTest(
            &DownloadURLTestRenderFrame::CreateTestRenderFrame) {}

  DownloadURLMockLocalFrameHost* download_url_mock_local_frame_host() {
    return static_cast<DownloadURLTestRenderFrame*>(&child_frame())
        ->download_url_mock_local_frame_host();
  }
};

// Tests that url download are throttled when reaching the limit.
TEST_F(RenderViewImplDownloadURLTest, DownloadUrlLimit) {
  WebURLRequest request;
  request.SetUrl(GURL("http://test/test.pdf"));
  request.SetRequestorOrigin(
      blink::WebSecurityOrigin::Create(GURL("http://test")));

  EXPECT_CALL(*download_url_mock_local_frame_host(), DownloadURL(testing::_))
      .Times(10);
  for (int i = 0; i < 10; ++i) {
    child_frame().GetWebFrame()->DownloadURL(
        request, network::mojom::RedirectMode::kManual, mojo::NullRemote());
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_CALL(*download_url_mock_local_frame_host(), DownloadURL(testing::_))
      .Times(0);
  child_frame().GetWebFrame()->DownloadURL(
      request, network::mojom::RedirectMode::kManual, mojo::NullRemote());
  base::RunLoop().RunUntilIdle();
}

// Regression test for crbug.com/692557. It shouldn't crash if we inititate a
// text finding, and then delete the frame immediately before the text finding
// returns any text match.
TEST_F(RenderFrameImplTest, NoCrashWhenDeletingFrameDuringFind) {
  child_frame().GetWebFrame()->FindForTesting(
      1, "foo", true /* match_case */, true /* forward */,
      true /* new_session */, true /* force */, false /* wrap_within_frame */,
      false /* async */);

  static_cast<mojom::Frame*>(&child_frame())
      ->Delete(mojom::FrameDeleteIntention::kNotMainFrame);
}

TEST_F(RenderFrameImplTest, NoCrashOnReceiveTitleWhenNavigatingToJavascript) {
  LoadHTML(
      "<html>"
      "  <everything id='outer'>"
      "    <title><empty></title>"
      "    <body>"
      "      <iframe id='iframe'></iframe>"
      "      <script>"
      "        iframe.contentWindow.onunload = () => {"
      "          document.adoptNode(outer);"
      "        };"
      "        window.location = 'javascript:\"PASS.\"';"
      "      </script>"
      "    </body>"
      "  </everything>"
      "</html> ");
}

TEST_F(RenderFrameImplTest, AutoplayFlags) {
  // Add autoplay flags to the page.
  GetMainRenderFrame()->AddAutoplayFlags(
      url::Origin::Create(GURL(kAutoplayTestOrigin)),
      blink::mojom::kAutoplayFlagHighMediaEngagement);

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, kAutoplayTestOrigin);

  // Check the flags have been set correctly.
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(*GetMainRenderFrame()));

  // Navigate the child frame.
  LoadChildFrame();

  // Check the flags are set on both frames.
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(*GetMainRenderFrame()));
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(child_frame()));

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, "https://www.example.com");
  LoadChildFrame();

  // Check the flags have been cleared.
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(*GetMainRenderFrame()));
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(child_frame()));
}

TEST_F(RenderFrameImplTest, AutoplayFlags_WrongOrigin) {
  // Add autoplay flags to the page.
  GetMainRenderFrame()->AddAutoplayFlags(
      url::Origin(), blink::mojom::kAutoplayFlagHighMediaEngagement);

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, kAutoplayTestOrigin);

  // Check the flags have been not been set.
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(*GetMainRenderFrame()));
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
    std::optional<blink::WebURL> updated =
        GetMainRenderFrame()->WillSendRequest(
            request.Url(), request.RequestorOrigin(), request.SiteForCookies(),
            blink::WebLocalFrameClient::ForRedirect(false), blink::WebURL());
    EXPECT_EQ(test_case.transformed, updated.has_value()
                                         ? updated->GetString().Utf8()
                                         : request.Url().GetString().Utf8());
  }
}

TEST_F(RenderFrameImplTest, MainFrameIntersectionRecorded) {
  RenderFrameTestObserver observer(&child_frame());
  gfx::Rect mainframe_intersection(0, 0, 200, 140);
  child_frame().OnMainFrameIntersectionChanged(mainframe_intersection);
  // Setting a new frame intersection in a local frame triggers the render frame
  // observer call.
  EXPECT_EQ(observer.last_intersection_rect(), mainframe_intersection);
}

TEST_F(RenderFrameImplTest, MainFrameViewportRectRecorded) {
  RenderFrameTestObserver observer(GetMainRenderFrame());
  gfx::Rect mainframe_viewport(0, 0, 200, 140);
  GetMainRenderFrame()->OnMainFrameViewportRectangleChanged(mainframe_viewport);
  EXPECT_EQ(observer.last_viewport_rect(), mainframe_viewport);

  // After a navigation, the notification of `mainframe_viewport` should be
  // propagated to `RenderFrameTestObserver` again for the new document.
  LoadHTML(kParentFrameHTML);
  RenderFrameTestObserver observer2(GetMainRenderFrame());
  GetMainRenderFrame()->OnMainFrameViewportRectangleChanged(mainframe_viewport);
  EXPECT_EQ(observer2.last_viewport_rect(), mainframe_viewport);
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
  bool operator!=(const SourceAnnotation& rhs) const { return !(*this == rhs); }
};

std::ostream& operator<<(std::ostream& out, const SourceAnnotation& s) {
  out << s.document_url.possibly_invalid_spec() << " : "
      << s.render_frame_event;
  return out;
}

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

  TestSimpleBrowserInterfaceBrokerImpl(
      const TestSimpleBrowserInterfaceBrokerImpl&) = delete;
  TestSimpleBrowserInterfaceBrokerImpl& operator=(
      const TestSimpleBrowserInterfaceBrokerImpl&) = delete;

  void BindAndFlush(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
    ASSERT_FALSE(receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
    receiver_.FlushForTesting();
  }

 private:
  // blink::mojom::BrowserInterfaceBroker:
  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    if (receiver.interface_name().value() == interface_name_) {
      binder_callback_.Run(receiver.PassPipe());
    }
  }

  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> receiver_;

  std::string interface_name_;
  BinderCallback binder_callback_;
};

class FrameHostTestInterfaceImpl : public mojom::FrameHostTestInterface {
 public:
  FrameHostTestInterfaceImpl() = default;

  FrameHostTestInterfaceImpl(const FrameHostTestInterfaceImpl&) = delete;
  FrameHostTestInterfaceImpl& operator=(const FrameHostTestInterfaceImpl&) =
      delete;

  ~FrameHostTestInterfaceImpl() override {}

  void BindAndFlush(
      mojo::PendingReceiver<mojom::FrameHostTestInterface> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.WaitForIncomingCall();
  }

  const std::optional<SourceAnnotation>& ping_source() const {
    return ping_source_;
  }

 protected:
  void Ping(const GURL& url, const std::string& event) override {
    ping_source_ = SourceAnnotation{url, event};
  }

 private:
  mojo::Receiver<mojom::FrameHostTestInterface> receiver_{this};
  std::optional<SourceAnnotation> ping_source_;
};

// RenderFrameObserver that issues FrameHostTestInterface interface requests
// through the RenderFrame's |remote_interfaces_| in response to observing
// important milestones in a frame's lifecycle.
class FrameHostTestInterfaceRequestIssuer : public RenderFrameObserver {
 public:
  explicit FrameHostTestInterfaceRequestIssuer(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame) {}

  FrameHostTestInterfaceRequestIssuer(
      const FrameHostTestInterfaceRequestIssuer&) = delete;
  FrameHostTestInterfaceRequestIssuer& operator=(
      const FrameHostTestInterfaceRequestIssuer&) = delete;

  void RequestTestInterfaceOnFrameEvent(const std::string& event) {
    mojo::Remote<mojom::FrameHostTestInterface> remote;
    blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
    render_frame()->GetBrowserInterfaceBroker().GetInterface(
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

  void DidCommitProvisionalLoad(ui::PageTransition transition) override {
    RequestTestInterfaceOnFrameEvent(kFrameEventDidCommitProvisionalLoad);
  }

  void DidFinishSameDocumentNavigation() override {
    RequestTestInterfaceOnFrameEvent(kFrameEventDidCommitSameDocumentLoad);
  }
};

// RenderFrameObserver that can be used to wait for the next commit in a frame.
class FrameCommitWaiter : public RenderFrameObserver {
 public:
  explicit FrameCommitWaiter(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame) {}

  FrameCommitWaiter(const FrameCommitWaiter&) = delete;
  FrameCommitWaiter& operator=(const FrameCommitWaiter&) = delete;

  void Wait() {
    if (did_commit_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  // RenderFrameObserver:
  void OnDestruct() override {}

  void DidCommitProvisionalLoad(ui::PageTransition transition) override {
    did_commit_ = true;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool did_commit_ = false;
};

// Testing ContentRendererClient implementation that fires the |callback|
// whenever a new frame is created.
class FrameCreationObservingRendererClient : public ContentRendererClient {
 public:
  using FrameCreatedCallback = base::RepeatingCallback<void(TestRenderFrame*)>;

  FrameCreationObservingRendererClient() {}

  FrameCreationObservingRendererClient(
      const FrameCreationObservingRendererClient&) = delete;
  FrameCreationObservingRendererClient& operator=(
      const FrameCreationObservingRendererClient&) = delete;

  ~FrameCreationObservingRendererClient() override {}

  void set_callback(FrameCreatedCallback callback) {
    callback_ = std::move(callback);
  }

  void reset_callback() { callback_.Reset(); }

 protected:
  void RenderFrameCreated(RenderFrame* render_frame) override {
    ContentRendererClient::RenderFrameCreated(render_frame);
    if (callback_) {
      callback_.Run(static_cast<TestRenderFrame*>(render_frame));
    }
  }

 private:
  FrameCreatedCallback callback_;
};

// Expects observing the creation of a new frame, and creates an instance of
// FrameHostTestInterfaceRequestIssuerRenderFrame for that new frame to exercise
// its RemoteInterfaceProvider interface.
class ScopedNewFrameInterfaceProviderExerciser {
 public:
  explicit ScopedNewFrameInterfaceProviderExerciser(
      FrameCreationObservingRendererClient* frame_creation_observer,
      const std::optional<std::string>& html_override_for_first_load =
          std::nullopt)
      : frame_creation_observer_(frame_creation_observer),
        html_override_for_first_load_(html_override_for_first_load) {
    frame_creation_observer_->set_callback(base::BindRepeating(
        &ScopedNewFrameInterfaceProviderExerciser::OnFrameCreated,
        base::Unretained(this)));
  }

  ScopedNewFrameInterfaceProviderExerciser(
      const ScopedNewFrameInterfaceProviderExerciser&) = delete;
  ScopedNewFrameInterfaceProviderExerciser& operator=(
      const ScopedNewFrameInterfaceProviderExerciser&) = delete;

  ~ScopedNewFrameInterfaceProviderExerciser() {
    frame_creation_observer_->reset_callback();
  }

  void ExpectNewFrameAndWaitForLoad(const GURL& expected_loaded_url) {
    ASSERT_NE(nullptr, frame_);
    frame_commit_waiter_->Wait();

    ASSERT_FALSE(frame_->GetWebFrame()->GetCurrentHistoryItem().IsNull());
    ASSERT_FALSE(frame_->GetWebFrame()->GetDocument().IsNull());
    EXPECT_EQ(expected_loaded_url,
              GURL(frame_->GetWebFrame()->GetDocument().Url()));

    browser_interface_broker_receiver_for_first_document_ =
        frame_->TakeLastBrowserInterfaceBrokerReceiver();
  }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  browser_interface_broker_receiver_for_initial_empty_document() {
    return std::move(
        browser_interface_broker_receiver_for_initial_empty_document_);
  }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  browser_interface_broker_receiver_for_first_document() {
    return std::move(browser_interface_broker_receiver_for_first_document_);
  }

 private:
  void OnFrameCreated(TestRenderFrame* frame) {
    ASSERT_EQ(nullptr, frame_.get());
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

    browser_interface_broker_receiver_for_initial_empty_document_ =
        frame_->TakeLastBrowserInterfaceBrokerReceiver();
    EXPECT_TRUE(frame->GetWebFrame()->GetCurrentHistoryItem().IsNull());
  }

  raw_ptr<FrameCreationObservingRendererClient> frame_creation_observer_;
  raw_ptr<TestRenderFrame> frame_ = nullptr;
  std::optional<std::string> html_override_for_first_load_;
  GURL first_committed_url_;

  std::optional<FrameCommitWaiter> frame_commit_waiter_;
  std::optional<FrameHostTestInterfaceRequestIssuer> test_request_issuer_;

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver_for_initial_empty_document_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver_for_first_document_;
};

// Extracts all interface receivers for FrameHostTestInterface pending on the
// specified |browser_interface_broker_receiver|, and returns a list of the
// source annotations that are provided in the pending Ping() call for each of
// these FrameHostTestInterface receivers.
void ExpectPendingInterfaceReceiversFromSources(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    std::vector<SourceAnnotation> expected_sources) {
  std::vector<SourceAnnotation> sources;
  ASSERT_TRUE(browser_interface_broker_receiver.is_valid());

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
  RenderFrameRemoteInterfacesTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAllowContentInitiatedDataUrlNavigations);
    blink::WebRuntimeFeatures::EnableFeatureFromString(
        "AllowContentInitiatedDataUrlNavigations", true);
  }

  RenderFrameRemoteInterfacesTest(const RenderFrameRemoteInterfacesTest&) =
      delete;
  RenderFrameRemoteInterfacesTest& operator=(
      const RenderFrameRemoteInterfacesTest&) = delete;

  ~RenderFrameRemoteInterfacesTest() override {
    blink::WebRuntimeFeatures::EnableFeatureFromString(
        "AllowContentInitiatedDataUrlNavigations", false);
  }

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
    return static_cast<TestRenderFrame*>(RenderViewTest::GetMainRenderFrame());
  }

  ContentRendererClient* CreateContentRendererClient() override {
    frame_creation_observer_ = new FrameCreationObservingRendererClient();
    return frame_creation_observer_;
  }

 private:
  // Owned by RenderViewTest.
  raw_ptr<FrameCreationObservingRendererClient> frame_creation_observer_ =
      nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Expect that |remote_interfaces_| is bound before the first committed load in
// a child frame, and then re-bound on the first commit.
TEST_F(RenderFrameRemoteInterfacesTest, ChildFrameAtFirstCommittedLoad) {
  ScopedNewFrameInterfaceProviderExerciser child_frame_exerciser(
      frame_creation_observer());
  const std::string html = base::StringPrintf("<iframe src=\"%s\"></iframe>",
                                              "data:text/html,Child");
  LoadHTMLWithUrlOverride(html.c_str(), kTestSecondURL);

  const GURL child_frame_url("data:text/html,Child");
  ASSERT_NO_FATAL_FAILURE(
      child_frame_exerciser.ExpectNewFrameAndWaitForLoad(child_frame_url));

  // TODO(crbug.com/40553427): It is unfortunate how many internal
  // details of frame/document creation this encodes. Need to decouple.
  const GURL initial_empty_url(kAboutBlankURL);
  ExpectPendingInterfaceReceiversFromSources(
      child_frame_exerciser
          .browser_interface_broker_receiver_for_initial_empty_document(),
      {{initial_empty_url, kFrameEventDidCreateNewFrame},
       {child_frame_url, kFrameEventReadyToCommitNavigation},
       // TODO(crbug.com/40444754): It seems strange that the new
       // document is created and DidCreateNewDocument is invoked *before* the
       // provisional load would have even committed.
       {child_frame_url, kFrameEventDidCreateNewDocument}});
  ExpectPendingInterfaceReceiversFromSources(
      child_frame_exerciser
          .browser_interface_broker_receiver_for_first_document(),
      {{child_frame_url, kFrameEventDidCommitProvisionalLoad},
       {child_frame_url, kFrameEventDidCreateDocumentElement}});
}

// Expect that |remote_interfaces_| is bound before the first committed load in
// the main frame of an opened window, and then re-bound on the first commit.
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
  // TODO(crbug.com/40553427): It is unfortunate how many internal
  // details of frame/document creation this encodes. Need to decouple.
  const GURL initial_empty_url;
  ExpectPendingInterfaceReceiversFromSources(
      main_frame_exerciser
          .browser_interface_broker_receiver_for_initial_empty_document(),
      {{initial_empty_url, kFrameEventDidCreateNewFrame},
       {new_window_url, kFrameEventReadyToCommitNavigation},
       {new_window_url, kFrameEventDidCreateNewDocument}});
  ExpectPendingInterfaceReceiversFromSources(
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
// TODO(crbug.com/40519010): when all clients are converted to use
// BrowserInterfaceBroker, PendingReceiver<InterfaceProvider>-related code will
// be removed.
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

    ExpectPendingInterfaceReceiversFromSources(
        child_frame_exerciser
            .browser_interface_broker_receiver_for_initial_empty_document(),
        {{initial_empty_url, kFrameEventDidCreateNewFrame},
         {child_frame_url, kFrameEventReadyToCommitNavigation},
         {child_frame_url, kFrameEventDidCreateNewDocument},
         {child_frame_url, kFrameEventDidCommitProvisionalLoad},
         {child_frame_url, kFrameEventDidCreateDocumentElement}});

    auto browser_interface_broker_receiver =
        child_frame_exerciser
            .browser_interface_broker_receiver_for_first_document();
    ASSERT_FALSE(browser_interface_broker_receiver.is_valid());
  }
}

// Expect that |remote_interfaces_| is bound to a new pipe on cross-document
// navigations.
TEST_F(RenderFrameRemoteInterfacesTest, ReplacedOnNonSameDocumentNavigation) {
  LoadHTMLWithUrlOverride("", kTestFirstURL);

  auto browser_interface_broker_receiver_for_first_document =
      GetMainRenderFrame()->TakeLastBrowserInterfaceBrokerReceiver();

  FrameHostTestInterfaceRequestIssuer requester(GetMainRenderFrame());
  requester.RequestTestInterfaceOnFrameEvent(kFrameEventAfterCommit);

  LoadHTMLWithUrlOverride("", kTestSecondURL);

  auto browser_interface_broker_receiver_for_second_document =
      GetMainRenderFrame()->TakeLastBrowserInterfaceBrokerReceiver();

  ASSERT_TRUE(browser_interface_broker_receiver_for_first_document.is_valid());

  ExpectPendingInterfaceReceiversFromSources(
      std::move(browser_interface_broker_receiver_for_first_document),
      {{GURL(kTestFirstURL), kFrameEventAfterCommit},
       {GURL(kTestSecondURL), kFrameEventReadyToCommitNavigation},
       {GURL(kTestSecondURL), kFrameEventDidCreateNewDocument}});

  ASSERT_TRUE(browser_interface_broker_receiver_for_second_document.is_valid());

  ExpectPendingInterfaceReceiversFromSources(
      std::move(browser_interface_broker_receiver_for_second_document),
      {{GURL(kTestSecondURL), kFrameEventDidCommitProvisionalLoad},
       {GURL(kTestSecondURL), kFrameEventDidCreateDocumentElement}});
}

// Expect that |remote_interfaces_| is not bound to a new pipe on same-document
// navigations, i.e. the existing InterfaceProvider connection is continued to
// be used.
TEST_F(RenderFrameRemoteInterfacesTest, ReusedOnSameDocumentNavigation) {
  LoadHTMLWithUrlOverride("", kTestFirstURL);

  auto browser_interface_broker_receiver =
      GetMainRenderFrame()->TakeLastBrowserInterfaceBrokerReceiver();

  FrameHostTestInterfaceRequestIssuer requester(GetMainRenderFrame());
  OnSameDocumentNavigation(GetMainFrame(), true /* is_new_navigation */);

  ASSERT_TRUE(browser_interface_broker_receiver.is_valid());

  ExpectPendingInterfaceReceiversFromSources(
      std::move(browser_interface_broker_receiver),
      {{GURL(kTestFirstURL), kFrameEventDidCommitSameDocumentLoad}});
}

TEST_F(RenderFrameImplTest, LastCommittedUrlForUKM) {
  // Test the case where we have a data url with a base_url.
  GURL data_url = GURL("data:text/html,");
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = data_url;
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->base_url_for_data_url = GURL("about:blank");
  auto commit_params = blink::CreateCommitNavigationParams();
  auto waiter = std::make_unique<FrameLoadWaiter>(GetMainRenderFrame());

  GetMainRenderFrame()->Navigate(std::move(common_params),
                                 std::move(commit_params));
  waiter->Wait();
  EXPECT_EQ(GURL(GetMainRenderFrame()->LastCommittedUrlForUKM()), data_url);

  // Test the case where we have an unreachable URL.
  GURL unreachable_url = GURL("http://www.example.com");
  waiter = std::make_unique<FrameLoadWaiter>(GetMainRenderFrame());
  GetMainRenderFrame()->LoadHTMLStringForTesting(
      "test", data_url, "UTF-8", unreachable_url,
      false /* replace_current_item */);
  waiter->Wait();
  EXPECT_EQ(GURL(GetMainRenderFrame()->LastCommittedUrlForUKM()),
            unreachable_url);

  // Test the base case, normal load.
  GURL override_url = GURL("http://example.com");
  waiter = std::make_unique<FrameLoadWaiter>(GetMainRenderFrame());
  LoadHTMLWithUrlOverride("Test", "http://example.com");
  waiter->Wait();
  EXPECT_EQ(GURL(GetMainRenderFrame()->LastCommittedUrlForUKM()), override_url);
}

// Verify that a frame with a pending update is cancelled when a forced update
// is sent.
TEST_F(RenderFrameImplTest, SendUpdateCancelsPending) {
  RenderFrameImpl* main_frame = GetMainRenderFrame();
  main_frame->StartDelayedSyncTimer();
  EXPECT_TRUE(main_frame->delayed_state_sync_timer_.IsRunning());
  main_frame->SendUpdateState();
  EXPECT_FALSE(main_frame->delayed_state_sync_timer_.IsRunning());
}

namespace {

// All content setting tests use the same data url, which contains html which
// has different behavior depending on whether script is enabled or disabled.
blink::mojom::CommonNavigationParamsPtr
GetCommonParamsForContentSettingsTest() {
  const char kHtml[] =
      "<html>"
      "<noscript>JS_DISABLED</noscript>"
      "<script>document.write('JS_ENABLED');</script>"
      "</html>";
  std::string data_url_contents = "data:text/html,";
  data_url_contents += kHtml;

  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL(data_url_contents);
  return common_params;
}

// Dump the layout tree and see whether it contains "text".
bool HasText(blink::WebLocalFrame* frame, const std::string& text) {
  std::string layout_tree =
      blink::TestWebFrameContentDumper::DumpLayoutTreeAsText(
          frame, blink::TestWebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8();

  return base::Contains(layout_tree, text);
}

// Waits for the navigation to finish.
void NavigateAndWait(content::TestRenderFrame* frame,
                     blink::mojom::CommonNavigationParamsPtr common_params,
                     blink::mojom::CommitNavigationParamsPtr commit_params,
                     blink::WebView* web_view) {
  FrameLoadWaiter waiter(frame);
  frame->Navigate(std::move(common_params), std::move(commit_params));
  waiter.Wait();
}

class FakeContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  explicit FakeContentSettingsClient(content::RenderFrame* render_frame)
      : render_frame_(render_frame) {
    render_frame_->GetWebFrame()->SetContentSettingsClient(this);
  }

  ~FakeContentSettingsClient() override {
    render_frame_->GetWebFrame()->SetContentSettingsClient(nullptr);
  }

  // blink::WebContentSettingsClient implementation.
  void DidNotAllowImage() override { ++did_not_allow_image_count_; }
  void DidNotAllowScript() override { ++did_not_allow_script_count_; }

  int did_not_allow_image_count_ = 0;
  int did_not_allow_script_count_ = 0;
  raw_ptr<content::RenderFrame> render_frame_;
};

}  // namespace

// Checks that when images are blocked, the ContentSettingsAgent receives a
// callback.
TEST_F(RenderFrameImplTest, ContentSettingsCallbackImageBlocked) {
  // Create a fake content settings client to track image blocked callbacks.
  FakeContentSettingsClient fake_content_settings_client(GetMainRenderFrame());

  // Navigate to a URL that consists of a red square.
  std::string data_url_contents =
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAABkAAAAZAQMAAAD+JxcgAAAAA1BMVEX/"
      "AAAZ4gk3AAAAC0lEQVR4AWMYSAAAAH0AAVFwgb4AAAAASUVORK5CYII=";

  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL(data_url_contents);
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::CreateCommitNavigationParams();
  commit_params->content_settings->allow_image = false;
  content::TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(GetMainRenderFrame());

  NavigateAndWait(frame, common_params->Clone(), commit_params->Clone(),
                  web_view_);

  EXPECT_EQ(1, fake_content_settings_client.did_not_allow_image_count_);
}

// Checks that when script is blocked, the ContentSettingsAgent receives a
// callback.
TEST_F(RenderFrameImplTest, ContentSettingsCallbackScriptBlocked) {
  // Create a fake content settings client to track script blocked callbacks.
  FakeContentSettingsClient fake_content_settings_client(GetMainRenderFrame());

  // Navigate to a URL with script disabled.
  auto common_params = GetCommonParamsForContentSettingsTest();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::CreateCommitNavigationParams();
  commit_params->content_settings->allow_script = false;
  content::TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(GetMainRenderFrame());

  NavigateAndWait(frame, common_params->Clone(), commit_params->Clone(),
                  web_view_);
  EXPECT_TRUE(HasText(GetMainFrame(), "JS_DISABLED"));
  EXPECT_FALSE(HasText(GetMainFrame(), "JS_ENABLED"));

  EXPECT_EQ(1, fake_content_settings_client.did_not_allow_script_count_);
}

// Checks that when script is allowed, the ContentSettingsAgent does not receive
// a callback.
TEST_F(RenderFrameImplTest, ContentSettingsCallbackScriptAllowed) {
  // Create a fake content settings client to track script blocked callbacks.
  FakeContentSettingsClient fake_content_settings_client(GetMainRenderFrame());

  // Navigate to a URL with script enabled.
  auto common_params = GetCommonParamsForContentSettingsTest();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::CreateCommitNavigationParams();
  content::TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(GetMainRenderFrame());

  NavigateAndWait(frame, common_params->Clone(), commit_params->Clone(),
                  web_view_);
  // Verify that the script was not blocked.
  EXPECT_FALSE(HasText(GetMainFrame(), "JS_DISABLED"));
  EXPECT_TRUE(HasText(GetMainFrame(), "JS_ENABLED"));

  // Verify there was no script blocked callback.
  EXPECT_EQ(0, fake_content_settings_client.did_not_allow_script_count_);
}

// Regression test for crbug.com/232410: Load a page with JS blocked. Then,
// allow JS and reload the page. In each case, only one of noscript or script
// tags should be enabled, but never both.
TEST_F(RenderFrameImplTest, ContentSettingsNoscriptTag) {
  // Navigate to a URL with script disabled.
  auto common_params = GetCommonParamsForContentSettingsTest();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::CreateCommitNavigationParams();
  commit_params->content_settings->allow_script = false;
  content::TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(GetMainRenderFrame());

  NavigateAndWait(frame, common_params->Clone(), commit_params->Clone(),
                  web_view_);
  EXPECT_TRUE(HasText(GetMainFrame(), "JS_DISABLED"));
  EXPECT_FALSE(HasText(GetMainFrame(), "JS_ENABLED"));

  // Reload the page but allow Javascript.
  common_params->navigation_type = blink::mojom::NavigationType::RELOAD;
  commit_params->content_settings->allow_script = true;
  NavigateAndWait(frame, common_params->Clone(), commit_params->Clone(),
                  web_view_);
  EXPECT_FALSE(HasText(GetMainFrame(), "JS_DISABLED"));
  EXPECT_TRUE(HasText(GetMainFrame(), "JS_ENABLED"));
}

// Checks that same document navigations don't update content settings for the
// page.
TEST_F(RenderFrameImplTest, ContentSettingsSameDocumentNavigation) {
  // Load a page which contains a script.
  auto common_params = GetCommonParamsForContentSettingsTest();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::CreateCommitNavigationParams();
  content::TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(GetMainRenderFrame());

  NavigateAndWait(frame, common_params->Clone(), commit_params->Clone(),
                  web_view_);

  // Verify that the script was not blocked.
  EXPECT_FALSE(HasText(GetMainFrame(), "JS_DISABLED"));
  EXPECT_TRUE(HasText(GetMainFrame(), "JS_ENABLED"));

  RenderFrameImpl* main_frame = GetMainRenderFrame();

  main_frame->DidFinishSameDocumentNavigation(
      blink::kWebStandardCommit,
      /*is_synchronously_committed=*/true,
      blink::mojom::SameDocumentNavigationType::kFragment,
      /*is_client_redirect=*/false,
      /*screenshot_destination=*/std::nullopt);

  // Verify that the script was not blocked.
  EXPECT_FALSE(HasText(GetMainFrame(), "JS_DISABLED"));
  EXPECT_TRUE(HasText(GetMainFrame(), "JS_ENABLED"));
}

class RenderFrameImplMojoJsTest : public RenderViewTest {
 public:
  RenderFrameImplMojoJsTest() {
    // An empty scoped feature list is used here to make sure that the feature
    // list can be (re)initialized in the death test successfully, otherwise a
    // CHECK will hit when tracing tries to reinitiailize in the same process.
    // This is an unfortunate quirk of how death tests work for browser tests.
    scoped_feature_list_.InitWithFeatures({}, {});
  }

  void SetUp() override {
    RenderViewTest::SetUp();
    EXPECT_TRUE(GetMainRenderFrame()->IsMainFrame());
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
    return static_cast<TestRenderFrame*>(RenderViewTest::GetMainRenderFrame());
  }

  // Gets the main world script context for the test main frame and returns if
  // MojoJS bindings are enabled.
  bool IsMojoJsEnabledForScriptContext() {
    v8::Isolate* isolate = Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> local_v8_context =
        GetMainFrame()->MainWorldScriptContext();

    return blink::WebV8Features::IsMojoJSEnabledForTesting(local_v8_context);
  }

  // Method used to validate the final stage protected memory check in
  // ContextFeatureSettings::isMojoJSEnabled constructs a scenario where
  // the |enable_mojo_js_| value of the ContextFeatureSettings is tampered with
  // directly before being used. We expect this to crash.
  void ContextFeatureSettingsEnableMojoJsTampered() {
    v8::Isolate* isolate = Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> local_v8_context =
        GetMainFrame()->MainWorldScriptContext();

    // Use |WebV8Features::EnableMojoJSForTesting| to enable the MojoJS bindings
    // while bypassing the earlier protected memory checks. This mimics an
    // attacker tampering with the |enable_mojo_js_| value of the
    // ContextFeatureSettings.
    blink::WebV8Features::EnableMojoJSWithoutSecurityChecksForTesting(
        local_v8_context);

    // Use |WebV8Features::isMojoJSEnabledForTesting| to manually access
    // |ContextFeatureSettings::isMojoEnabled()| This is used to mimic a
    // scenario where an attacker manages to directly tamper with
    // |enable_mojo_js_| before the |ContextFeatureSettings::isMojoJSEnabled()|
    // is called to determine if the bindings is enabled. This validates the
    // final protected memory check and should crash.
    blink::WebV8Features::IsMojoJSEnabledForTesting(local_v8_context);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies enabling MojoJS bindings.
TEST_F(RenderFrameImplMojoJsTest, AllowMojoWebUIBindings) {
  GetMainRenderFrame()->AllowBindings(
      BindingsPolicySet({BindingsPolicyValue::kMojoWebUi}).ToEnumBitmask());
  LoadHTML(kSimpleScriptHtml);

  // Expect no crash and MojoJs bindings are enabled in the context.
  EXPECT_TRUE(IsMojoJsEnabledForScriptContext());
}

// Verifies enabling MojoJS bindings via EnableMojoJsBindings method.
TEST_F(RenderFrameImplMojoJsTest, EnableMojoJSBindings) {
  GetMainRenderFrame()->EnableMojoJsBindings(
      content::mojom::ExtraMojoJsFeatures::New());
  LoadHTML(kSimpleScriptHtml);

  // Expect no crash and MojoJs bindings are enabled in the context.
  EXPECT_TRUE(IsMojoJsEnabledForScriptContext());
}

// Verifies enabling MojoJS bindings via directly enabling mojo
TEST_F(RenderFrameImplMojoJsTest, EnableMojoJSBindingsWithBroker) {
  GetMainRenderFrame()->EnableMojoJsBindingsWithBroker(
      TestRenderFrame::CreateStubBrowserInterfaceBrokerRemote());
  LoadHTML(kSimpleScriptHtml);

  // Expect no crash and MojoJs bindings are enabled in the context.
  EXPECT_TRUE(IsMojoJsEnabledForScriptContext());
}

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
using RenderFrameImplMojoJsDeathTest = RenderFrameImplMojoJsTest;
// Verifies that tampering with enabled_bindings_ to enable MojoJS bindings
// crashes.
TEST_F(RenderFrameImplMojoJsDeathTest, EnabledBindingsTampered) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  // Should CHECK fail due to the bindings value differing from the protected
  // memory value.
  EXPECT_CHECK_DEATH_WITH(
      {
        GetMainRenderFrame()->enabled_bindings_.Put(
            BindingsPolicyValue::kMojoWebUi);

        LoadHTML(kSimpleScriptHtml);
      },
      "Check failed: \\*mojo_js_allowed_");
}

// Verifies that tampering with enable_mojo_js_bindings_ to enable MojoJS
// bindings crashes.
TEST_F(RenderFrameImplMojoJsDeathTest, EnableMojoJsBindingsTampered) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  // Should CHECK fail due to the bindings value differing from the protected
  // memory value.
  EXPECT_CHECK_DEATH_WITH(
      {
        GetMainRenderFrame()->enable_mojo_js_bindings_ = true;

        LoadHTML(kSimpleScriptHtml);
      },
      "Check failed: \\*mojo_js_allowed_");
}

// Verifies that tampering with mojo_js_interface_broker_ to enable MojoJS
// bindings crashes.
TEST_F(RenderFrameImplMojoJsDeathTest, MojoJsInterfaceBrokerTampered) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  // Should CHECK fail due to the bindings value differing from the protected
  // memory value.
  EXPECT_CHECK_DEATH_WITH(
      {
        GetMainRenderFrame()->mojo_js_interface_broker_ =
            TestRenderFrame::CreateStubBrowserInterfaceBrokerRemote();

        LoadHTML(kSimpleScriptHtml);
      },
      "Check failed: \\*mojo_js_allowed_");
}

// Verifies that tampering with mojo_js_interface_broker_ to enable MojoJS
// bindings crashes.
TEST_F(RenderFrameImplMojoJsDeathTest,
       ContextFeatureSettingsEnableMojoJsTampered) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  // Should CHECK fail due to the bindings value differing from the protected
  // memory value.
  EXPECT_CHECK_DEATH_WITH(ContextFeatureSettingsEnableMojoJsTampered(),
                          "Check failed: \\*mojo_js_allowed_");
}
#endif  //  BUILDFLAG(PROTECTED_MEMORY_ENABLED)

}  // namespace content
