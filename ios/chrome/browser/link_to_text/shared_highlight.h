// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_SHARED_HIGHLIGHT_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_SHARED_HIGHLIGHT_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

// This struct holds a |URL| with a text fragment linking to |selectedText| on a
// Web page. It also holds that Web page's |title|.
struct SharedHighlight {
  SharedHighlight(const GURL& url, NSString* title, NSString* selectedText)
      : url(url), title(title), selectedText(selectedText) {}

  GURL url;
  NSString* title;
  NSString* selectedText;
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_SHARED_HIGHLIGHT_H_
