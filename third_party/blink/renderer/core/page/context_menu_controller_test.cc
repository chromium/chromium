// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/context_menu_controller.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-blink.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/fake_web_plugin.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory_impl.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/gfx/geometry/rect.h"

using testing::Return;

namespace blink {

namespace {

constexpr char kTestResourceFilename[] = "white-1x1.png";
constexpr char kTestResourceMimeType[] = "image/png";

class MockWebMediaPlayerForContextMenu : public EmptyWebMediaPlayer {
 public:
  MOCK_CONST_METHOD0(Duration, double());
  MOCK_CONST_METHOD0(HasAudio, bool());
  MOCK_CONST_METHOD0(HasVideo, bool());
};

class ContextMenuControllerTestPlugin : public FakeWebPlugin {
 public:
  struct PluginAttributes {
    // Whether the plugin has copy permission.
    bool can_copy;

    // The selected text in the plugin when the context menu is created.
    WebString selected_text;
  };

  explicit ContextMenuControllerTestPlugin(const WebPluginParams& params)
      : FakeWebPlugin(params) {}

  // FakeWebPlugin:
  WebString SelectionAsText() const override { return selected_text_; }
  bool CanCopy() const override { return can_copy_; }

  void SetAttributesForTesting(const PluginAttributes& attributes) {
    can_copy_ = attributes.can_copy;
    selected_text_ = attributes.selected_text;
  }

 private:
  bool can_copy_ = true;
  WebString selected_text_;
};

class TestWebFrameClientImpl : public frame_test_helpers::TestWebFrameClient {
 public:
  WebPlugin* CreatePlugin(const WebPluginParams& params) override {
    return new ContextMenuControllerTestPlugin(params);
  }

  void UpdateContextMenuDataForTesting(
      const ContextMenuData& data,
      const std::optional<gfx::Point>& host_context_menu_location) override {
    context_menu_data_ = data;
    host_context_menu_location_ = host_context_menu_location;
  }

  std::unique_ptr<WebMediaPlayer> CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings* settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override {
    return std::make_unique<MockWebMediaPlayerForContextMenu>();
  }

  const ContextMenuData& GetContextMenuData() const {
    return context_menu_data_;
  }

  const std::optional<gfx::Point>& host_context_menu_location() const {
    return host_context_menu_location_;
  }

 private:
  ContextMenuData context_menu_data_;
  std::optional<gfx::Point> host_context_menu_location_;
};

void RegisterMockedImageURLLoad(const String& url) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL(url.Utf8().c_str()),
      test::CoreTestDataPath(kTestResourceFilename), kTestResourceMimeType);
}

}  // namespace

template <>
struct DowncastTraits<ContextMenuControllerTestPlugin> {
  static bool AllowFrom(const WebPlugin& object) { return true; }
};

class ContextMenuControllerTest : public testing::Test {
 public:
  ContextMenuControllerTest() = default;

  void SetUp() override {
    web_view_helper_.Initialize(&web_frame_client_);

    WebLocalFrameImpl* local_main_frame = web_view_helper_.LocalMainFrame();
    local_main_frame->ViewImpl()->MainFrameViewWidget()->Resize(
        gfx::Size(640, 480));
    local_main_frame->ViewImpl()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  bool ShowContextMenu(const PhysicalOffset& location,
                       WebMenuSourceType source) {
    bool success =
        web_view_helper_.GetWebView()
            ->GetPage()
            ->GetContextMenuController()
            .ShowContextMenu(GetDocument()->GetFrame(), location, source);
    base::RunLoop().RunUntilIdle();
    return success;
  }

  bool ShowContextMenuForElement(Element* element, WebMenuSourceType source) {
    const DOMRect* rect = element->GetBoundingClientRect();
    PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                            LayoutUnit((rect->top() + rect->bottom()) / 2));
    ContextMenuAllowedScope context_menu_allowed_scope;
    return ShowContextMenu(location, source);
  }

  Document* GetDocument() {
    return static_cast<Document*>(
        web_view_helper_.LocalMainFrame()->GetDocument());
  }

  WebView* GetWebView() { return web_view_helper_.GetWebView(); }
  Page* GetPage() { return web_view_helper_.GetWebView()->GetPage(); }
  WebLocalFrameImpl* LocalMainFrame() {
    return web_view_helper_.LocalMainFrame();
  }
  void LoadAhem() { web_view_helper_.LoadAhem(); }

  const TestWebFrameClientImpl& GetWebFrameClient() const {
    return web_frame_client_;
  }

  void DurationChanged(HTMLVideoElement* video) { video->DurationChanged(); }

  void SetReadyState(HTMLVideoElement* video,
                     HTMLMediaElement::ReadyState state) {
    video->SetReadyState(state);
  }

 protected:
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestWebFrameClientImpl web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(ContextMenuControllerTest, CopyFromPlugin) {
  ContextMenuAllowedScope context_menu_allowed_scope;
  frame_test_helpers::LoadFrame(LocalMainFrame(), R"HTML(data:text/html,
  <html>
    <body>
      <embed id="embed" type="application/x-webkit-test-webplugin"
       src="chrome-extension://test" original-url="http://www.test.pdf">
      </embed>
    </body>
  <html>
  )HTML");

  Document* document = GetDocument();
  ASSERT_TRUE(IsA<HTMLDocument>(document));

  Element* embed_element = document->getElementById(AtomicString("embed"));
  ASSERT_TRUE(IsA<HTMLEmbedElement>(embed_element));

  auto* embedded =
      DynamicTo<LayoutEmbeddedContent>(embed_element->GetLayoutObject());
  WebPluginContainerImpl* embedded_plugin_view = embedded->Plugin();
  ASSERT_TRUE(!!embedded_plugin_view);

  auto* test_plugin = DynamicTo<ContextMenuControllerTestPlugin>(
      embedded_plugin_view->Plugin());

  // The plugin has copy permission but no text is selected.
  test_plugin->SetAttributesForTesting(
      {/*can_copy=*/true, /*selected_text=*/""});

  ASSERT_TRUE(ShowContextMenuForElement(embed_element, kMenuSourceMouse));
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kPlugin);
  EXPECT_FALSE(
      !!(context_menu_data.edit_flags & ContextMenuDataEditFlags::kCanCopy));
  EXPECT_EQ(context_menu_data.selected_text, "");

