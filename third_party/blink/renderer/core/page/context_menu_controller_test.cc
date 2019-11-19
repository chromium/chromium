// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/context_menu_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_menu_source_type.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::Return;

namespace blink {

namespace {

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
  void ShowContextMenu(const WebContextMenuData& data) override {
    context_menu_data_ = data;
  }

  WebMediaPlayer* CreateMediaPlayer(const WebMediaPlayerSource&,
                                    WebMediaPlayerClient*,
                                    blink::MediaInspectorContext*,
                                    WebMediaPlayerEncryptedMediaClient*,
                                    WebContentDecryptionModule*,
                                    const WebString& sink_id) override {
    return new MockWebMediaPlayerForContextMenu();
  }

  const WebContextMenuData& GetContextMenuData() const {
    return context_menu_data_;
  }

 private:
  WebContextMenuData context_menu_data_;
};

}  // anonymous namespace

class ContextMenuControllerTest : public testing::Test {
 public:
  void SetUp() override {
    web_view_helper_.Initialize(&web_frame_client_);

    WebLocalFrameImpl* local_main_frame = web_view_helper_.LocalMainFrame();
    local_main_frame->ViewImpl()->MainFrameWidget()->Resize(WebSize(640, 480));
    local_main_frame->ViewImpl()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  bool ShowContextMenu(const PhysicalOffset& location,
                       WebMenuSourceType source) {
    return web_view_helper_.GetWebView()
        ->GetPage()
        ->GetContextMenuController()
        .ShowContextMenu(GetDocument()->GetFrame(), location, source);
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

 private:
  TestWebFrameClientImpl web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(ContextMenuControllerTest, VideoNotLoaded) {
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
  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(ContextMenuDataMediaType::kVideo, context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.GetString());

  const Vector<std::pair<WebContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {WebContextMenuData::kMediaInError, false},
          {WebContextMenuData::kMediaPaused, true},
          {WebContextMenuData::kMediaMuted, false},
          {WebContextMenuData::kMediaLoop, false},
          {WebContextMenuData::kMediaCanSave, true},
          {WebContextMenuData::kMediaHasAudio, false},
          {WebContextMenuData::kMediaCanToggleControls, false},
          {WebContextMenuData::kMediaControls, false},
          {WebContextMenuData::kMediaCanPrint, false},
          {WebContextMenuData::kMediaCanRotate, false},
          {WebContextMenuData::kMediaCanPictureInPicture, false},
          {WebContextMenuData::kMediaPictureInPicture, false},
          {WebContextMenuData::kMediaCanLoop, true},
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
  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(ContextMenuDataMediaType::kAudio, context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.GetString());

  const Vector<std::pair<WebContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {WebContextMenuData::kMediaInError, false},
          {WebContextMenuData::kMediaPaused, true},
          {WebContextMenuData::kMediaMuted, false},
          {WebContextMenuData::kMediaLoop, false},
          {WebContextMenuData::kMediaCanSave, true},
          {WebContextMenuData::kMediaHasAudio, true},
          {WebContextMenuData::kMediaCanToggleControls, false},
          {WebContextMenuData::kMediaControls, false},
          {WebContextMenuData::kMediaCanPrint, false},
          {WebContextMenuData::kMediaCanRotate, false},
          {WebContextMenuData::kMediaCanPictureInPicture, false},
          {WebContextMenuData::kMediaPictureInPicture, false},
          {WebContextMenuData::kMediaCanLoop, true},
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
  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(ContextMenuDataMediaType::kVideo, context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.GetString());

  const Vector<std::pair<WebContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {WebContextMenuData::kMediaInError, false},
          {WebContextMenuData::kMediaPaused, true},
          {WebContextMenuData::kMediaMuted, false},
          {WebContextMenuData::kMediaLoop, false},
          {WebContextMenuData::kMediaCanSave, true},
          {WebContextMenuData::kMediaHasAudio, false},
          {WebContextMenuData::kMediaCanToggleControls, true},
          {WebContextMenuData::kMediaControls, false},
          {WebContextMenuData::kMediaCanPrint, false},
          {WebContextMenuData::kMediaCanRotate, false},
          {WebContextMenuData::kMediaCanPictureInPicture, true},
          {WebContextMenuData::kMediaPictureInPicture, false},
          {WebContextMenuData::kMediaCanLoop, true},
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
  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(ContextMenuDataMediaType::kVideo, context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.GetString());

  const Vector<std::pair<WebContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {WebContextMenuData::kMediaInError, false},
          {WebContextMenuData::kMediaPaused, true},
          {WebContextMenuData::kMediaMuted, false},
          {WebContextMenuData::kMediaLoop, false},
          {WebContextMenuData::kMediaCanSave, true},
          {WebContextMenuData::kMediaHasAudio, false},
          {WebContextMenuData::kMediaCanToggleControls, true},
          {WebContextMenuData::kMediaControls, false},
          {WebContextMenuData::kMediaCanPrint, false},
          {WebContextMenuData::kMediaCanRotate, false},
          {WebContextMenuData::kMediaCanPictureInPicture, false},
          {WebContextMenuData::kMediaPictureInPicture, false},
          {WebContextMenuData::kMediaCanLoop, true},
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
  blink::WebMediaStream web_media_stream;
  blink::WebVector<blink::WebMediaStreamTrack> dummy_tracks;
  web_media_stream.Initialize(dummy_tracks, dummy_tracks);
  video->SetSrcObject(web_media_stream);
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
  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(ContextMenuDataMediaType::kVideo, context_menu_data.media_type);

  const Vector<std::pair<WebContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {WebContextMenuData::kMediaInError, false},
          {WebContextMenuData::kMediaPaused, true},
          {WebContextMenuData::kMediaMuted, false},
          {WebContextMenuData::kMediaLoop, false},
          {WebContextMenuData::kMediaCanSave, false},
          {WebContextMenuData::kMediaHasAudio, false},
          {WebContextMenuData::kMediaCanToggleControls, true},
          {WebContextMenuData::kMediaControls, false},
          {WebContextMenuData::kMediaCanPrint, false},
          {WebContextMenuData::kMediaCanRotate, false},
          {WebContextMenuData::kMediaCanPictureInPicture, true},
          {WebContextMenuData::kMediaPictureInPicture, false},
          {WebContextMenuData::kMediaCanLoop, false},
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
  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(ContextMenuDataMediaType::kVideo, context_menu_data.media_type);
  EXPECT_EQ(video_url, context_menu_data.src_url.GetString());

  const Vector<std::pair<WebContextMenuData::MediaFlags, bool>>
      expected_media_flags = {
          {WebContextMenuData::kMediaInError, false},
          {WebContextMenuData::kMediaPaused, true},
          {WebContextMenuData::kMediaMuted, false},
          {WebContextMenuData::kMediaLoop, false},
          {WebContextMenuData::kMediaCanSave, false},
          {WebContextMenuData::kMediaHasAudio, false},
          {WebContextMenuData::kMediaCanToggleControls, true},
          {WebContextMenuData::kMediaControls, false},
          {WebContextMenuData::kMediaCanPrint, false},
          {WebContextMenuData::kMediaCanRotate, false},
          {WebContextMenuData::kMediaCanPictureInPicture, true},
          {WebContextMenuData::kMediaPictureInPicture, false},
          {WebContextMenuData::kMediaCanLoop, false},
      };

  for (const auto& expected_media_flag : expected_media_flags) {
    EXPECT_EQ(expected_media_flag.second,
              !!(context_menu_data.media_flags & expected_media_flag.first))
        << "Flag 0x" << std::hex << expected_media_flag.first;
  }
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

  Element* text_element = document->getElementById("t");
  document->UpdateStyleAndLayout();
  FrameSelection& selection = document->GetFrame()->Selection();

  // <text> element
  selection.SelectSubString(*text_element, 4, 8);
  EXPECT_TRUE(ShowContextMenuForElement(text_element, kMenuSourceMouse));

  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type, ContextMenuDataMediaType::kNone);
  EXPECT_EQ(context_menu_data.edit_flags, ContextMenuDataEditFlags::kCanCopy);
  EXPECT_EQ(context_menu_data.selected_text, "able tex");

  // <div contenteditable=true>
  Element* editable_element = document->getElementById("e");
  selection.SelectSubString(*editable_element, 0, 42);
  EXPECT_TRUE(ShowContextMenuForElement(editable_element, kMenuSourceMouse));

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type, ContextMenuDataMediaType::kNone);
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
  ASSERT_TRUE(document->IsXMLDocument());
  ASSERT_FALSE(document->IsHTMLDocument());

  Element* text_element = document->getElementById("t");
  document->UpdateStyleAndLayout();
  FrameSelection& selection = document->GetFrame()->Selection();

  selection.SelectAll();
  EXPECT_TRUE(ShowContextMenuForElement(text_element, kMenuSourceMouse));

  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.media_type, ContextMenuDataMediaType::kNone);
  EXPECT_EQ(context_menu_data.edit_flags, ContextMenuDataEditFlags::kCanCopy);
  EXPECT_EQ(context_menu_data.selected_text, "Blue text");
}

