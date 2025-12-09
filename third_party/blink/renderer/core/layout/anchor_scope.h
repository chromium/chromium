// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCOPE_H_

#include "third_party/blink/renderer/core/layout/naming_scope.h"

namespace blink {

class LayoutObject;
class ScopedCSSName;

// A name scoped according to the 'anchor-scope' property.
//
// https://drafts.csswg.org/css-anchor-position-1/#anchor-scope
using AnchorScopedName = NamingScope;

AnchorScopedName* ToAnchorScopedName(const ScopedCSSName&, const LayoutObject&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCOPE_H_