  // The plugin has copy permission and some text is selected.
  test_plugin->SetAttributesForTesting({/*can_copy=*/true,
                                        /*selected_text=*/"some text"});
  ASSERT_TRUE(ShowContextMenuForElement(embed_element, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kPlugin);
  EXPECT_TRUE(
      !!(context_menu_data.edit_flags & ContextMenuDataEditFlags::kCanCopy));
  EXPECT_EQ(context_menu_data.selected_text, "some text");

  // The plugin does not have copy permission and no text is selected.
  test_plugin->SetAttributesForTesting({/*can_copy=*/false,
                                        /*selected_text=*/""});
  ASSERT_TRUE(ShowContextMenuForElement(embed_element, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kPlugin);
  EXPECT_FALSE(
      !!(context_menu_data.edit_flags & ContextMenuDataEditFlags::kCanCopy));
  EXPECT_EQ(context_menu_data.selected_text, "");

  // The plugin does not have copy permission but some text is selected.
  test_plugin->SetAttributesForTesting({/*can_copy=*/false,
                                        /*selected_text=*/"some text"});
  ASSERT_TRUE(ShowContextMenuForElement(embed_element, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kPlugin);
  EXPECT_EQ(context_menu_data.selected_text, "some text");
  EXPECT_FALSE(
      !!(context_menu_data.edit_flags & ContextMenuDataEditFlags::kCanCopy));
}

TEST_F(ContextMenuControllerTest, VideoNotLoaded) {
  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  AtomicString video_url("https://example.com/foo.webm");

  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  video->SetSrc(video_url);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveNothing);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(false));

  DOMRect* rect = video->GetBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec().c_str());

  const Vector<std::pair<ContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {ContextMenuData::kMediaInError, false},
          {ContextMenuData::kMediaPaused, true},
          {ContextMenuData::kMediaMuted, false},
          {ContextMenuData::kMediaLoop, false},
          {ContextMenuData::kMediaCanSave, true},
          {ContextMenuData::kMediaHasAudio, false},
          {ContextMenuData::kMediaCanToggleControls, false},
          {ContextMenuData::kMediaControls, false},
          {ContextMenuData::kMediaCanPrint, false},
          {ContextMenuData::kMediaCanRotate, false},
          {ContextMenuData::kMediaCanPictureInPicture, false},
          {ContextMenuData::kMediaPictureInPicture, false},
          {ContextMenuData::kMediaCanLoop, true},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
}

TEST_F(ContextMenuControllerTest, VideoWithAudioOnly) {
  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  AtomicString video_url("https://example.com/foo.webm");

  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  video->SetSrc(video_url);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveNothing);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasAudio())
      .WillRepeatedly(Return(true));

  DOMRect* rect = video->GetBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kAudio,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec().c_str());

  const Vector<std::pair<ContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {ContextMenuData::kMediaInError, false},
          {ContextMenuData::kMediaPaused, true},
          {ContextMenuData::kMediaMuted, false},
          {ContextMenuData::kMediaLoop, false},
          {ContextMenuData::kMediaCanSave, true},
          {ContextMenuData::kMediaHasAudio, true},
          {ContextMenuData::kMediaCanToggleControls, false},
          {ContextMenuData::kMediaControls, false},
          {ContextMenuData::kMediaCanPrint, false},
          {ContextMenuData::kMediaCanRotate, false},
          {ContextMenuData::kMediaCanPictureInPicture, false},
          {ContextMenuData::kMediaPictureInPicture, false},
          {ContextMenuData::kMediaCanLoop, true},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
}

TEST_F(ContextMenuControllerTest, PictureInPictureEnabledVideoLoaded) {
  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  AtomicString video_url("https://example.com/foo.webm");

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  video->SetSrc(video_url);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(true));

  DOMRect* rect = video->GetBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec().c_str());

  const Vector<std::pair<ContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {ContextMenuData::kMediaInError, false},
          {ContextMenuData::kMediaPaused, true},
          {ContextMenuData::kMediaMuted, false},
          {ContextMenuData::kMediaLoop, false},
          {ContextMenuData::kMediaCanSave, true},
          {ContextMenuData::kMediaHasAudio, false},
          {ContextMenuData::kMediaCanToggleControls, true},
          {ContextMenuData::kMediaControls, false},
          {ContextMenuData::kMediaCanPrint, false},
          {ContextMenuData::kMediaCanRotate, false},
          {ContextMenuData::kMediaCanPictureInPicture, true},
          {ContextMenuData::kMediaPictureInPicture, false},
          {ContextMenuData::kMediaCanLoop, true},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
}

