// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/transferables.h"

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"

namespace blink {

Transferables::~Transferables() {
  // Explicitly free all backing stores for containers to avoid memory
  // regressions.
  // TODO(bikineev): Revisit after young generation is there.
  array_buffers.clear();
  image_bitmaps.clear();
  offscreen_canvases.clear();
  message_ports.clear();
  mojo_handles.clear();
  readable_streams.clear();
  writable_streams.clear();
  transform_streams.clear();
}

}  // namespace blink
