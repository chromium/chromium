// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_

namespace blink {

class LayoutObject;

namespace media_element_parser_helpers {

// When |layout_object| is not properly styled (according to
// PermissionsPolicyFeature::kUnsizedMedia) this invocation counts a potential
// violation. If |send_report| is set, then an actual violation report is
// generated.
void CheckUnsizedMediaViolation(const LayoutObject* layout_object,
                                bool send_report);

}  // namespace media_element_parser_helpers

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