TEST_F(ContextMenuControllerTest, PictureInPictureDisabledVideoLoaded) {
  // Make sure Picture-in-Picture is disabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(false);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  AtomicString video_url("https://example.com/foo.webm");

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  video->SetSrc(video_url);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(true));

  DOMRect* rect = video->GetBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec().c_str());

  const Vector<std::pair<ContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {ContextMenuData::kMediaInError, false},
          {ContextMenuData::kMediaPaused, true},
          {ContextMenuData::kMediaMuted, false},
          {ContextMenuData::kMediaLoop, false},
          {ContextMenuData::kMediaCanSave, true},
          {ContextMenuData::kMediaHasAudio, false},
          {ContextMenuData::kMediaCanToggleControls, true},
          {ContextMenuData::kMediaControls, false},
          {ContextMenuData::kMediaCanPrint, false},
          {ContextMenuData::kMediaCanRotate, false},
          {ContextMenuData::kMediaCanPictureInPicture, false},
          {ContextMenuData::kMediaPictureInPicture, false},
          {ContextMenuData::kMediaCanLoop, true},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
}

TEST_F(ContextMenuControllerTest, MediaStreamVideoLoaded) {
  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  MediaStreamComponentVector dummy_components;
  auto* media_stream_descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
      dummy_components, dummy_components);
  video->SetSrcObjectVariant(media_stream_descriptor);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(true));

  DOMRect* rect = video->GetBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);

  const Vector<std::pair<ContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {ContextMenuData::kMediaInError, false},
          {ContextMenuData::kMediaPaused, true},
          {ContextMenuData::kMediaMuted, false},
          {ContextMenuData::kMediaLoop, false},
          {ContextMenuData::kMediaCanSave, false},
          {ContextMenuData::kMediaHasAudio, false},
          {ContextMenuData::kMediaCanToggleControls, true},
          {ContextMenuData::kMediaControls, false},
          {ContextMenuData::kMediaCanPrint, false},
          {ContextMenuData::kMediaCanRotate, false},
          {ContextMenuData::kMediaCanPictureInPicture, true},
          {ContextMenuData::kMediaPictureInPicture, false},
          {ContextMenuData::kMediaCanLoop, false},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
}

TEST_F(ContextMenuControllerTest, InfiniteDurationVideoLoaded) {
  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  AtomicString video_url("https://example.com/foo.webm");

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  video->SetSrc(video_url);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              Duration())
      .WillRepeatedly(Return(std::numeric_limits<double>::infinity()));
  DurationChanged(video.Get());

  DOMRect* rect = video->GetBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec().c_str());

  const Vector<std::pair<ContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {ContextMenuData::kMediaInError, false},
          {ContextMenuData::kMediaPaused, true},
          {ContextMenuData::kMediaMuted, false},
          {ContextMenuData::kMediaLoop, false},
          {ContextMenuData::kMediaCanSave, false},
          {ContextMenuData::kMediaHasAudio, false},
          {ContextMenuData::kMediaCanToggleControls, true},
          {ContextMenuData::kMediaControls, false},
          {ContextMenuData::kMediaCanPrint, false},
          {ContextMenuData::kMediaCanRotate, false},
          {ContextMenuData::kMediaCanPictureInPicture, true},
          {ContextMenuData::kMediaPictureInPicture, false},
          {ContextMenuData::kMediaCanLoop, false},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
}

TEST_F(ContextMenuControllerTest, HitTestVideoChildElements) {
  // Test that hit tests on parts of a video element result in hits on the video
  // element itself as opposed to its child elements.

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  AtomicString video_url("https://example.com/foo.webm");

  // Setup video element.
  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  video->SetSrc(video_url);
  video->setAttribute(
      html_names::kStyleAttr,
      AtomicString(
          "position: absolute; left: 0; top: 0; width: 200px; height: 200px"));
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  auto check_location = [&](PhysicalOffset location) {
    EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

    ContextMenuData context_menu_data =
        GetWebFrameClient().GetContextMenuData();
    EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
              context_menu_data.media_type);
    EXPECT_EQ(video_url, context_menu_data.src_url.spec().c_str());
  };

  // Center of video.
  check_location(PhysicalOffset(100, 100));

  // Play button.
  check_location(PhysicalOffset(10, 195));

  // Timeline bar.
  check_location(PhysicalOffset(100, 195));
}

TEST_F(ContextMenuControllerTest, EditingActionsEnabledInSVGDocument) {
  frame_test_helpers::LoadFrame(LocalMainFrame(), R"SVG(data:image/svg+xml,
    <svg xmlns='http://www.w3.org/2000/svg'
         xmlns:h='http://www.w3.org/1999/xhtml'
         font-family='Ahem'>
      <text y='20' id='t'>Copyable text</text>
      <foreignObject y='30' width='100' height='200'>
        <h:div id='e' style='width: 100px; height: 50px'
               contenteditable='true'>
          XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        </h:div>
      </foreignObject>
    </svg>
  )SVG");
  LoadAhem();

  Document* document = GetDocument();
  ASSERT_TRUE(document->IsSVGDocument());

  Element* text_element = document->getElementById(AtomicString("t"));
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  FrameSelection& selection = document->GetFrame()->Selection();

  // <text> element
  selection.SelectSubString(*text_element, 4, 8);
  EXPECT_TRUE(ShowContextMenuForElement(text_element, kMenuSourceMouse));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kNone);
  EXPECT_EQ(context_menu_data.edit_flags, ContextMenuDataEditFlags::kCanCopy);
  EXPECT_EQ(context_menu_data.selected_text, "able tex");

  // <div contenteditable=true>
  Element* editable_element = document->getElementById(AtomicString("e"));
  selection.SelectSubString(*editable_element, 0, 42);
  EXPECT_TRUE(ShowContextMenuForElement(editable_element, kMenuSourceMouse));

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kNone);
  EXPECT_EQ(context_menu_data.edit_flags,
            ContextMenuDataEditFlags::kCanCut |
                ContextMenuDataEditFlags::kCanCopy |
                ContextMenuDataEditFlags::kCanPaste |
                ContextMenuDataEditFlags::kCanDelete |
                ContextMenuDataEditFlags::kCanEditRichly);
}

