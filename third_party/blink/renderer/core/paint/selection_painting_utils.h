// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SELECTION_PAINTING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SELECTION_PAINTING_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Color;
class Document;
class ComputedStyle;
class Node;
struct TextPaintStyle;
struct PaintInfo;

class CORE_EXPORT SelectionPaintingUtils {
  STATIC_ONLY(SelectionPaintingUtils);

 public:
  static Color SelectionBackgroundColor(const Document&,
                                        const ComputedStyle&,
                                        Node*);
  static Color SelectionForegroundColor(const Document&,
                                        const ComputedStyle&,
                                        Node*,
                                        const GlobalPaintFlags);
  static Color SelectionEmphasisMarkColor(const Document&,
                                          const ComputedStyle&,
                                          Node*,
                                          const GlobalPaintFlags);
  static TextPaintStyle SelectionPaintingStyle(const Document&,
                                               const ComputedStyle&,
                                               Node*,
                                               bool have_selection,
                                               const TextPaintStyle& text_style,
                                               const PaintInfo&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SELECTION_PAINTING_UTILS_H_
