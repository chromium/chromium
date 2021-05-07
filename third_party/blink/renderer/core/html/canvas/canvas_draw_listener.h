// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_DRAW_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_DRAW_LISTENER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class WebGraphicsContext3DProviderWrapper;

class CORE_EXPORT CanvasDrawListener : public GarbageCollectedMixin {
 public:
  virtual ~CanvasDrawListener();
  virtual void SendNewFrame(
      scoped_refptr<StaticBitmapImage>,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) = 0;
  virtual bool NeedsNewFrame() const = 0;
  virtual void RequestFrame() = 0;

 protected:
  CanvasDrawListener();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_DRAW_LISTENER_H_
