// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_GRAPHICS_LAYER_TREE_AS_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_GRAPHICS_LAYER_TREE_AS_TEXT_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"

namespace blink {

class GraphicsLayer;
class JSONObject;

std::unique_ptr<JSONObject> GraphicsLayerTreeAsJSON(const GraphicsLayer*,
                                                    LayerTreeFlags);

String CORE_EXPORT GraphicsLayerTreeAsTextForTesting(const GraphicsLayer*,
                                                     LayerTreeFlags);

#if DCHECK_IS_ON()
void CORE_EXPORT VerboseLogGraphicsLayerTree(const GraphicsLayer*);
#endif
}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void CORE_EXPORT showGraphicsLayerTree(const blink::GraphicsLayer*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_GRAPHICS_LAYER_TREE_AS_TEXT_H_
