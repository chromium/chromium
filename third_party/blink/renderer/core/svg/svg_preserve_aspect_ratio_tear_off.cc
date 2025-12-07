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

#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio_tear_off.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

void SVGPreserveAspectRatioTearOff::setAlign(uint16_t align,
                                             ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  if (align == kSvgPreserveaspectratioUnknown ||
      align > kSvgPreserveaspectratioXmaxymax) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The alignment provided is invalid.");
    return;
  }
  Target()->SetAlign(
      static_cast<SVGPreserveAspectRatio::SVGPreserveAspectRatioType>(align));
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

void SVGPreserveAspectRatioTearOff::setMeetOrSlice(
    uint16_t meet_or_slice,
    ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  if (meet_or_slice == kSvgMeetorsliceUnknown ||
      meet_or_slice > kSvgMeetorsliceSlice) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The meetOrSlice provided is invalid.");
    return;
  }
  Target()->SetMeetOrSlice(
      static_cast<SVGPreserveAspectRatio::SVGMeetOrSliceType>(meet_or_slice));
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

SVGPreserveAspectRatioTearOff::SVGPreserveAspectRatioTearOff(
    SVGPreserveAspectRatio* target,
    SVGAnimatedPropertyBase* binding,
    PropertyIsAnimValType property_is_anim_val)
    : SVGPropertyTearOff<SVGPreserveAspectRatio>(target,
                                                 binding,
                                                 property_is_anim_val) {}

}  // namespace blink
