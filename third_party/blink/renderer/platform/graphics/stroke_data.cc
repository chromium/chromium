// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "third_party/blink/renderer/platform/graphics/stroke_data.h"

#include <memory>

namespace blink {

void StrokeData::SetLineDash(const DashArray& dashes, float dash_offset) {
  wtf_size_t dash_length = dashes.size();
  if (!dash_length) {
    dash_.reset();
    return;
  }

  wtf_size_t count = !(dash_length % 2) ? dash_length : dash_length * 2;
  auto intervals = std::make_unique<SkScalar[]>(count);

  for (wtf_size_t i = 0; i < count; i++)
    intervals[i] = dashes[i % dash_length];

  dash_ = cc::PathEffect::MakeDash(intervals.get(), count, dash_offset);
}

void StrokeData::SetDashEffect(sk_sp<cc::PathEffect> dash_effect) {
  dash_ = std::move(dash_effect);
}

void StrokeData::SetupPaint(cc::PaintFlags* flags) const {
  flags->setStyle(cc::PaintFlags::kStroke_Style);
  flags->setStrokeWidth(SkFloatToScalar(thickness_));
  flags->setStrokeCap(line_cap_);
  flags->setStrokeJoin(line_join_);
  flags->setStrokeMiter(SkFloatToScalar(miter_limit_));
  flags->setPathEffect(dash_);
}

}  // namespace blink
