// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_INPUT_HANDLER_PROXY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_INPUT_HANDLER_PROXY_H_


namespace gfx {
class Point;
class ScrollOffset;
class SizeF;
}  // namespace gfx

namespace blink {

class SynchronousInputHandler {
 public:
  virtual ~SynchronousInputHandler() {}

  // Informs the Android WebView embedder of the current root scroll and page
  // scale state.
  virtual void UpdateRootLayerState(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) = 0;
};

// Android WebView requires synchronous scrolling from the WebView application.
// This interface provides support for that behaviour. The WebView embedder will
// act as the InputHandler for controlling the timing of input (fling)
// animations.
class SynchronousInputHandlerProxy {
 public:
  virtual ~SynchronousInputHandlerProxy() {}

  // SynchronousInputHandler needs to be informed of root layer updates.
  virtual void SetSynchronousInputHandler(
      SynchronousInputHandler* synchronous_input_handler) = 0;

  // Called when the synchronous input handler wants to change the root scroll
  // offset. Since it has the final say, this overrides values from compositor-
  // controlled behaviour. After the offset is applied, the
  // SynchronousInputHandler should be given back the result in case it differs
  // from what was sent.
  virtual void SynchronouslySetRootScrollOffset(
      const gfx::ScrollOffset& root_offset) = 0;

  // Similar to SetRootScrollOffset above, to control the zoom level, ie scale
  // factor. Note |magnify_delta| is an incremental rather than absolute value.
  // SynchronousInputHandler should be given back the resulting absolute value.
  virtual void SynchronouslyZoomBy(float magnify_delta,
                                   const gfx::Point& anchor) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_INPUT_HANDLER_PROXY_H_
