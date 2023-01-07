// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

class WebClipboardImplTest : public ContentBrowserTest {
 public:
  WebClipboardImplTest() = default;
  ~WebClipboardImplTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebClipboardImplTest, PasteRTF) {
  BrowserTestClipboardScope clipboard;

  const std::string rtf_content = "{\\rtf1\\ansi Hello, {\\b world.}}";
  clipboard.SetRtf(rtf_content);

  FrameFocusedObserver focus_observer(
      shell()->web_contents()->GetPrimaryMainFrame());
  // paste_listener.html takes RTF from the clipboard and sets the title.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "paste_listener.html")));
  focus_observer.Wait();

  const std::u16string expected_title = base::UTF8ToUTF16(rtf_content);
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  shell()->web_contents()->Paste();
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

}  // namespace

}  // namespace content
