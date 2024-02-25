// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_drag_source_mac.h"

#include "base/apple/foundation_util.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest_mac.h"
#include "url/origin.h"

namespace content {

using WebDragSourceMacTest = RenderViewHostTestHarness;

TEST_F(WebDragSourceMacTest, DragInvalidlyEscapedBookmarklet) {
  DropData drop_data;
  drop_data.url = GURL("javascript:%");

  WebDragSource* source = [[WebDragSource alloc] initWithHost:nullptr
                                                     dropData:drop_data
                                                 sourceOrigin:url::Origin()
                                                 isPrivileged:NO];

  // Test that asking for the data of an invalidly-escaped URL doesn't throw any
  // exceptions. http://crbug.com/128371
  id result = [source pasteboardPropertyListForType:NSPasteboardTypeURL];
  NSString* result_string = base::apple::ObjCCast<NSString>(result);
  EXPECT_NSEQ(@"javascript:%25", result_string);
}

}  // namespace content
