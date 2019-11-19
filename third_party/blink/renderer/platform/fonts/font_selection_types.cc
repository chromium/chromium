/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"

#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace {

class IntegerHasher {
  STACK_ALLOCATED();

 public:
  void add(unsigned integer) {
    m_underlyingHasher.AddCharactersAssumingAligned(integer, integer >> 16);
  }

  unsigned hash() const { return m_underlyingHasher.GetHash(); }

 private:
  StringHasher m_underlyingHasher;
};

}  // namespace

namespace blink {

unsigned FontSelectionRequest::GetHash() const {
  IntegerHasher hasher;
  hasher.add(weight.RawValue());
  hasher.add(width.RawValue());
  hasher.add(slope.RawValue());
  return hasher.hash();
}

unsigned FontSelectionRequestKeyHash::GetHash(
    const FontSelectionRequestKey& key) {
  IntegerHasher hasher;
  hasher.add(key.request.GetHash());
  hasher.add(key.isDeletedValue);
  return hasher.hash();
}

unsigned FontSelectionCapabilitiesHash::GetHash(
    const FontSelectionCapabilities& key) {
  IntegerHasher hasher;
  hasher.add(key.width.UniqueValue());
  hasher.add(key.slope.UniqueValue());
  hasher.add(key.weight.UniqueValue());
  hasher.add(key.IsHashTableDeletedValue());
  return hasher.hash();
}

String FontSelectionValue::ToString() const {
  return String::Format("%f", (float)*this);
}

String FontSelectionRequest::ToString() const {
  return String::Format(
      "weight=%s, width=%s, slope=%s", weight.ToString().Ascii().c_str(),
      width.ToString().Ascii().data(), slope.ToString().Ascii().c_str());
}

}  // namespace blink
