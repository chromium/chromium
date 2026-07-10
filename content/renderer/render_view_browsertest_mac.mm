// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>  // for the kVK_* constants.
#include <Cocoa/Cocoa.h>

#include "base/apple/owned_objc.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_frame_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::TestWebFrameContentDumper;

namespace content {

NSEvent* CmdDeadKeyEvent(NSEventType type, unsigned short code) {
  UniChar uniChar = 0;
  switch (code) {
    case kVK_UpArrow:
      uniChar = NSUpArrowFunctionKey;
      break;
    case kVK_DownArrow:
      uniChar = NSDownArrowFunctionKey;
      break;
    default:
      NOTREACHED();
  }
  NSString* s = [NSString stringWithFormat:@"%C", uniChar];

  return [NSEvent keyEventWithType:type
                          location:NSZeroPoint
                     modifierFlags:NSEventModifierFlagCommand
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:s
       charactersIgnoringModifiers:s
                         isARepeat:NO
                           keyCode:code];
}

// Test that cmd-up/down scrolls the page exactly if it is not intercepted by
// javascript.
TEST_F(RenderViewTest, MacTestCmdUp) {
  const char* kRawHtml = "<!DOCTYPE html>"
                         "<style>"
                         "  /* Add a vertical scrollbar */"
                         "  body { height: 10128px; }"
                         "</style>"
                         "<div id='keydown'></div>"
                         "<div id='scroll'></div>"
                         "<script>"
                         "  var allowKeyEvents = true;"
                         "  var scroll = document.getElementById('scroll');"
                         "  var result = document.getElementById('keydown');"
                         "  onkeydown = function(event) {"
                         "    result.textContent ="
                         "      event.keyCode + ',' +"
                         "      event.shiftKey + ',' +"
                         "      event.ctrlKey + ',' +"
                         "      event.metaKey + ',' +"
                         "      event.altKey;"
                         "    return allowKeyEvents;"
                         "  }"
                         "</script>";

  blink::web_pref::WebPreferences prefs;
  prefs.enable_scroll_animator = false;

  blink::WebFrameWidget* blink_widget =
      web_view_->MainFrame()->ToWebLocalFrame()->LocalRoot()->FrameWidget();

  web_view_->SetWebPreferences(prefs);

  const int kMaxOutputCharacters = 1024;
  std::string output;

  // First test when javascript does not eat keypresses -- should scroll.
  RenderFrameImpl::FromWebFrame(web_view_->MainFrame()->ToWebLocalFrame())
      ->set_send_content_state_immediately(true);
  LoadHTML(kRawHtml);

  const char* kArrowDownScrollDown = "40,false,false,true,false\n9844";
  blink_widget->AddEditCommandForNextKeyEvent(
      blink::WebString::FromLatin1("moveToEndOfDocument"), blink::WebString());
  input::NativeWebKeyboardEvent arrowDownKeyDown1(base::apple::OwnedNSEvent(
      CmdDeadKeyEvent(NSEventTypeKeyDown, kVK_DownArrow)));
  arrowDownKeyDown1.SetTimeStamp(base::TimeTicks::Now());
  SendNativeKeyEvent(arrowDownKeyDown1);
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = TestWebFrameContentDumper::DumpWebViewAsText(web_view_,
                                                        kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowDownScrollDown, output);

  const char* kArrowUpScrollUp = "38,false,false,true,false\n0";
  blink_widget->AddEditCommandForNextKeyEvent(
      blink::WebString::FromLatin1("moveToBeginningOfDocument"),
      blink::WebString());
  input::NativeWebKeyboardEvent arrowUpKeyDown1(base::apple::OwnedNSEvent(
      CmdDeadKeyEvent(NSEventTypeKeyDown, kVK_UpArrow)));
  arrowUpKeyDown1.SetTimeStamp(base::TimeTicks::Now());
  SendNativeKeyEvent(arrowUpKeyDown1);
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = TestWebFrameContentDumper::DumpWebViewAsText(web_view_,
                                                        kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowUpScrollUp, output);

  // Now let javascript eat the key events -- no scrolling should happen.
  // Set a scroll position slightly down the page to ensure that it does not
  // move.
  ExecuteJavaScriptForTests("allowKeyEvents = false; window.scrollTo(0, 100)");

  const char* kArrowDownNoScroll = "40,false,false,true,false\n100";
  blink_widget->AddEditCommandForNextKeyEvent(
      blink::WebString::FromLatin1("moveToEndOfDocument"), blink::WebString());
  input::NativeWebKeyboardEvent arrowDownKeyDown2(base::apple::OwnedNSEvent(
      CmdDeadKeyEvent(NSEventTypeKeyDown, kVK_DownArrow)));
  arrowDownKeyDown2.SetTimeStamp(base::TimeTicks::Now());
  SendNativeKeyEvent(arrowDownKeyDown2);
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = TestWebFrameContentDumper::DumpWebViewAsText(web_view_,
                                                        kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowDownNoScroll, output);

  const char* kArrowUpNoScroll = "38,false,false,true,false\n100";
  blink_widget->AddEditCommandForNextKeyEvent(
      blink::WebString::FromLatin1("moveToBeginningOfDocument"),
      blink::WebString());
  input::NativeWebKeyboardEvent arrowUpKeyDown2(base::apple::OwnedNSEvent(
      CmdDeadKeyEvent(NSEventTypeKeyDown, kVK_UpArrow)));
  arrowUpKeyDown2.SetTimeStamp(base::TimeTicks::Now());
  SendNativeKeyEvent(arrowUpKeyDown2);
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = TestWebFrameContentDumper::DumpWebViewAsText(web_view_,
                                                        kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowUpNoScroll, output);
}

}  // namespace content
