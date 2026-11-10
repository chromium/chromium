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

#include <algorithm>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/convert_to_8bit_hash_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

constexpr auto kStar = MakeStaticAtomicStringImpl("*");
constexpr auto kXml = MakeStaticAtomicStringImpl("xml");
constexpr auto kXmlns = MakeStaticAtomicStringImpl("xmlns");
constexpr auto kXlink = MakeStaticAtomicStringImpl("xlink");
constexpr auto kHttp = MakeStaticAtomicStringImpl("http");
constexpr auto kHttps = MakeStaticAtomicStringImpl("https");

template <size_t N>
struct NewlineThenWhitespaceEntry {
  static constexpr StaticAsciiStringImpl<N> kImpl{
      typename StaticAsciiStringImpl<N>::NewlineThenWhitespaceTag{}};
};

template <size_t... Is>
consteval NewlineThenWhitespaceStringsTable::TableType
MakeNewlineThenWhitespaceTable(std::index_sequence<Is...>) {
  return {String(),
          String(NewlineThenWhitespaceEntry<Is + 1>::kImpl.AsStringImpl(),
                 String::StaticImplTag{})...};
}

}  // namespace

[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_null_atom;
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_empty_atom(
    &StringImpl::g_global_empty,
    AtomicString::StaticAtomTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_star_atom(
    kStar.AsStringImpl(),
    AtomicString::StaticAtomTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_xml_atom(
    kXml.AsStringImpl(),
    AtomicString::StaticAtomTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_xmlns_atom(
    kXmlns.AsStringImpl(),
    AtomicString::StaticAtomTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_xlink_atom(
    kXlink.AsStringImpl(),
    AtomicString::StaticAtomTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_http_atom(
    kHttp.AsStringImpl(),
    AtomicString::StaticAtomTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const AtomicString g_https_atom(
    kHttps.AsStringImpl(),
    AtomicString::StaticAtomTag{});

// This is not an AtomicString because it is unlikely to be used as an
// event/element/attribute name, so it shouldn't pollute the AtomicString hash
// table.
[[clang::no_destroy]] WTF_EXPORT constinit const String g_xmlns_with_colon =
    String::FromLiteral<"xmlns:">();

[[clang::no_destroy]] WTF_EXPORT constinit const String g_empty_string(
    &StringImpl::g_global_empty,
    String::StaticImplTag{});
[[clang::no_destroy]] WTF_EXPORT constinit const String g_empty_string16_bit(
    &StringImpl::g_global_empty16_bit,
    String::StaticImplTag{});

[[clang::
      no_destroy]] WTF_EXPORT constinit const NewlineThenWhitespaceStringsTable::
    TableType NewlineThenWhitespaceStringsTable::g_table_ =
        MakeNewlineThenWhitespaceTable(
            std::make_index_sequence<kTableSize - 1>());

void NewlineThenWhitespaceStringsTable::Init() {
  for (size_t i = 1; i < kTableSize; ++i) {
    StringImpl::CreateStatic(g_table_[i].Impl());
  }
}

bool NewlineThenWhitespaceStringsTable::IsNewlineThenWhitespaces(
    const StringView& view) {
  if (view.empty()) {
    return false;
  }
  // SAFETY: `view` tested for emptyness above.
  if (UNSAFE_BUFFERS(view[0]) != '\n') {
    return false;
  }
  if (view.Is8Bit()) {
    return std::ranges::all_of(view.Span8().subspan(1u),
                               [](LChar ch) { return ch == ' '; });
  }
  return std::ranges::all_of(view.Span16().subspan(1u),
                             [](UChar ch) { return ch == ' '; });
}

WTF_EXPORT uint32_t HashWideString24(base::span<const UChar> utf16_chars) {
  base::span<const uint8_t> bytes = base::as_bytes(utf16_chars);
  if (ContainsOnlyLatin1(utf16_chars)) {
    return HashString24<ConvertTo8BitHashReader>(bytes);
  } else {
    return HashString24(bytes);
  }
}

NOINLINE uint32_t StringImpl::HashSlowCase() const {
  if (Is8Bit()) {
    // This is the common case, so we take the size penalty
    // of the inlining here.
    SetHash(HashString24Inline(Span8()));
  } else {
    SetHash(HashWideString24(Span16()));
  }
  return ExistingHash();
}

void AtomicString::Init() {
  DCHECK(IsMainThread());
}

void InitStringStatics() {
  DCHECK(IsMainThread());

  StringImpl::InitStatics();
  constexpr wtf_size_t kStringStaticsCount =
      6 + (NewlineThenWhitespaceStringsTable::kTableSize - 1);
  StringImpl::ReserveStaticStringsCapacityForSize(kStringStaticsCount);

  StringImpl::CreateStatic(kStar.AsStringImpl());
  StringImpl::CreateStatic(kXml.AsStringImpl());
  StringImpl::CreateStatic(kXmlns.AsStringImpl());
  StringImpl::CreateStatic(kXlink.AsStringImpl());
  StringImpl::CreateStatic(kHttp.AsStringImpl());
  StringImpl::CreateStatic(kHttps.AsStringImpl());

  NewlineThenWhitespaceStringsTable::Init();
}

}  // namespace blink
