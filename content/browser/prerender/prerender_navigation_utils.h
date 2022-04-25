// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_NAVIGATION_UTILS_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_NAVIGATION_UTILS_H_

namespace content::prerender_navigation_utils {

// Returns true if the response code is disallowed for pre-rendering (e.g 404,
// etc), and false otherwise. This should be called only for the response of the
// main frame in a prerendered page.
bool IsDisallowedHttpResponseCode(int response_code);

}  // namespace content::prerender_navigation_utils

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_NAVIGATION_UTILS_H_