TEST_F(ContextMenuControllerTest, EditingActionsEnabledInXMLDocument) {
  frame_test_helpers::LoadFrame(LocalMainFrame(), R"XML(data:text/xml,
    <root>
      <style xmlns="http://www.w3.org/1999/xhtml">
        root { color: blue; }
      </style>
      <text id="t">Blue text</text>
    </root>
  )XML");

  Document* document = GetDocument();
  ASSERT_TRUE(IsA<XMLDocument>(document));
  ASSERT_FALSE(IsA<HTMLDocument>(document));

  Element* text_element = document->getElementById(AtomicString("t"));
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  FrameSelection& selection = document->GetFrame()->Selection();

  selection.SelectAll();
  EXPECT_TRUE(ShowContextMenuForElement(text_element, kMenuSourceMouse));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kNone);
  EXPECT_EQ(context_menu_data.edit_flags, ContextMenuDataEditFlags::kCanCopy);
  EXPECT_EQ(context_menu_data.selected_text, "Blue text");
}

TEST_F(ContextMenuControllerTest, ShowNonLocatedContextMenuEvent) {
  GetDocument()->documentElement()->setInnerHTML(
      "<input id='sample' type='text' size='5' value='Sample Input Text'>");

  Document* document = GetDocument();
  Element* input_element = document->getElementById(AtomicString("sample"));
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Select the 'Sample' of |input|.
  DOMRect* rect = input_element->GetBoundingClientRect();
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(gfx::PointF(rect->left(), rect->top()));
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(gesture_event, ui::LatencyInfo()));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample");

  // Adjust the selection from the start of |input| to the middle.
  gfx::Point middle_point((rect->left() + rect->right()) / 2,
                          (rect->top() + rect->bottom()) / 2);
  LocalMainFrame()->MoveRangeSelectionExtent(middle_point);
  LocalMainFrame()->LocalRootFrameWidget()->ShowContextMenu(
      ui::mojom::MenuSourceType::TOUCH_HANDLE, middle_point);

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_NE(context_menu_data.selected_text, "");

  // Scroll the value of |input| to end.
  input_element->setScrollLeft(input_element->scrollWidth());

  // Select all the value of |input| to ensure the start of selection is
  // invisible.
  LocalMainFrame()->MoveRangeSelectionExtent(
      gfx::Point(rect->right(), rect->bottom()));
  LocalMainFrame()->LocalRootFrameWidget()->ShowContextMenu(
      ui::mojom::MenuSourceType::TOUCH_HANDLE,
      gfx::Point(rect->right() / 2, rect->bottom() / 2));

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample Input Text");
}

#if !BUILDFLAG(IS_MAC)
// Mac has no way to open a context menu based on a keyboard event.
TEST_F(ContextMenuControllerTest,
       ValidateNonLocatedContextMenuOnLargeImageElement) {
  GetDocument()->documentElement()->setInnerHTML(
      "<img src=\"http://example.test/cat.jpg\" id=\"sample_image\" "
      "width=\"200\" height=\"10000\" tabindex=\"-1\" />");

  Document* document = GetDocument();
  Element* image_element =
      document->getElementById(AtomicString("sample_image"));
  // Set focus on the image element.
  image_element->Focus();
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Simulate Shift + F10 key event.
  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kShiftKey,
                             WebInputEvent::GetStaticTimeStampForTests());

  key_event.windows_key_code = ui::VKEY_F10;
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type,
            mojom::blink::ContextMenuDataMediaType::kImage);
}
#endif

TEST_F(ContextMenuControllerTest, ContextMenuImageHitTestSVGImageElement) {
  RegisterMockedImageURLLoad("http://test.png");
  Document* document = GetDocument();

  ContextMenuAllowedScope context_menu_allowed_scope;
  document->documentElement()->setInnerHTML(R"HTML(
    <svg>
      <image id="target" href="http://test.png" width="100" height="100"/>
    </svg>
  )HTML");

  // Flush the image-loading microtask.
  base::RunLoop().RunUntilIdle();

  url_test_helpers::ServeAsynchronousRequests();

  Element* image = document->getElementById(AtomicString("target"));
  EXPECT_TRUE(ShowContextMenuForElement(image, kMenuSourceLongPress));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("http://test.png/", context_menu_data.src_url.spec());
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kImage,
            context_menu_data.media_type);
  EXPECT_TRUE(context_menu_data.has_image_contents);
}

