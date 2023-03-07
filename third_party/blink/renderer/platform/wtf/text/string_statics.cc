/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/string_statics.h"

#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_null_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_empty_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_star_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_xml_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_xmlns_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_xlink_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_http_atom);
WTF_EXPORT DEFINE_GLOBAL(AtomicString, g_https_atom);

// This is not an AtomicString because it is unlikely to be used as an
// event/element/attribute name, so it shouldn't pollute the AtomicString hash
// table.
WTF_EXPORT DEFINE_GLOBAL(String, g_xmlns_with_colon);

WTF_EXPORT DEFINE_GLOBAL(String, g_empty_string);
WTF_EXPORT DEFINE_GLOBAL(String, g_empty_string16_bit);

namespace {
std::aligned_storage_t<sizeof(String) *
                           NewlineThenWhitespaceStringsTable::kTableSize,
                       alignof(String)>
    g_canonical_whitespace_table_storage;
}

WTF_EXPORT const String (&NewlineThenWhitespaceStringsTable::g_table_)
    [NewlineThenWhitespaceStringsTable::kTableSize] = *reinterpret_cast<
        String (*)[NewlineThenWhitespaceStringsTable::kTableSize]>(
        &g_canonical_whitespace_table_storage);

NOINLINE unsigned StringImpl::HashSlowCase() const {
  if (Is8Bit())
    SetHash(StringHasher::ComputeHashAndMaskTop8Bits(Characters8(), length_));
  else
    SetHash(StringHasher::ComputeHashAndMaskTop8Bits(Characters16(), length_));
  return ExistingHash();
}

void AtomicString::Init() {
  DCHECK(IsMainThread());

  new (NotNullTag::kNotNull, (void*)&g_null_atom) AtomicString;
  new (NotNullTag::kNotNull, (void*)&g_empty_atom) AtomicString("");
}

template <unsigned charactersCount>
scoped_refptr<StringImpl> AddStaticASCIILiteral(
    const char (&characters)[charactersCount]) {
  unsigned length = charactersCount - 1;
  unsigned hash = StringHasher::ComputeHashAndMaskTop8Bits(
      reinterpret_cast<const LChar*>(characters), length);
  return base::AdoptRef(StringImpl::CreateStatic(characters, length, hash));
}

void NewlineThenWhitespaceStringsTable::Init() {
  LChar whitespace_buffer[kTableSize + 1] = {'\n'};
  std::fill(std::next(std::begin(whitespace_buffer), 1),
            std::end(whitespace_buffer), ' ');

  // Keep g_table_[0] uninitialized.
  for (size_t length = 1; length < kTableSize; ++length) {
    const unsigned hash =
        StringHasher::ComputeHashAndMaskTop8Bits(whitespace_buffer, length);
    auto* string_impl = StringImpl::CreateStatic(
        reinterpret_cast<const char*>(whitespace_buffer), length, hash);
    new (NotNullTag::kNotNull, (void*)(&g_table_[length]))
        String(AtomicString(string_impl).GetString());
  }
}

bool NewlineThenWhitespaceStringsTable::IsNewlineThenWhitespaces(
    const StringView& view) {
  if (view.empty()) {
    return false;
  }
  if (view[0] != '\n') {
    return false;
  }
  if (view.Is8Bit()) {
    return std::all_of(view.Characters8() + 1,
                       view.Characters8() + view.length(),
                       [](LChar ch) { return ch == ' '; });
  }
  return std::all_of(view.Characters16() + 1,
                     view.Characters16() + view.length(),
                     [](UChar ch) { return ch == ' '; });
}

void StringStatics::Init() {
  DCHECK(IsMainThread());

  StringImpl::InitStatics();
  new (NotNullTag::kNotNull, (void*)&g_empty_string) String(StringImpl::empty_);
  new (NotNullTag::kNotNull, (void*)&g_empty_string16_bit)
      String(StringImpl::empty16_bit_);

  // FIXME: These should be allocated at compile time.
  new (NotNullTag::kNotNull, (void*)&g_star_atom) AtomicString("*");
  new (NotNullTag::kNotNull, (void*)&g_xml_atom)
      AtomicString(AddStaticASCIILiteral("xml"));
  new (NotNullTag::kNotNull, (void*)&g_xmlns_atom)
      AtomicString(AddStaticASCIILiteral("xmlns"));
  new (NotNullTag::kNotNull, (void*)&g_xlink_atom)
      AtomicString(AddStaticASCIILiteral("xlink"));
  new (NotNullTag::kNotNull, (void*)&g_xmlns_with_colon) String("xmlns:");
  new (NotNullTag::kNotNull, (void*)&g_http_atom)
      AtomicString(AddStaticASCIILiteral("http"));
  new (NotNullTag::kNotNull, (void*)&g_https_atom)
      AtomicString(AddStaticASCIILiteral("https"));

  NewlineThenWhitespaceStringsTable::Init();
}

}  // namespace WTF
