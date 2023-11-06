// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope_data.h"

#include "third_party/blink/renderer/core/css/style_scope.h"

namespace blink {

void StyleScopeData::AddTriggeredImplicitScope(const StyleScope& style_scope) {
  if (!triggered_implicit_scopes_.Contains(&style_scope)) {
    triggered_implicit_scopes_.push_back(&style_scope);
  }
}

void StyleScopeData::RemoveTriggeredImplicitScope(
    const StyleScope& style_scope) {
  WTF::Erase(triggered_implicit_scopes_, &style_scope);
}

bool StyleScopeData::TriggersScope(const StyleScope& style_scope) const {
  return triggered_implicit_scopes_.Contains(&style_scope);
}

void StyleScopeData::Trace(Visitor* visitor) const {
  visitor->Trace(triggered_implicit_scopes_);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
