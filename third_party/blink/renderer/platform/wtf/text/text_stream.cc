/*
 * Copyright (C) 2004, 2008, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

// large enough for any integer or floating point value in string format,
// including trailing null character
static const size_t kPrintBufferSize = 100;

static inline bool HasFractions(double val) {
  // We use 0.011 to more than match the number of significant digits we print
  // out when dumping the render tree.
  static const double kEpsilon = 0.011;
  int ival = static_cast<int>(round(val));
  double dval = static_cast<double>(ival);
  return fabs(val - dval) > kEpsilon;
}

TextStream& TextStream::operator<<(bool b) {
  return *this << (b ? "1" : "0");
}

TextStream& TextStream::operator<<(int16_t i) {
  text_.AppendNumber(i);
  return *this;
}

TextStream& TextStream::operator<<(uint16_t i) {
  text_.AppendNumber(i);
  return *this;
}

TextStream& TextStream::operator<<(int32_t i) {
  text_.AppendNumber(i);
  return *this;
}

TextStream& TextStream::operator<<(uint32_t i) {
  text_.AppendNumber(i);
  return *this;
}

TextStream& TextStream::operator<<(int64_t i) {
  text_.AppendNumber(i);
  return *this;
}

TextStream& TextStream::operator<<(uint64_t i) {
  text_.AppendNumber(i);
  return *this;
}

TextStream& TextStream::operator<<(float f) {
  text_.Append(String::NumberToStringFixedWidth(f, 2));
  return *this;
}

TextStream& TextStream::operator<<(double d) {
  text_.Append(String::NumberToStringFixedWidth(d, 2));
  return *this;
}

TextStream& TextStream::operator<<(const char* string) {
  text_.Append(string);
  return *this;
}

TextStream& TextStream::operator<<(const void* p) {
  char buffer[kPrintBufferSize];
  snprintf(buffer, sizeof(buffer) - 1, "%p", p);
  return *this << buffer;
}

TextStream& TextStream::operator<<(const String& string) {
  text_.Append(string);
  return *this;
}

TextStream& TextStream::operator<<(
    const FormatNumberRespectingIntegers& number_to_format) {
  if (HasFractions(number_to_format.value))
    return *this << number_to_format.value;

  text_.AppendNumber(static_cast<int>(round(number_to_format.value)));
  return *this;
}

String TextStream::Release() {
  String result = text_.ToString();
  text_.Clear();
  return result;
}

void WriteIndent(TextStream& ts, int indent) {
  for (int i = 0; i != indent; ++i)
    ts << "  ";
}

}  // namespace WTF
