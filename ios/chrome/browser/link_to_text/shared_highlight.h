// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_SHARED_HIGHLIGHT_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_SHARED_HIGHLIGHT_H_

#include "url/gurl.h"

// This struct holds a quote and a URL with a text fragment linking to
// the quote on the web page.
struct SharedHighlight {
  SharedHighlight(const GURL& url, std::string quote)
      : url(url), quote(quote) {}

  GURL url;
  std::string quote;
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_SHARED_HIGHLIGHT_H_
