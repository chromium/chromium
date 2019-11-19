/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/svg_static_string_list.h"

#include "third_party/blink/renderer/core/svg/svg_string_list_tear_off.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGStaticStringList::SVGStaticStringList(SVGElement* context_element,
                                         const QualifiedName& attribute_name,
                                         SVGStringListBase* initial_value)
    : SVGAnimatedPropertyBase(kAnimatedUnknown,
                              context_element,
                              attribute_name),
      value_(initial_value) {
  DCHECK(context_element);
}

SVGStaticStringList::~SVGStaticStringList() = default;

void SVGStaticStringList::Trace(blink::Visitor* visitor) {
  visitor->Trace(value_);
  visitor->Trace(tear_off_);
  SVGAnimatedPropertyBase::Trace(visitor);
}

SVGPropertyBase* SVGStaticStringList::CurrentValueBase() {
  return value_.Get();
}

const SVGPropertyBase& SVGStaticStringList::BaseValueBase() const {
  NOTREACHED();
  return *value_;
}

bool SVGStaticStringList::IsAnimating() const {
  return false;
}

SVGPropertyBase* SVGStaticStringList::CreateAnimatedValue() {
  NOTREACHED();
  return nullptr;
}

void SVGStaticStringList::SetAnimatedValue(SVGPropertyBase*) {
  NOTREACHED();
}

void SVGStaticStringList::AnimationEnded() {
  NOTREACHED();
}

SVGStringListTearOff* SVGStaticStringList::TearOff() {
  if (!tear_off_) {
    tear_off_ = MakeGarbageCollected<SVGStringListTearOff>(
        value_, this, kPropertyIsNotAnimVal);
  }
  return tear_off_.Get();
}

SVGParsingError SVGStaticStringList::AttributeChanged(const String& value) {
  ClearBaseValueNeedsSynchronization();
  return value_->SetValueAsString(value);
}

}  // namespace blink
