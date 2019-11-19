/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_TEXT_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_TEXT_STREAM_H_

#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

class WTF_EXPORT TextStream final {
  STACK_ALLOCATED();

 public:
  struct FormatNumberRespectingIntegers {
    FormatNumberRespectingIntegers(double number) : value(number) {}
    double value;
  };

  TextStream& operator<<(bool);
  TextStream& operator<<(int16_t);
  TextStream& operator<<(uint16_t);
  TextStream& operator<<(int32_t);
  TextStream& operator<<(uint32_t);
  TextStream& operator<<(int64_t);
  TextStream& operator<<(uint64_t);
  TextStream& operator<<(float);
  TextStream& operator<<(double);
  TextStream& operator<<(const char*);
  TextStream& operator<<(const void*);
  TextStream& operator<<(const String&);
  TextStream& operator<<(const FormatNumberRespectingIntegers&);

  String Release();

 private:
  StringBuilder text_;
};

WTF_EXPORT void WriteIndent(TextStream&, int indent);

template <typename Item>
TextStream& operator<<(TextStream& ts, const Vector<Item>& vector) {
  ts << "[";

  unsigned size = vector.size();
  for (unsigned i = 0; i < size; ++i) {
    ts << vector[i];
    if (i < size - 1)
      ts << ", ";
  }

  ts << "]";
  return ts;
}

}  // namespace WTF

#endif