TEST_F(ContextMenuControllerTest, SelectionRectClipped) {
  GetDocument()->documentElement()->setInnerHTML(
      "<textarea id='text-area' cols=6 rows=2>Sample editable text</textarea>");

  Document* document = GetDocument();
  Element* editable_element =
      document->getElementById(AtomicString("text-area"));
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  FrameSelection& selection = document->GetFrame()->Selection();

  // Select the 'Sample' of |textarea|.
  DOMRect* rect = editable_element->GetBoundingClientRect();
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(gfx::PointF(rect->left(), rect->top()));
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(gesture_event, ui::LatencyInfo()));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample");

  // The selection rect is not clipped.
  gfx::Rect anchor, focus;
  selection.ComputeAbsoluteBounds(anchor, focus);
  anchor = document->GetFrame()->View()->FrameToViewport(anchor);
  focus = document->GetFrame()->View()->FrameToViewport(focus);
  int left = std::min(focus.x(), anchor.x());
  int top = std::min(focus.y(), anchor.y());
  int right = std::max(focus.right(), anchor.right());
  int bottom = std::max(focus.bottom(), anchor.bottom());
  gfx::Rect selection_rect =
      document->GetFrame()->LocalFrameRoot().View()->FrameToViewport(
          gfx::Rect(left, top, right - left, bottom - top));
  EXPECT_EQ(context_menu_data.selection_rect, selection_rect);

  // Select all the content of |textarea|.
  selection.SelectAll();
  EXPECT_TRUE(ShowContextMenuForElement(editable_element, kMenuSourceMouse));

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample editable text");

  // The selection rect is clipped by the editable box.
  gfx::Rect clip_bound = editable_element->VisibleBoundsInLocalRoot();
  selection.ComputeAbsoluteBounds(anchor, focus);
  anchor = document->GetFrame()->View()->ConvertToRootFrame(anchor);
  focus = document->GetFrame()->View()->ConvertToRootFrame(focus);
  left = std::max(clip_bound.x(), std::min(focus.x(), anchor.x()));
  top = std::max(clip_bound.y(), std::min(focus.y(), anchor.y()));
  right = std::min(clip_bound.right(), std::max(focus.right(), anchor.right()));
  bottom =
      std::min(clip_bound.bottom(), std::max(focus.bottom(), anchor.bottom()));
  selection_rect =
      document->GetFrame()->LocalFrameRoot().View()->FrameToViewport(
          gfx::Rect(left, top, right - left, bottom - top));
  EXPECT_EQ(context_menu_data.selection_rect, selection_rect);
}

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext*, Event*));
};

// Test that a basic image hit test works and is no† impacted by
// penetrating image selection logic.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageHitTestStandardImageSelection) {
  String url = "http://test.png";
  LOG(ERROR) << "URL IS: " << url.Utf8().c_str();
  RegisterMockedImageURLLoad(url);

  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src="http://test.png">
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("http://test.png/", context_menu_data.src_url.spec());
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kImage,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that image selection can penetrate through a fully transparent div
// above the target image.
TEST_F(ContextMenuControllerTest, ContextMenuImageHitTestSucceededPenetrating) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <img id=target src='http://test.png'>
      <div id=occluder></div>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("http://test.png/", context_menu_data.src_url.spec());
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kImage,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that a basic image hit test works and is no† impacted by
// penetrating image selection logic.
TEST_F(ContextMenuControllerTest, ContextMenuImageHitTestStandardCanvas) {
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <canvas id=target>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kCanvas,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that  an image node will not be selected through an opaque div
// above the target image.
TEST_F(ContextMenuControllerTest, ContextMenuImageHitTestOpaqueNodeBlocking) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #opaque {
          background: blue;
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 3;
        }
      </style>

      <img id=target src='http://test.png'>
      <div id=opaque></div>
      <div id=occluder></div>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kNone,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that an image node will not be selected if a node with a context menu
// listener is above the image node, but that we will still log the presence of
// the image.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageHitTestContextMenuListenerAboveImageBlocking) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #nodewithlistener {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 3;
        }
      </style>
      <img id=target src='http://test.png'>
      <div id=nodewithlistener></div>
      <div id=occluder></div>
    </body>
)HTML");

  Persistent<MockEventListener> event_listener =
      MakeGarbageCollected<MockEventListener>();
  base::HistogramTester histograms;

  Element* target_image = GetDocument()->getElementById(AtomicString("target"));
  target_image->addEventListener(event_type_names::kContextmenu,
                                 event_listener);

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kNone,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that an image node will not be selected if the image node itself has a
// context menu listener on it (and the image node is not the topmost element)
TEST_F(ContextMenuControllerTest,
       ContextMenuImageHitTestContextMenuListenerOnImageBlocking) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <img id=target src='http://test.png'>
      <div id=occluder></div>
    </body>
  )HTML");

  // Attaching a listener for the finished event indicates pending activity.
  Persistent<MockEventListener> event_listener =
      MakeGarbageCollected<MockEventListener>();
  base::HistogramTester histograms;

  Element* target_image = GetDocument()->getElementById(AtomicString("target"));
  target_image->addEventListener(event_type_names::kContextmenu,
                                 event_listener);

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kNone,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that an image node will be selected if the image node itself has an
// unrelated event listener on it.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageHitTestNonBlockingNonContextMenuListenerOnImage) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <img id=target src='http://test.png'>
      <div id=occluder></div>
    </body>
  )HTML");

  Persistent<MockEventListener> event_listener =
      MakeGarbageCollected<MockEventListener>();
  base::HistogramTester histograms;

  Element* target_image = GetDocument()->getElementById(AtomicString("target"));
  target_image->addEventListener(event_type_names::kClick, event_listener);

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kImage,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that an image node will still be selected if it is the topmost node
// despite an ancestor having a context menu listener attached to it.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageHitTestStandardContextMenuListenerAncestorNonBlocking) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #hiddenancestor {
          top: 0;
          left: 0;
          position: absolute;
          width: 1px;
          height: 1px;
          z-index: 1;
        }
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <div id=hiddenancestor>
        <img id=target src='http://test.png'>
      </div>
    </body>
  )HTML");

  Persistent<MockEventListener> event_listener =
      MakeGarbageCollected<MockEventListener>();
  base::HistogramTester histograms;

  Element* hidden_ancestor =
      GetDocument()->getElementById(AtomicString("hiddenancestor"));
  hidden_ancestor->addEventListener(event_type_names::kContextmenu,
                                    event_listener);

  // This hit test would miss the node with the listener if it was not an
  // ancestor.
  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  // EXPECT_TRUE(context_menu_data.has_image_contents);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that an image node will not be selected if a non image node with a
