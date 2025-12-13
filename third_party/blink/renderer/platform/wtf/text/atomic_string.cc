/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {

ASSERT_SIZE(AtomicString, String);

AtomicString::AtomicString(base::span<const LChar> chars)
    : string_(AtomicStringTable::Instance().Add(chars)) {}

AtomicString::AtomicString(base::span<const UChar> chars,
                           AtomicStringUCharEncoding encoding)
    : string_(AtomicStringTable::Instance().Add(chars, encoding)) {}

AtomicString::AtomicString(const UChar* chars)
    : string_(AtomicStringTable::Instance().Add(
          // SAFETY: safe when `chars` points to a null-terminated cstring.
          UNSAFE_BUFFERS(
              {chars, chars ? LengthOfNullTerminatedString(chars) : 0}),
          AtomicStringUCharEncoding::kUnknown)) {}

AtomicString::AtomicString(const StringView& string_view)
    : string_(AtomicStringTable::Instance().Add(string_view)) {}

scoped_refptr<StringImpl> AtomicString::AddSlowCase(
    scoped_refptr<StringImpl>&& string) {
  DCHECK(!string->IsAtomic());
  return AtomicStringTable::Instance().Add(std::move(string));
}

scoped_refptr<StringImpl> AtomicString::AddSlowCase(StringImpl* string) {
  DCHECK(!string->IsAtomic());
  return AtomicStringTable::Instance().Add(string);
}

AtomicString AtomicString::FromUTF8(base::span<const uint8_t> bytes) {
  if (!bytes.data()) {
    return g_null_atom;
  }
  if (bytes.empty()) {
    return g_empty_atom;
  }
  return AtomicString(AtomicStringTable::Instance().AddUTF8(bytes));
}

AtomicString AtomicString::FromUTF8(const char* chars) {
  if (!chars)
    return g_null_atom;
  if (!*chars)
    return g_empty_atom;
  return AtomicString(AtomicStringTable::Instance().AddUTF8(
      base::as_byte_span(std::string_view(chars))));
}

AtomicString AtomicString::FromUTF8(std::string_view utf8_string) {
  return FromUTF8(base::as_byte_span(utf8_string));
}

AtomicString AtomicString::LowerASCII(AtomicString source) {
  if (source.IsLowerASCII()) [[likely]] {
    return source;
  }
  StringImpl* impl = source.Impl();
  // if impl is null, then IsLowerASCII() should have returned true.
  DCHECK(impl);
  scoped_refptr<StringImpl> new_impl = impl->LowerASCII();
  return AtomicString(String(std::move(new_impl)));
}

AtomicString AtomicString::LowerASCII() const {
  return AtomicString::LowerASCII(*this);
}

AtomicString AtomicString::UpperASCII() const {
  StringImpl* impl = Impl();
  if (!impl) [[unlikely]] {
    return *this;
  }
  return AtomicString(impl->UpperASCII());
}

AtomicString AtomicString::Number(double number, unsigned precision) {
  DoubleToStringConverter converter;
  return AtomicString(converter.ToStringWithFixedPrecision(number, precision));
}

std::ostream& operator<<(std::ostream& out, const AtomicString& s) {
  return out << s.GetString();
}

void AtomicString::WriteIntoTrace(perfetto::TracedValue context) const {
  perfetto::WriteIntoTracedValue(std::move(context), GetString());
}

#ifndef NDEBUG
void AtomicString::Show() const {
  string_.Show();
}
#endif

}  // namespace blink
