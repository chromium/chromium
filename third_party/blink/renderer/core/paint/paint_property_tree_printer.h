// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_PRINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_PRINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if DCHECK_IS_ON()

namespace blink {

class LocalFrameView;
class LayoutObject;
class ObjectPaintProperties;
class VisualViewport;

namespace paint_property_tree_printer {

void UpdateDebugNames(const VisualViewport&);
void UpdateDebugNames(const LayoutObject&, ObjectPaintProperties&);

}  // namespace paint_property_tree_printer

}  // namespace blink

// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showAllPropertyTrees(const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showTransformPropertyTree(
    const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showClipPropertyTree(const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showEffectPropertyTree(const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showScrollPropertyTree(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
transformPropertyTreeAsString(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
clipPropertyTreeAsString(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
effectPropertyTreeAsString(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
scrollPropertyTreeAsString(const blink::LocalFrameView& rootFrame);

#endif  // if DCHECK_IS_ON()

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_PRINTER_H_
