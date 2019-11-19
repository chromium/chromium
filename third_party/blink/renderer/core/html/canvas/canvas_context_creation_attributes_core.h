// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CORE_EXPORT CanvasContextCreationAttributesCore {
  DISALLOW_NEW();

 public:
  CanvasContextCreationAttributesCore();
  CanvasContextCreationAttributesCore(
      blink::CanvasContextCreationAttributesCore const&);
  virtual ~CanvasContextCreationAttributesCore();

  bool alpha = true;
  bool antialias = true;
  String color_space = "srgb";
  bool depth = true;
  bool fail_if_major_performance_caveat = false;
  bool desynchronized = false;
  String pixel_format = "uint8";
  bool premultiplied_alpha = true;
  bool preserve_drawing_buffer = false;
  String power_preference = "default";
  bool stencil = false;
  bool xr_compatible = false;
};

}  // namespace blink

#endif  // CanvasContextCreationAttributes_h
