// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_WEB_VIEW_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_WEB_VIEW_TEST_HELPER_H_

namespace gfx {
class Point;
class Size;
}  // namespace gfx

namespace blink {

class WebView;
class WebHitTestResult;

// Do a hit test equivalent to what would be done for a GestureTap event
// that has width/height corresponding to the supplied |tapArea|.
WebHitTestResult HitTestResultForTap(WebView*,
                                     const gfx::Point& tap_point,
                                     const gfx::Size& tap_area);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_WEB_VIEW_TEST_HELPER_H_
