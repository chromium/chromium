// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_

#include "third_party/blink/renderer/platform/geometry/int_size.h"

namespace blink {

class Element;
class LayoutObject;

namespace media_element_parser_helpers {

// Returns true for elements that are either <img>, <svg:image> or <video> that
// are not in an image or media document; returns false otherwise.
bool IsMediaElement(const Element* element);

// When |layout_object| is not properly styled (according to
// FeaturePolicyFeature::kUnsizedMedia) this invocation counts a potential
// violation. If |send_report| is set, then an actual violation report is
// generated.
void ReportUnsizedMediaViolation(const LayoutObject* layout_object,
                                 bool send_report);

}  // namespace media_element_parser_helpers

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
