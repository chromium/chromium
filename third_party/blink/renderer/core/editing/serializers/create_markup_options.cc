// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/serializers/create_markup_options.h"

namespace blink {

CreateMarkupOptions::Builder&
CreateMarkupOptions::Builder::SetConstrainingAncestor(const Node* node) {
  data_.constraining_ancestor_ = node;
  return *this;
}

CreateMarkupOptions::Builder&
CreateMarkupOptions::Builder::SetShouldResolveURLs(
    AbsoluteURLs should_resolve_urls) {
  data_.should_resolve_urls_ = should_resolve_urls;
  return *this;
}

CreateMarkupOptions::Builder&
CreateMarkupOptions::Builder::SetShouldAnnotateForInterchange(
    bool annotate_for_interchange) {
  data_.should_annotate_for_interchange_ = annotate_for_interchange;
  return *this;
}

CreateMarkupOptions::Builder&
CreateMarkupOptions::Builder::SetShouldConvertBlocksToInlines(
    bool convert_blocks_to_inlines) {
  data_.should_convert_blocks_to_inlines_ = convert_blocks_to_inlines;
  return *this;
}

CreateMarkupOptions::Builder&
CreateMarkupOptions::Builder::SetIsForMarkupSanitization(
    bool is_for_sanitization) {
  data_.is_for_markup_sanitization_ = is_for_sanitization;
  return *this;
}

CreateMarkupOptions::Builder&
CreateMarkupOptions::Builder::SetIgnoresCSSTextTransformsForRenderedText(
    bool ignores_text_transforms) {
  data_.ignores_css_text_transforms_for_rendered_text = ignores_text_transforms;
  return *this;
}

}  // namespace blink
