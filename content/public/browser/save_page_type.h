// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SAVE_PAGE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_SAVE_PAGE_TYPE_H_

namespace content {

enum SavePageType {
  // The value of the save type before its set by the user.
  SAVE_PAGE_TYPE_UNKNOWN = -1,
  // User chose to save only the HTML of the page.
  SAVE_PAGE_TYPE_AS_ONLY_HTML = 0,
  // User chose to save complete-html page.
  SAVE_PAGE_TYPE_AS_COMPLETE_HTML = 1,
  // User chose to save complete-html page as MHTML.
  SAVE_PAGE_TYPE_AS_MHTML = 2,

  // Insert new values BEFORE this value.
  SAVE_PAGE_TYPE_MAX,
};
}

#endif  // CONTENT_PUBLIC_BROWSER_SAVE_PAGE_TYPE_H_
