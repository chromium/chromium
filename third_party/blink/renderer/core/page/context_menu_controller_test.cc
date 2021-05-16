// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/context_menu_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/weburl_loader_mock.h"
#include "third_party/blink/renderer/platform/testing/weburl_loader_mock_factory_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

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

  SurfaceLayerMode GetVideoSurfaceLayerMode() const override {
    return SurfaceLayerMode::kAlways;
  }
};

class TestWebFrameClientImpl : public frame_test_helpers::TestWebFrameClient {
 public:
  void UpdateContextMenuDataForTesting(
      const ContextMenuData& data,
      const absl::optional<gfx::Point>&) override {
    context_menu_data_ = data;
  }

  WebMediaPlayer* CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings& settings) override {
    return new MockWebMediaPlayerForContextMenu();
  }

  const ContextMenuData& GetContextMenuData() const {
    return context_menu_data_;
  }

 private:
  ContextMenuData context_menu_data_;
};

void RegisterMockedImageURLLoad(const String& url) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL(url.Utf8().c_str()),
      test::CoreTestDataPath(kTestResourceFilename), kTestResourceMimeType);
}

}  // anonymous namespace

class ContextMenuControllerTest : public testing::Test,
                                  public ::testing::WithParamInterface<bool> {
 public:
  explicit ContextMenuControllerTest(
      bool penetrating_image_selection_enabled = GetParam()) {
    feature_list_.InitWithFeatureState(
        features::kEnablePenetratingImageSelection,
        penetrating_image_selection_enabled);
  }

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
    const DOMRect* rect = element->getBoundingClientRect();
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
  base::test::ScopedFeatureList feature_list_;
  TestWebFrameClientImpl web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

INSTANTIATE_TEST_SUITE_P(, ContextMenuControllerTest, ::testing::Bool());

TEST_P(ContextMenuControllerTest, VideoNotLoaded) {
  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  const char video_url[] = "https://example.com/foo.webm";

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

  DOMRect* rect = video->getBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec());

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

TEST_P(ContextMenuControllerTest, VideoWithAudioOnly) {
  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  const char video_url[] = "https://example.com/foo.webm";

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

  DOMRect* rect = video->getBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kAudio,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec());

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

TEST_P(ContextMenuControllerTest, PictureInPictureEnabledVideoLoaded) {
  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  const char video_url[] = "https://example.com/foo.webm";

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

  DOMRect* rect = video->getBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec());

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

TEST_P(ContextMenuControllerTest, PictureInPictureDisabledVideoLoaded) {
  // Make sure Picture-in-Picture is disabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(false);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  const char video_url[] = "https://example.com/foo.webm";

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

  DOMRect* rect = video->getBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec());

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

TEST_P(ContextMenuControllerTest, MediaStreamVideoLoaded) {
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
  video->SetSrcObject(media_stream_descriptor);
  GetDocument()->body()->AppendChild(video);
  test::RunPendingTasks();
  SetReadyState(video.Get(), HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  EXPECT_CALL(*static_cast<MockWebMediaPlayerForContextMenu*>(
                  video->GetWebMediaPlayer()),
              HasVideo())
      .WillRepeatedly(Return(true));

  DOMRect* rect = video->getBoundingClientRect();
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

TEST_P(ContextMenuControllerTest, InfiniteDurationVideoLoaded) {
  // Make sure Picture-in-Picture is enabled.
  GetDocument()->GetSettings()->SetPictureInPictureEnabled(true);

  ContextMenuAllowedScope context_menu_allowed_scope;
  HitTestResult hit_test_result;
  const char video_url[] = "https://example.com/foo.webm";

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

  DOMRect* rect = video->getBoundingClientRect();
  PhysicalOffset location(LayoutUnit((rect->left() + rect->right()) / 2),
                          LayoutUnit((rect->top() + rect->bottom()) / 2));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceMouse));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kVideo,
            context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.spec());

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

TEST_P(ContextMenuControllerTest, EditingActionsEnabledInSVGDocument) {
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

  Element* text_element = document->getElementById("t");
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
  Element* editable_element = document->getElementById("e");
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

TEST_P(ContextMenuControllerTest, EditingActionsEnabledInXMLDocument) {
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

  Element* text_element = document->getElementById("t");
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

TEST_P(ContextMenuControllerTest, ShowNonLocatedContextMenuEvent) {
  GetDocument()->documentElement()->setInnerHTML(
      "<input id='sample' type='text' size='5' value='Sample Input Text'>");

  Document* document = GetDocument();
  Element* input_element = document->getElementById("sample");
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Select the 'Sample' of |input|.
  DOMRect* rect = input_element->getBoundingClientRect();
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

#if !defined(OS_MAC)
// Mac has no way to open a context menu based on a keyboard event.
TEST_P(ContextMenuControllerTest,
       ValidateNonLocatedContextMenuOnLargeImageElement) {
  GetDocument()->documentElement()->setInnerHTML(
      "<img src=\"http://example.test/cat.jpg\" id=\"sample_image\" "
      "width=\"200\" height=\"10000\" tabindex=\"-1\" />");

  Document* document = GetDocument();
  Element* image_element = document->getElementById("sample_image");
  // Set focus on the image element.
  image_element->focus();
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

TEST_P(ContextMenuControllerTest, SelectionRectClipped) {
  GetDocument()->documentElement()->setInnerHTML(
      "<textarea id='text-area' cols=6 rows=2>Sample editable text</textarea>");

  Document* document = GetDocument();
  Element* editable_element = document->getElementById("text-area");
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  FrameSelection& selection = document->GetFrame()->Selection();

  // Select the 'Sample' of |textarea|.
  DOMRect* rect = editable_element->getBoundingClientRect();
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(gfx::PointF(rect->left(), rect->top()));
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(gesture_event, ui::LatencyInfo()));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample");

  // The selection rect is not clipped.
  IntRect anchor, focus;
  selection.ComputeAbsoluteBounds(anchor, focus);
  anchor = document->GetFrame()->View()->FrameToViewport(anchor);
  focus = document->GetFrame()->View()->FrameToViewport(focus);
  int left = std::min(focus.X(), anchor.X());
  int top = std::min(focus.Y(), anchor.Y());
  int right = std::max(focus.MaxX(), anchor.MaxX());
  int bottom = std::max(focus.MaxY(), anchor.MaxY());
  gfx::Rect selection_rect(left, top, right - left, bottom - top);
  EXPECT_EQ(context_menu_data.selection_rect, selection_rect);

  // Select all the content of |textarea|.
  selection.SelectAll();
  EXPECT_TRUE(ShowContextMenuForElement(editable_element, kMenuSourceMouse));

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample editable text");

  // The selection rect is clipped by the editable box.
  IntRect clip_bound = editable_element->VisibleBoundsInVisualViewport();
  selection.ComputeAbsoluteBounds(anchor, focus);
  anchor = document->GetFrame()->View()->FrameToViewport(anchor);
  focus = document->GetFrame()->View()->FrameToViewport(focus);
  left = std::max(clip_bound.X(), std::min(focus.X(), anchor.X()));
  top = std::max(clip_bound.Y(), std::min(focus.Y(), anchor.Y()));
  right = std::min(clip_bound.MaxX(), std::max(focus.MaxX(), anchor.MaxX()));
  bottom = std::min(clip_bound.MaxY(), std::max(focus.MaxY(), anchor.MaxY()));
  selection_rect = gfx::Rect(left, top, right - left, bottom - top);
  EXPECT_EQ(context_menu_data.selection_rect, selection_rect);
}

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext*, Event*));
};

// Test that a basic image hit test works without penetration enabled.
TEST_P(ContextMenuControllerTest, ContextMenuImageHitTestStandardImageControl) {
  if (base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
      </style>
      <img id=target src='http://test.png'>
    </body>
  )HTML");

  base::HistogramTester histograms;

  PhysicalOffset location(LayoutUnit(5), LayoutUnit(5));
  EXPECT_TRUE(ShowContextMenu(location, kMenuSourceLongPress));

  // Context menu info are sent to the WebLocalFrameClient.
  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("http://test.png/", context_menu_data.src_url.spec());
  // EXPECT_TRUE(context_menu_data.has_image_contents);
  EXPECT_EQ(mojom::blink::ContextMenuDataMediaType::kImage,
            context_menu_data.media_type);

  // No histograms should be sent in the control group.
  histograms.ExpectBucketCount(
      "Blink.ContextMenu.ImageSelection.Outcome",
      ContextMenuController::ImageSelectionOutcome::kImageFoundStandard, 0);
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

// Test that a basic image hit test works and is no† impacted by
// penetrating image selection logic.
TEST_P(ContextMenuControllerTest,
       ContextMenuImageHitTestStandardImageSelectionExperiment) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }

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
TEST_P(ContextMenuControllerTest, ContextMenuImageHitTestSucceededPenetrating) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
TEST_P(ContextMenuControllerTest, ContextMenuImageHitTestStandardCanvas) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
TEST_P(ContextMenuControllerTest, ContextMenuImageHitTestOpaqueNodeBlocking) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageHitTestContextMenuListenerAboveImageBlocking) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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

  Element* target_image = GetDocument()->getElementById("target");
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageHitTestContextMenuListenerOnImageBlocking) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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

  Element* target_image = GetDocument()->getElementById("target");
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageHitTestNonBlockingNonContextMenuListenerOnImage) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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

  Element* target_image = GetDocument()->getElementById("target");
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageHitTestStandardContextMenuListenerAncestorNonBlocking) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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

  Element* hidden_ancestor = GetDocument()->getElementById("hiddenancestor");
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageHitTestContextMenuListenerAncestorBlocking) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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

  Element* hidden_ancestor = GetDocument()->getElementById("hiddenancestor");
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
TEST_P(ContextMenuControllerTest, ContextMenuImageRetrievalCachedImageFound) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageRetrievalCachedImageNotFound) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageRetrievalAfterCachedImageReset) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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
TEST_P(ContextMenuControllerTest,
       ContextMenuImageRetrievalCachedImageCrossFrame) {
  if (!base::FeatureList::IsEnabled(
          features::kEnablePenetratingImageSelection)) {
    return;
  }
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

TEST_P(ContextMenuControllerTest, OpenedFromHighlight) {
  WebURL url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(LocalMainFrame(),
      R"(<html><head><style>body
      {background-color:transparent}</style></head>
      <p id="one">This is a test page one</p>
      <p id="two">This is a test page two</p>
      <p id="three">This is a test page three</p>
      <p id="four">This is a test page four</p>
      </html>
      )", url);

  Document* document = GetDocument();
  ASSERT_TRUE(IsA<HTMLDocument>(document));

  Element* first_element = document->getElementById("one");
  Element* middle_element = document->getElementById("one");
  Element* third_element = document->getElementById("three");
  Element* last_element = document->getElementById("four");

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

// TODO(crbug.com/1184996): Add additional unit test for blocking frame logging.

}  // namespace blink
