// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BLINK_TEST_BROWSER_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_BLINK_TEST_BROWSER_SUPPORT_H_

#include <string>

// A collections of browser-side functions designed for use with blink tests.
namespace content {
class RenderFrameHost;

// Gets a frame name from browser side.
std::string GetFrameNameFromBrowserForWebTests(
    RenderFrameHost* render_frame_host);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BLINK_TEST_BROWSER_SUPPORT_H_
