// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_

namespace content {

// Flags passed to the WebContentsDelegate.NavigationStateChanged to tell it
// what has changed. Combine them to update more than one thing.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.content_public.browser)
// GENERATED_JAVA_PREFIX_TO_STRIP: INVALIDATE_TYPE_
enum InvalidateTypes {
  INVALIDATE_TYPE_URL           = 1 << 0,  // The URL has changed.
  INVALIDATE_TYPE_TAB           = 1 << 1,  // The favicon, app icon, or crashed
                                           // state changed.
  INVALIDATE_TYPE_LOAD          = 1 << 2,  // The loading state has changed.
  INVALIDATE_TYPE_TITLE         = 1 << 3,  // The title changed.
  INVALIDATE_TYPE_AUDIO         = 1 << 4,  // The tab became audible or
                                           // inaudible.
                                           // TODO(crbug.com/846374):
                                           // remove this.

  INVALIDATE_TYPE_ALL           = (1 << 5) - 1,
};

}

#endif  // CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_
