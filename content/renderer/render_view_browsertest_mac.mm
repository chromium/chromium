// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/text_input_client_messages.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

#include <Carbon/Carbon.h>  // for the kVK_* constants.
#include <Cocoa/Cocoa.h>

using blink::WebFrameContentDumper;
using blink::WebImeTextSpan;

namespace content {

NSEvent* CmdDeadKeyEvent(NSEventType type, unsigned short code) {
  UniChar uniChar = 0;
  switch(code) {
    case kVK_UpArrow:
      uniChar = NSUpArrowFunctionKey;
      break;
    case kVK_DownArrow:
      uniChar = NSDownArrowFunctionKey;
      break;
    default:
      CHECK(false);
  }
  NSString* s = [NSString stringWithFormat:@"%C", uniChar];

  return [NSEvent keyEventWithType:type
                          location:NSZeroPoint
                     modifierFlags:NSCommandKeyMask
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
  const char* kRawHtml =
      "<!DOCTYPE html>"
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

  WebPreferences prefs;
  prefs.enable_scroll_animator = false;

  RenderViewImpl* view = static_cast<RenderViewImpl*>(view_);
  view->OnUpdateWebPreferences(prefs);

  const int kMaxOutputCharacters = 1024;
  std::string output;

  NSEvent* arrowDownKeyDown = CmdDeadKeyEvent(NSKeyDown, kVK_DownArrow);
  NSEvent* arrowUpKeyDown = CmdDeadKeyEvent(NSKeyDown, kVK_UpArrow);

  // First test when javascript does not eat keypresses -- should scroll.
  view->set_send_content_state_immediately(true);
  LoadHTML(kRawHtml);
  render_thread_->sink().ClearMessages();

  const char* kArrowDownScrollDown = "40,false,false,true,false\n9844";
  view->GetWidget()->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToEndOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowDownKeyDown));
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::DumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowDownScrollDown, output);

  const char* kArrowUpScrollUp = "38,false,false,true,false\n0";
  view->GetWidget()->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToBeginningOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowUpKeyDown));
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::DumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowUpScrollUp, output);

  // Now let javascript eat the key events -- no scrolling should happen.
  // Set a scroll position slightly down the page to ensure that it does not
  // move.
  ExecuteJavaScriptForTests("allowKeyEvents = false; window.scrollTo(0, 100)");

  const char* kArrowDownNoScroll = "40,false,false,true,false\n100";
  view->GetWidget()->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToEndOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowDownKeyDown));
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::DumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowDownNoScroll, output);

  const char* kArrowUpNoScroll = "38,false,false,true,false\n100";
  view->GetWidget()->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToBeginningOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowUpKeyDown));
  base::RunLoop().RunUntilIdle();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::DumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .Ascii();
  EXPECT_EQ(kArrowUpNoScroll, output);
}

}  // namespace content
