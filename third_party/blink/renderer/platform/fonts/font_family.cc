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

#include "third_party/blink/renderer/platform/fonts/font_family.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

bool operator==(const FontFamily& a, const FontFamily& b) {
  if (a.Family() != b.Family())
    return false;
  const FontFamily* ap;
  const FontFamily* bp;
  for (ap = a.Next(), bp = b.Next(); ap != bp;
       ap = ap->Next(), bp = bp->Next()) {
    if (!ap || !bp)
      return false;
    if (ap->Family() != bp->Family())
      return false;
  }
  return true;
}

void FontFamily::AppendFamily(AtomicString family) {
  scoped_refptr<SharedFontFamily> appended_family = SharedFontFamily::Create();
  appended_family->SetFamily(family);
  AppendFamily(appended_family);
}

String FontFamily::ToString() const {
  StringBuilder builder;
  builder.Append(family_);
  const FontFamily* current = Next();
  while (current) {
    builder.Append(",");
    builder.Append(current->Family());
    current = current->Next();
  }
  return builder.ToString();
}

}  // namespace blink