// context listener ancestor is above it and verify that topmost context menu
// listener special logic only applies if the topmost node is an image.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageHitTestContextMenuListenerAncestorBlocking) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #hiddenancestor {
          top: 0;
          left: 0;
          position: absolute;
          width: 1px;
          height: 1px;
          z-index: 2;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 3;
        }
      </style>
      <img id=target src='http://test.png'>
      <div id=hiddenancestor>
        <div id=occluder></div>
      </div>
    </body>
  )HTML");

  Persistent<MockEventListener> event_listener =
      MakeGarbageCollected<MockEventListener>();
  base::HistogramTester histograms;

  Element* hidden_ancestor =
      GetDocument()->getElementById(AtomicString("hiddenancestor"));
  hidden_ancestor->addEventListener(event_type_names::kContextmenu,
                                    event_listener);

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kNone,
            context_menu_data.media_type);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundPenetrating, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByOpaqueNode, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kFoundContextMenuListener,
      1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kBlockedByCrossFrameNode,
      0);
}

// Test that an image node is successfully cached and retrieved in the common
// case.
TEST_F(ContextMenuControllerTest, ContextMenuImageRetrievalCachedImageFound) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <img id=target src='http://test.png'>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  Node* image_node =
      web_view_helper_.GetWebView()
          ->GetPage()
          ->GetContextMenuController()
          .ContextMenuImageNodeForFrame(GetDocument()->GetFrame());
  EXPECT_TRUE(image_node != nullptr);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageFound, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageNotFound, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::
          kCrossFrameRetrieval,
      0);
}

// Test that an image node is not successfully retrieved if a hit test was never
// conducted.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageRetrievalCachedImageNotFound) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <img id=target src='http://test.png'>
    </body>
  )HTML");

  base::HistogramTester histograms;

  // Attempt to retrieve without an initial call to show the context menu.
  Node* image_node =
      web_view_helper_.GetWebView()
          ->GetPage()
          ->GetContextMenuController()
          .ContextMenuImageNodeForFrame(GetDocument()->GetFrame());
  EXPECT_TRUE(image_node == nullptr);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageFound, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageNotFound, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::
          kCrossFrameRetrieval,
      0);
}

// Test that the retrieved image node is null if another hit test has been
// conducted in the same controller before the retrieval occurred.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageRetrievalAfterCachedImageReset) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'>Content</a>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location_with_image(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location_with_image, kMenuSourceLongPress));

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));
  ShowContextMenu(location_with_link, kMenuSourceLongPress);

  Node* image_node =
      web_view_helper_.GetWebView()
          ->GetPage()
          ->GetContextMenuController()
          .ContextMenuImageNodeForFrame(GetDocument()->GetFrame());
  EXPECT_TRUE(image_node == nullptr);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageFound, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageNotFound, 1);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::
          kCrossFrameRetrieval,
      0);
}

// Test that the retrieved image node is null if the retrieval frame is
// different than the one used in the initial context menu image selection.
TEST_F(ContextMenuControllerTest,
       ContextMenuImageRetrievalCachedImageCrossFrame) {
  RegisterMockedImageURLLoad("http://test.png");
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #occluder {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 2;
        }
      </style>
      <img id=target src='http://test.png'>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location_with_image(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location_with_image, kMenuSourceLongPress));

  // Pass in nullptr for frame reference as a way of simulating a different
  // frame being passed in.
  Node* image_node = web_view_helper_.GetWebView()
                         ->GetPage()
                         ->GetContextMenuController()
                         .ContextMenuImageNodeForFrame(nullptr);
  EXPECT_TRUE(image_node == nullptr);

  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageFound, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::kImageNotFound, 0);
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome",
      ContextMenuController::ImageSelectionRetrievalOutcome::
          kCrossFrameRetrieval,
      1);
}

TEST_F(ContextMenuControllerTest, OpenedFromHighlight) {
  WebURL url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(LocalMainFrame(),
                                     R"(<html><head><style>body
      {background-color:transparent}</style></head>
      <p id="one">This is a test page one</p>
      <p id="two">This is a test page two</p>
      <p id="three">This is a test page three</p>
      <p id="four">This is a test page four</p>
      </html>
      )",
                                     url);

  Document* document = GetDocument();
  ASSERT_TRUE(IsA<HTMLDocument>(document));

  Element* first_element = document->getElementById(AtomicString("one"));
  Element* middle_element = document->getElementById(AtomicString("one"));
  Element* third_element = document->getElementById(AtomicString("three"));
  Element* last_element = document->getElementById(AtomicString("four"));

  // Install a text fragment marker from the beginning of <p> one to near the
  // end of <p> three.
  EphemeralRange dom_range =
      EphemeralRange(Position(first_element->firstChild(), 0),
                     Position(third_element->firstChild(), 22));
  document->Markers().AddTextFragmentMarker(dom_range);
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Opening the context menu from the last <p> should not set
  // |opened_from_highlight|.
  EXPECT_TRUE(ShowContextMenuForElement(last_element, kMenuSourceMouse));
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_FALSE(context_menu_data.opened_from_highlight);

  // Opening the context menu from the second <p> should set
  // |opened_from_highlight|.
  EXPECT_TRUE(ShowContextMenuForElement(middle_element, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.opened_from_highlight);

  // Opening the context menu from the middle of the third <p> should set
  // |opened_from_highlight|.
  EXPECT_TRUE(ShowContextMenuForElement(third_element, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.opened_from_highlight);
}

// Test that opening context menu with keyboard does not change text selection.
TEST_F(ContextMenuControllerTest,
       KeyboardTriggeredContextMenuPreservesSelection) {
  ContextMenuAllowedScope context_menu_allowed_scope;

  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <p id='first'>This is a sample text."</p>
    </body>
  )HTML");

  Node* first_paragraph =
      GetDocument()->getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 5);
  const auto& selected_end = Position(first_paragraph, 9);

  GetDocument()->GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selected_start, selected_end)
          .Build(),
      SetSelectionOptions());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDocument()->GetFrame()->Selection().SelectedText(), "is a");

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceKeyboard));
  EXPECT_EQ(GetDocument()->GetFrame()->Selection().SelectedText(), "is a");
}

