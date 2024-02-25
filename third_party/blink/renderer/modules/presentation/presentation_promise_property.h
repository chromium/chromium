// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_PROMISE_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_PROMISE_PROPERTY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

class PresentationAvailability;

using PresentationAvailabilityProperty =
    ScriptPromiseProperty<PresentationAvailability, DOMException>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_PROMISE_PROPERTY_H_
