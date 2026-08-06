// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_GEOMETRY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_GEOMETRY_UTILS_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_css_box_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class BoxQuadOptions;
class CSSPseudoElement;
class ConvertCoordinateOptions;
class DOMPoint;
class DOMPointInit;
class DOMQuad;
class DOMQuadInit;
class DOMRectReadOnly;
class ExceptionState;
class LayoutObject;
class Node;
class V8UnionCSSPseudoElementOrDocumentOrElementOrText;

// Static utility functions for GeometryUtils mixin implementation.
// These functions can be reused by CSSPseudoElement, Element, Text, and
// Document to implement the GeometryUtils interface.
// See: https://drafts.csswg.org/cssom-view-1/#the-geometryutils-interface
namespace geometry_utils {

// Gets the LayoutObject from a GeometryNode union type.
// Returns nullptr if the node doesn't have a layout object.
CORE_EXPORT LayoutObject* GetLayoutObjectFromGeometryNode(
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* node);

// Entry point helpers for GeometryUtils mixin methods that validate origins and
// update layout before delegating to the core coordinate conversion functions.
CORE_EXPORT HeapVector<Member<DOMQuad>> GetBoxQuads(
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const BoxQuadOptions* options,
    ExceptionState& exception_state);

CORE_EXPORT DOMQuad* ConvertQuadFromNode(
    const DOMQuadInit* quad,
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options,
    ExceptionState& exception_state);

CORE_EXPORT DOMQuad* ConvertRectFromNode(
    DOMRectReadOnly* rect,
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options,
    ExceptionState& exception_state);

CORE_EXPORT DOMPoint* ConvertPointFromNode(
    const DOMPointInit* point,
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options,
    ExceptionState& exception_state);

// Computes box quads for a layout object.
// |layout_object| - the layout object to get quads for
// |box_type| - the CSSBoxType to use
// |relative_to| - the relative layout object to offset the quads
// Returns a vector of DOMQuad objects.
CORE_EXPORT HeapVector<Member<DOMQuad>> GetBoxQuads(LayoutObject* layout_object,
                                                    V8CSSBoxType::Enum box_type,
                                                    LayoutObject* relative_to);

// Converts a quad from source node coordinates to target layout object
// coordinates.
// |quad| - the input quad to convert
// |source_layout| - the source layout object
// |target_layout| - the target layout object
// Returns the converted DOMQuad, or nullptr on error.
CORE_EXPORT DOMQuad* ConvertQuadFromNode(DOMQuad* quad,
                                         LayoutObject* source_layout,
                                         LayoutObject* target_layout,
                                         V8CSSBoxType::Enum from_box,
                                         V8CSSBoxType::Enum to_box);

// Converts a rect from source node coordinates to target layout object
// coordinates.
// |rect| - the input rect to convert
// |source_layout| - the source layout object
// |target_layout| - the target layout object
// Returns the converted DOMQuad, or nullptr on error.
CORE_EXPORT DOMQuad* ConvertRectFromNode(DOMRectReadOnly* rect,
                                         LayoutObject* source_layout,
                                         LayoutObject* target_layout,
                                         V8CSSBoxType::Enum from_box,
                                         V8CSSBoxType::Enum to_box);

// Converts a point from source node coordinates to target layout object
// coordinates.
// |point| - the input point to convert
// |source_layout| - the source layout object
// |target_layout| - the target layout object
// Returns the converted DOMPoint, or nullptr on error.
CORE_EXPORT DOMPoint* ConvertPointFromNode(DOMPoint* point,
                                           LayoutObject* source_layout,
                                           LayoutObject* target_layout,
                                           V8CSSBoxType::Enum from_box,
                                           V8CSSBoxType::Enum to_box);

}  // namespace geometry_utils

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_GEOMETRY_UTILS_H_