TEST_F(ContextMenuControllerTest, CheckRendererIdFromContextMenuOnTextField) {
  WebURL url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(LocalMainFrame(),
                                     R"(<html><head><style>body
      {background-color:transparent}</style></head>
      <form>
      <label for="name">Name:</label><br>
      <input type="text" id="name" name="name"><br>
      <label for="address">Address:</label><br>
      <textarea id="address" name="address"></textarea>
      </form>
      <p id="one">This is a test page one</p>
      <label for="two">Two:</label><br>
      <input type="text" id="two" name="two"><br>
      <label for="three">Three:</label><br>
      <textarea id="three" name="three"></textarea>
      </html>
      )",
                                     url);

  Document* document = GetDocument();
  ASSERT_TRUE(IsA<HTMLDocument>(document));

  // field_id, is_form_renderer_id_present, is_field_renderer_id_present,
  // form_control_type
  std::vector<std::tuple<AtomicString, bool, bool,
                         std::optional<mojom::FormControlType>>>
      expectations = {// Input Text Field
                      {AtomicString("name"), true, true,
                       mojom::FormControlType::kInputText},
                      // Text Area Field
                      {AtomicString("address"), true, true,
                       mojom::FormControlType::kTextArea},
                      // Non form element
                      {AtomicString("one"), false, false, std::nullopt},
                      // Formless Input field
                      {AtomicString("two"), false, true,
                       mojom::FormControlType::kInputText},
                      // Formless text area field
                      {AtomicString("three"), false, true,
                       mojom::FormControlType::kTextArea}};

  for (const auto& expectation : expectations) {
    auto [field_id, is_form_renderer_id_present, is_field_renderer_id_present,
          form_control_type] = expectation;
    Element* form_element = document->getElementById(field_id);
    EXPECT_TRUE(ShowContextMenuForElement(form_element, kMenuSourceMouse));
    ContextMenuData context_menu_data =
        GetWebFrameClient().GetContextMenuData();
    EXPECT_EQ(context_menu_data.form_renderer_id != 0,
              is_form_renderer_id_present);
    EXPECT_EQ(context_menu_data.form_control_type, form_control_type);
  }
}

TEST_F(ContextMenuControllerTest, AttributionSrc) {
  // The context must be secure for attributionsrc to work at all.
  frame_test_helpers::LoadHTMLString(
      LocalMainFrame(), R"(<html><body>)",
      url_test_helpers::ToKURL("https://test.com/"));

  static constexpr char kSecureURL[] = "https://a.com/";
  static constexpr char kInsecureURL[] = "http://b.com/";

  const struct {
    const char* href;
    const char* attributionsrc;
    bool impression_expected;
  } kTestCases[] = {
      {
          .href = nullptr,
          .attributionsrc = nullptr,
          .impression_expected = false,
      },
      {
          .href = nullptr,
          .attributionsrc = "",
          .impression_expected = false,
      },
      {
          .href = nullptr,
          .attributionsrc = kInsecureURL,
          .impression_expected = false,
      },
      {
          .href = nullptr,
          .attributionsrc = kSecureURL,
          .impression_expected = false,
      },
      {
          .href = kInsecureURL,
          .attributionsrc = nullptr,
          .impression_expected = false,
      },
      {
          .href = kInsecureURL,
          .attributionsrc = "",
          .impression_expected = false,
      },
      {
          .href = kInsecureURL,
          .attributionsrc = kInsecureURL,
          .impression_expected = false,
      },
      {
          .href = kInsecureURL,
          .attributionsrc = kSecureURL,
          .impression_expected = false,
      },
      {
          .href = kSecureURL,
          .attributionsrc = nullptr,
          .impression_expected = false,
      },
      {
          .href = kSecureURL,
          .attributionsrc = "",
          .impression_expected = true,
      },
      {
          .href = kSecureURL,
          .attributionsrc = kInsecureURL,
          .impression_expected = true,
      },
      {
          .href = kSecureURL,
          .attributionsrc = kSecureURL,
          .impression_expected = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    Persistent<HTMLAnchorElement> anchor =
        MakeGarbageCollected<HTMLAnchorElement>(*GetDocument());
    anchor->setInnerText("abc");

    if (test_case.href)
      anchor->SetHref(AtomicString(test_case.href));

    if (test_case.attributionsrc) {
      anchor->setAttribute(html_names::kAttributionsrcAttr,
                           AtomicString(test_case.attributionsrc));
    }

    GetPage()->SetAttributionSupport(network::mojom::AttributionSupport::kWeb);

    GetDocument()->body()->AppendChild(anchor);
    ASSERT_TRUE(ShowContextMenuForElement(anchor, kMenuSourceMouse));

    url_test_helpers::ServeAsynchronousRequests();

    ContextMenuData context_menu_data =
        GetWebFrameClient().GetContextMenuData();

    EXPECT_EQ(context_menu_data.impression.has_value(),
              test_case.impression_expected);
  }
}

// Test that if text selection contains unselectable content, the opened context
// menu should omit the unselectable content.
TEST_F(ContextMenuControllerTest, SelectUnselectableContent) {
  GetDocument()->documentElement()->setInnerHTML(R"HTML(
    <body>
      <p id="test">A <span style="user-select:none;">test_none <span>test_span
        </span><span style="user-select:all;">test_all</span></span> B</p>
    </body>
  )HTML");

  Document* document = GetDocument();
  Element* element = document->getElementById(AtomicString("test"));

  // Select text, which has nested unselectable and selectable content.
  const auto& start = Position(element->firstChild(), 0);
  const auto& end = Position(element->lastChild(), 2);
  document->GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(start, end).Build(),
      SetSelectionOptions());

  // The context menu should omit the unselectable content from the selected
  // text.
  EXPECT_TRUE(ShowContextMenuForElement(element, kMenuSourceMouse));
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "A test_all B");
}