TEST_F(ContextMenuControllerTest, ShowNonLocatedContextMenuEvent) {
  GetDocument()->documentElement()->SetInnerHTMLFromString(
      "<input id='sample' type='text' size='5' value='Sample Input Text'>");

  Document* document = GetDocument();
  Element* input_element = document->getElementById("sample");
  document->UpdateStyleAndLayout();

  // Select the 'Sample' of |input|.
  DOMRect* rect = input_element->getBoundingClientRect();
  WebGestureEvent gesture_event(
      WebInputEvent::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(WebFloatPoint(rect->left(), rect->top()));
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(gesture_event));

  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample");

  // Adjust the selection from the start of |input| to the middle.
  LayoutPoint middle_point((rect->left() + rect->right()) / 2,
                           (rect->top() + rect->bottom()) / 2);
  LocalMainFrame()->MoveRangeSelectionExtent(
      WebPoint(middle_point.X().ToInt(), middle_point.Y().ToInt()));
  GetWebView()->MainFrameWidget()->ShowContextMenu(kMenuSourceTouchHandle);

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_NE(context_menu_data.selected_text, "");

  // Scroll the value of |input| to end.
  input_element->setScrollLeft(input_element->scrollWidth());

  // Select all the value of |input| to ensure the start of selection is
  // invisible.
  LocalMainFrame()->MoveRangeSelectionExtent(
      WebPoint(rect->right(), rect->bottom()));
  GetWebView()->MainFrameWidget()->ShowContextMenu(kMenuSourceTouchHandle);

  context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ(context_menu_data.selected_text, "Sample Input Text");
}

TEST_F(ContextMenuControllerTest, SelectionRectClipped) {
  GetDocument()->documentElement()->SetInnerHTMLFromString(
      "<textarea id='text-area' cols=6 rows=2>Sample editable text</textarea>");

  Document* document = GetDocument();
  Element* editable_element = document->getElementById("text-area");
  document->UpdateStyleAndLayout();
  FrameSelection& selection = document->GetFrame()->Selection();

  // Select the 'Sample' of |textarea|.
  DOMRect* rect = editable_element->getBoundingClientRect();
  WebGestureEvent gesture_event(
      WebInputEvent::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(WebFloatPoint(rect->left(), rect->top()));
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(gesture_event));

  WebContextMenuData context_menu_data =
      GetWebFrameClient().GetContextMenuData();
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
  WebRect selection_rect(left, top, right - left, bottom - top);
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
  selection_rect = WebRect(left, top, right - left, bottom - top);
  EXPECT_EQ(context_menu_data.selection_rect, selection_rect);
}

}  // namespace blink
