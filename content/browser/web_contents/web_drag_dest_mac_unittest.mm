// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_drag_dest_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/drop_data.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/dragdrop/cocoa_dnd_util.h"
#import "ui/base/test/cocoa_helper.h"

using content::DropData;
using content::RenderViewHostImplTestHarness;

namespace {
NSString* const kCrCorePasteboardFlavorType_url =
    @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
NSString* const kCrCorePasteboardFlavorType_urln =
    @"CorePasteboardFlavorType 0x75726C6E"; // 'urln'  title
}  // namespace

class WebDragDestTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    drag_dest_.reset([[WebDragDest alloc] initWithWebContentsImpl:contents()]);
  }

  void PutURLOnPasteboard(NSString* urlString, NSPasteboard* pboard) {
    [pboard declareTypes:[NSArray arrayWithObject:NSURLPboardType]
                   owner:nil];
    NSURL* url = [NSURL URLWithString:urlString];
    EXPECT_TRUE(url);
    [url writeToPasteboard:pboard];
  }

  void PutCoreURLAndTitleOnPasteboard(NSString* urlString, NSString* title,
                                      NSPasteboard* pboard) {
    [pboard
        declareTypes:[NSArray arrayWithObjects:kCrCorePasteboardFlavorType_url,
                                               kCrCorePasteboardFlavorType_urln,
                                               nil]
               owner:nil];
    [pboard setString:urlString
              forType:kCrCorePasteboardFlavorType_url];
    [pboard setString:title
              forType:kCrCorePasteboardFlavorType_urln];
  }

  base::mac::ScopedNSAutoreleasePool pool_;
  base::scoped_nsobject<WebDragDest> drag_dest_;
};

// Make sure nothing leaks.
TEST_F(WebDragDestTest, Init) {
  EXPECT_TRUE(drag_dest_);
}

TEST_F(WebDragDestTest, URL) {
  NSString* url = nil;
  NSString* title = nil;
  GURL result_url;
  std::u16string result_title;

  // Put a URL on the pasteboard and check it.
  scoped_refptr<ui::UniquePasteboard> pboard = new ui::UniquePasteboard;
  url = @"http://www.google.com/";
  PutURLOnPasteboard(url, pboard->get());
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(&result_url, &result_title,
                                                    pboard->get(), NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());

  // Put a 'url ' and 'urln' on the pasteboard and check it.
  pboard = new ui::UniquePasteboard;
  url = @"http://www.google.com/";
  title = @"Title of Awesomeness!",
  PutCoreURLAndTitleOnPasteboard(url, title, pboard->get());
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(&result_url, &result_title,
                                                    pboard->get(), NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);

  // Also check that it passes file:// via 'url '/'urln' properly.
  pboard = new ui::UniquePasteboard;
  url = @"file:///tmp/dont_delete_me.txt";
  title = @"very important";
  PutCoreURLAndTitleOnPasteboard(url, title, pboard->get());
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(&result_url, &result_title,
                                                    pboard->get(), NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);

  // And javascript:.
  pboard = new ui::UniquePasteboard;
  url = @"javascript:open('http://www.youtube.com/')";
  title = @"kill some time";
  PutCoreURLAndTitleOnPasteboard(url, title, pboard->get());
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(&result_url, &result_title,
                                                    pboard->get(), NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);

  pboard = new ui::UniquePasteboard;
  url = @"/bin/sh";
  [pboard->get() declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType]
                        owner:nil];
  [pboard->get() setPropertyList:[NSArray arrayWithObject:url]
                         forType:NSFilenamesPboardType];
  EXPECT_FALSE(ui::PopulateURLAndTitleFromPasteboard(&result_url, &result_title,
                                                     pboard->get(), NO));
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(&result_url, &result_title,
                                                    pboard->get(), YES));
  base::scoped_nsobject<NSURL> expected_output(
      [[NSURL alloc] initFileURLWithPath:url isDirectory:NO]);
  EXPECT_EQ([[expected_output absoluteString] UTF8String], result_url.spec());

  EXPECT_EQ("sh", base::UTF16ToUTF8(result_title));
}

TEST_F(WebDragDestTest, Data) {
  DropData data;
  scoped_refptr<ui::UniquePasteboard> pboard = new ui::UniquePasteboard;

  PutURLOnPasteboard(@"http://www.google.com", pboard->get());
  [pboard->get() addTypes:[NSArray arrayWithObjects:NSHTMLPboardType,
                                                    NSStringPboardType, nil]
                    owner:nil];
  NSString* htmlString = @"<html><body><b>hi there</b></body></html>";
  NSString* textString = @"hi there";
  [pboard->get() setString:htmlString forType:NSHTMLPboardType];
  [pboard->get() setString:textString forType:NSStringPboardType];
  content::PopulateDropDataFromPasteboard(&data, pboard->get());
  EXPECT_EQ(data.url.spec(), "http://www.google.com/");
  EXPECT_EQ(base::SysNSStringToUTF16(textString), data.text);
  EXPECT_EQ(base::SysNSStringToUTF16(htmlString), data.html);
}