class ContextMenuControllerRemoteParentFrameTest : public testing::Test {
 public:
  ContextMenuControllerRemoteParentFrameTest() = default;

  void SetUp() override {
    web_view_helper_.InitializeRemote();
    web_view_helper_.RemoteMainFrame()->View()->DisableAutoResizeForTesting(
        gfx::Size(640, 480));

    child_frame_ = web_view_helper_.CreateLocalChild(
        *web_view_helper_.RemoteMainFrame(),
        /*name=*/"child",
        /*properties=*/{},
        /*previous_sibling=*/nullptr, &child_web_frame_client_);
    frame_test_helpers::LoadFrame(child_frame_, "data:text/html,some page");

    auto& focus_controller =
        child_frame_->GetFrame()->GetPage()->GetFocusController();
    focus_controller.SetActive(true);
    focus_controller.SetFocusedFrame(child_frame_->GetFrame());
  }

  void ShowContextMenu(const gfx::Point& point) {
    child_frame_->LocalRootFrameWidget()->ShowContextMenu(
        ui::mojom::MenuSourceType::MOUSE, point);
    base::RunLoop().RunUntilIdle();
  }

  const TestWebFrameClientImpl& child_web_frame_client() const {
    return child_web_frame_client_;
  }

 protected:
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestWebFrameClientImpl child_web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  Persistent<WebLocalFrameImpl> child_frame_;
};

TEST_F(ContextMenuControllerRemoteParentFrameTest, ShowContextMenuInChild) {
  const gfx::Point kPoint(123, 234);
  ShowContextMenu(kPoint);

  const std::optional<gfx::Point>& host_context_menu_location =
      child_web_frame_client().host_context_menu_location();
  ASSERT_TRUE(host_context_menu_location.has_value());
  EXPECT_EQ(kPoint, host_context_menu_location.value());
}

// Test the field of `context_menu_data` `is_password_type_by_heuristics` which
// should be set if a field's type is plain text but heuristics (e.g. the name
// attribute contains 'password' as a substring) recognize it as a password
// field.
TEST_F(ContextMenuControllerTest, IsPasswordTypeByHeuristic) {
  WebURL url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(LocalMainFrame(),
                                     R"(<html>
        <form>
          <input type="password" id="not_heuristic"></textarea>
          <input id="not_related"></textarea>
          <input id="heuristic_password"></textarea>
          <input id="MyPwd"></textarea>
          <input id="moja_lOzinKa123"></textarea>
        </form>
      </html>
      )",
                                     url);
  Document* document = GetDocument();
  ASSERT_TRUE(IsA<HTMLDocument>(document));

  // Heuristics-based recognition is not needed, it is a clear password by
  // form_control_type.
  Element* not_heuristic_password =
      document->getElementById(AtomicString("not_heuristic"));
  EXPECT_TRUE(
      ShowContextMenuForElement(not_heuristic_password, kMenuSourceMouse));
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_FALSE(context_menu_data.is_password_type_by_heuristics);

  // Unrelated text field should not be recognized as password field.
  Element* not_related = document->getElementById(AtomicString("not_related"));
  EXPECT_TRUE(ShowContextMenuForElement(not_related, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_FALSE(context_menu_data.is_password_type_by_heuristics);

  // Field is of type 'text' and has 'password' in its id. Therefore, is
  // password type by heuristics.
  Element* heuristic_password =
      document->getElementById(AtomicString("heuristic_password"));
  EXPECT_TRUE(ShowContextMenuForElement(heuristic_password, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.is_password_type_by_heuristics);

  // Field is of type 'text' and has 'pwd' in its id. Therefore, is
  // password type by heuristics.
  Element* short_password = document->getElementById(AtomicString("MyPwd"));
  EXPECT_TRUE(ShowContextMenuForElement(short_password, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.is_password_type_by_heuristics);

  // Field is of type 'text' and has 'lozinka' (a foreign translation of
  // password) in its id. Therefore, is password type by heuristics.
  Element* foreign_password =
      document->getElementById(AtomicString("moja_lOzinKa123"));
  EXPECT_TRUE(ShowContextMenuForElement(foreign_password, kMenuSourceMouse));
  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.is_password_type_by_heuristics);
}

// Test the field of `context_menu_data` `is_password_type_by_heuristics` which
// should be set if a field's type is plain text and `HasBeenPassword` returns
// true (due to either server predictions or user's masking of input values).
TEST_F(ContextMenuControllerTest, HasBeenPasswordHeuristic) {
  WebURL url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(LocalMainFrame(),
                                     R"(<html>
        <form>
          <input type="text" id="has_been_password">
        </form>
      </html>
      )",
                                     url);
  Document* document = GetDocument();
  ASSERT_TRUE(IsA<HTMLDocument>(document));

  Element* input_element =
      document->getElementById(AtomicString("has_been_password"));
  ASSERT_TRUE(input_element);

  DynamicTo<HTMLInputElement>(input_element)->SetHasBeenPasswordField();

  ASSERT_TRUE(ShowContextMenuForElement(input_element, kMenuSourceMouse));
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.is_password_type_by_heuristics);
}

}  // namespace blink
