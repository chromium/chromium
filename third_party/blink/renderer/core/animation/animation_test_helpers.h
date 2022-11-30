// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TEST_HELPERS_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class Element;
class KeyframeEffect;
class KeyframeEffectModelBase;

namespace animation_test_helpers {

void SetV8ObjectPropertyAsString(v8::Isolate*,
                                 v8::Local<v8::Object>,
                                 const StringView& name,
                                 const StringView& value);
void SetV8ObjectPropertyAsNumber(v8::Isolate*,
                                 v8::Local<v8::Object>,
                                 const StringView& name,
                                 double value);

// Creates a KeyframeEffect with two keyframes corresponding to
// value_start (offset 0.0) and value_end (offset 1.0). Default blink::Timing
// values are used, except for iteration_duration which is set to 1000ms.
KeyframeEffect* CreateSimpleKeyframeEffectForTest(Element*,
                                                  CSSPropertyID,
                                                  String value_start,
                                                  String value_end);
KeyframeEffectModelBase* CreateSimpleKeyframeEffectModelForTest(
    CSSPropertyID,
    String value_start,
    String value_end);

// Ensures that a set of interpolations actually computes and caches their
// internal interpolated value, so that tests can retrieve them.
//
// All members of the ActiveInterpolations must be instances of
// InvalidatableInterpolation.
void EnsureInterpolatedValueCached(ActiveInterpolations*, Document&, Element*);

}  // namespace animation_test_helpers
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TEST_HELPERS_H_
