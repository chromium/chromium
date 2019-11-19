// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLCanvasElement;
class OffscreenCanvas;

class CORE_EXPORT CanvasRenderingContextFactory {
  USING_FAST_MALLOC(CanvasRenderingContextFactory);

 public:
  CanvasRenderingContextFactory() = default;
  virtual ~CanvasRenderingContextFactory() = default;

  virtual CanvasRenderingContext* Create(
      CanvasRenderingContextHost*,
      const CanvasContextCreationAttributesCore&) = 0;

  virtual CanvasRenderingContext::ContextType GetContextType() const = 0;
  virtual void OnError(HTMLCanvasElement*, const String& error) {}
  virtual void OnError(OffscreenCanvas*, const String& error) {}

  DISALLOW_COPY_AND_ASSIGN(CanvasRenderingContextFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_FACTORY_H_
