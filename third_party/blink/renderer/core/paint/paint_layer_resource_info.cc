/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/paint_layer_resource_info.h"

#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

PaintLayerResourceInfo::PaintLayerResourceInfo(PaintLayer* layer)
    : layer_(layer) {}

PaintLayerResourceInfo::~PaintLayerResourceInfo() {
  DCHECK(!layer_);
}

void PaintLayerResourceInfo::ResourceContentChanged(SVGResource*) {
  DCHECK(layer_);
  LayoutObject& layout_object = layer_->GetLayoutObject();
  layout_object.SetShouldDoFullPaintInvalidation();
  layer_->SetNeedsCompositingInputsUpdate();
  // The effect paint property nodes depend on SVG filters so we need
  // to update these properties when filter resources change.
  layout_object.SetNeedsPaintPropertyUpdate();
  layer_->SetFilterOnEffectNodeDirty();
  layer_->SetBackdropFilterOnEffectNodeDirty();
}

}  // namespace blink
