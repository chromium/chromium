// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_

#include "third_party/blink/renderer/platform/geometry/int_size.h"

namespace blink {

class Element;
class Document;
class LayoutObject;

namespace media_element_parser_helpers {

// Parses the intrinsicSize attribute of HTMLImageElement, HTMLVideoElement, and
// SVGImageElement. Returns true if the value is updated.
// https://github.com/ojanvafai/intrinsicsize-attribute/blob/master/README.md
bool ParseIntrinsicSizeAttribute(const String& value,
                                 const Element* element,
                                 IntSize* intrinsic_size,
                                 bool* is_default_intrinsic_size,
                                 String* message);

// Returns true for elements that are either <img>, <svg:image> or <video> that
// are not in an image or media document; returns false otherwise.
bool IsMediaElement(const Element* element);

// Returns if the document is allowed to use
// FeaturePolicyFeature::kUnsizedMedia.
bool IsUnsizedMediaEnabled(const Document& document);

void ReportUnsizedMediaViolation(const LayoutObject* layout_object);

}  // namespace media_element_parser_helpers

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
