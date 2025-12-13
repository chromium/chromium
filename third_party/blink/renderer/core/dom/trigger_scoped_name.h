// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRIGGER_SCOPED_NAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRIGGER_SCOPED_NAME_H_

#include "third_party/blink/renderer/core/layout/naming_scope.h"

namespace blink {

class AnimationTrigger;
class Element;
class ScopedCSSName;

// A name scoped according to the 'trigger-scope' property.
//
// https://drafts.csswg.org/css-animations-2/#trigger-scope
using TriggerScopedName = NamingScope;

TriggerScopedName* ToTriggerScopedName(const ScopedCSSName&, const Element&);

using TriggerScopedNameMap =
    GCedHeapHashMap<Member<const TriggerScopedName>, Member<AnimationTrigger>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRIGGER_SCOPED_NAME_H_
