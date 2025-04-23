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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_statics.h"

#include <algorithm>

#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/convert_to_8bit_hash_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_null_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_empty_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_star_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_xml_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_xmlns_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_xlink_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_http_atom);
DEFINE_GLOBAL(WTF_EXPORT, AtomicString, g_https_atom);

// This is not an AtomicString because it is unlikely to be used as an
// event/element/attribute name, so it shouldn't pollute the AtomicString hash
// table.
DEFINE_GLOBAL(WTF_EXPORT, String, g_xmlns_with_colon);

DEFINE_GLOBAL(WTF_EXPORT, String, g_empty_string);
DEFINE_GLOBAL(WTF_EXPORT, String, g_empty_string16_bit);

namespace {
alignas(String) char g_canonical_whitespace_table_storage[sizeof(
    NewlineThenWhitespaceStringsTable::TableType)];
}

WTF_EXPORT unsigned ComputeHashForWideString(const UChar* str,
                                             unsigned length) {
  bool is_all_latin1 = true;
  for (unsigned i = 0; i < length; ++i) {
    if (str[i] & 0xff00) {
      is_all_latin1 = false;
      break;
    }
  }
  if (is_all_latin1) {
    return StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
        (char*)str, length);
  } else {
    return StringHasher::ComputeHashAndMaskTop8Bits((char*)str, length * 2);
  }
}

WTF_EXPORT const NewlineThenWhitespaceStringsTable::TableType&
    NewlineThenWhitespaceStringsTable::g_table_ =
        *reinterpret_cast<NewlineThenWhitespaceStringsTable::TableType*>(
            g_canonical_whitespace_table_storage);

NOINLINE unsigned StringImpl::HashSlowCase() const {
  if (Is8Bit()) {
    // This is the common case, so we take the size penalty
    // of the inlining here.
    SetHash(StringHasher::ComputeHashAndMaskTop8BitsInline((char*)Characters8(),
                                                           length_));
  } else {
    SetHash(ComputeHashForWideString(Characters16(), length_));
  }
  return ExistingHash();
}

void AtomicString::Init() {
  DCHECK(IsMainThread());

  new (NotNullTag::kNotNull, (void*)&g_null_atom) AtomicString;
  new (NotNullTag::kNotNull, (void*)&g_empty_atom) AtomicString("");
}

scoped_refptr<StringImpl> AddStaticASCIILiteral(
    base::span<const char> literal) {
  return base::AdoptRef(StringImpl::CreateStatic(literal));
}

void NewlineThenWhitespaceStringsTable::Init() {
  char whitespace_buffer[kTableSize + 1] = {'\n'};
  std::ranges::fill(base::span(whitespace_buffer).subspan<1u>(), ' ');

  // Keep g_table_[0] uninitialized.
  for (size_t length = 1; length < kTableSize; ++length) {
    auto* string_impl =
        StringImpl::CreateStatic(base::span(whitespace_buffer).first(length));
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
    return std::ranges::all_of(view.Span8().subspan(1u),
                               [](LChar ch) { return ch == ' '; });
  }
  return std::ranges::all_of(view.Span16().subspan(1u),
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
      AtomicString(AddStaticASCIILiteral(base::span_from_cstring("xml")));
  new (NotNullTag::kNotNull, (void*)&g_xmlns_atom)
      AtomicString(AddStaticASCIILiteral(base::span_from_cstring("xmlns")));
  new (NotNullTag::kNotNull, (void*)&g_xlink_atom)
      AtomicString(AddStaticASCIILiteral(base::span_from_cstring("xlink")));
  new (NotNullTag::kNotNull, (void*)&g_xmlns_with_colon) String("xmlns:");
  new (NotNullTag::kNotNull, (void*)&g_http_atom)
      AtomicString(AddStaticASCIILiteral(base::span_from_cstring("http")));
  new (NotNullTag::kNotNull, (void*)&g_https_atom)
      AtomicString(AddStaticASCIILiteral(base::span_from_cstring("https")));

  NewlineThenWhitespaceStringsTable::Init();
}

}  // namespace WTF
