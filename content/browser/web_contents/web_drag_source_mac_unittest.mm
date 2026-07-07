// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_drag_source_mac.h"

#include "base/apple/foundation_util.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest_mac.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "url/origin.h"

namespace content {

using WebDragSourceMacTest = RenderViewHostTestHarness;

TEST_F(WebDragSourceMacTest, DragInvalidlyEscapedBookmarklet) {
  DropData drop_data;
  drop_data.url_infos = {ui::ClipboardUrlInfo{GURL("javascript:%"), u""}};

  WebDragSource* source =
      [[WebDragSource alloc] initWithHost:nullptr
                          renderProcessId:content::ChildProcessId()
                            documentToken:blink::DocumentToken()
                             sourceOrigin:url::Origin()
                                 dropData:drop_data
                             isPrivileged:NO];

  // Test that asking for the data of an invalidly-escaped URL doesn't throw any
  // exceptions. http://crbug.com/128371
  id result = [source pasteboardPropertyListForType:NSPasteboardTypeURL];
  NSString* result_string = base::apple::ObjCCast<NSString>(result);
  EXPECT_NSEQ(@"javascript:%25", result_string);
}

// The primary source (created via the full initializer) exposes the first URL
// through the standard URL type and the complete URL/title list through the
// WebKit-compatible flavor, so Chromium/WebKit readers can recover every URL.
TEST_F(WebDragSourceMacTest, MultipleURLsPrimaryItemCarriesFullList) {
  DropData drop_data;
  drop_data.url_infos = {
      ui::ClipboardUrlInfo{GURL("https://www.chromium.org/"), u"Chromium"},
      ui::ClipboardUrlInfo{GURL("https://www.mozilla.org/"), u"Mozilla"},
      ui::ClipboardUrlInfo{GURL("https://webkit.org/"), u"WebKit"},
  };

  WebDragSource* primary =
      [[WebDragSource alloc] initWithHost:nullptr
                          renderProcessId:content::ChildProcessId()
                            documentToken:blink::DocumentToken()
                             sourceOrigin:url::Origin()
                                 dropData:drop_data
                             isPrivileged:NO];

  // The standard URL type exposes only the first URL.
  EXPECT_NSEQ(@"https://www.chromium.org/",
              [primary pasteboardPropertyListForType:NSPasteboardTypeURL]);

  // The WebKit flavor carries all URLs and titles as @[ urls, titles ].
  id plist = [primary
      pasteboardPropertyListForType:ui::kUTTypeWebKitWebUrlsWithTitles];
  NSArray* array = base::apple::ObjCCast<NSArray>(plist);
  ASSERT_EQ(2u, array.count);
  EXPECT_NSEQ((@[
                @"https://www.chromium.org/", @"https://www.mozilla.org/",
                @"https://webkit.org/"
              ]),
              array[0]);
  EXPECT_NSEQ((@[ @"Chromium", @"Mozilla", @"WebKit" ]), array[1]);
}

}  // namespace content
