// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_INPUT_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_INPUT_HELPERS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class PropertyHandle;
class TimingFunction;
class QualifiedName;

class CORE_EXPORT AnimationInputHelpers {
  STATIC_ONLY(AnimationInputHelpers);

 public:
  static CSSPropertyID KeyframeAttributeToCSSProperty(const String&,
                                                      const Document&);
  static CSSPropertyID KeyframeAttributeToPresentationAttribute(const String&,
                                                                const Element*);
  static const QualifiedName* KeyframeAttributeToSVGAttribute(const String&,
                                                              Element*);
  static scoped_refptr<TimingFunction> ParseTimingFunction(const String&,
                                                           Document*,
                                                           ExceptionState&);

  static String PropertyHandleToKeyframeAttribute(PropertyHandle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_INPUT_HELPERS_H_
