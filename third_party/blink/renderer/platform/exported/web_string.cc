/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_string.h"

#include "base/strings/latin1_string_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_fast_path.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

STATIC_ASSERT_ENUM(blink::Utf8ConversionMode::kLenient,
                   blink::WebString::UTF8ConversionMode::kLenient);
STATIC_ASSERT_ENUM(blink::Utf8ConversionMode::kStrict,
                   blink::WebString::UTF8ConversionMode::kStrict);
STATIC_ASSERT_ENUM(
    blink::Utf8ConversionMode::kStrictReplacingErrors,
    blink::WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);

namespace blink {

WebString::~WebString() = default;
WebString::WebString() = default;
WebString::WebString(const WebString&) = default;
WebString::WebString(WebString&&) = default;
WebString& WebString::operator=(const WebString&) = default;
WebString& WebString::operator=(WebString&&) = default;

WebString::WebString(std::u16string_view s)
    : impl_(StringImpl::Create8BitIfPossible(s)) {}

void WebString::Reset() {
  impl_ = nullptr;
}

size_t WebString::length() const {
  return impl_ ? impl_->length() : 0;
}

bool WebString::Is8Bit() const {
  return impl_->Is8Bit();
}

std::string WebString::Utf8(UTF8ConversionMode mode) const {
  return String(impl_).Utf8(static_cast<Utf8ConversionMode>(mode));
}

WebString WebString::Substring(size_t pos, size_t len) const {
  return String(impl_->Substring(base::checked_cast<wtf_size_t>(pos),
                                 base::checked_cast<wtf_size_t>(len)));
}

WebString WebString::FromUTF8(std::string_view s) {
  return String::FromUTF8(s);
}

std::u16string WebString::Utf16() const {
  return impl_ ? impl_->ToU16String() : std::u16string();
}

WebString WebString::FromUTF16(std::optional<std::u16string_view> s) {
  if (!s.has_value()) {
    return WebString();
  }
  return WebString(*s);
}

std::string WebString::Latin1() const {
  return String(impl_).Latin1();
}

WebString WebString::FromLatin1(std::string_view s) {
  return String(s);
}

std::string WebString::Ascii() const {
  DCHECK(ContainsOnlyASCII());

  if (IsEmpty())
    return std::string();

  if (impl_->Is8Bit()) {
    auto latin1 = base::as_chars(impl_->Span8());
    return std::string(base::as_string_view(latin1));
  }

  auto utf16 = impl_->Span16();
  return std::string(utf16.begin(), utf16.end());
}

bool WebString::ContainsOnlyASCII() const {
  return String(impl_).ContainsOnlyASCIIOrEmpty();
}

WebString WebString::FromASCII(std::string_view s) {
  DCHECK(base::IsStringASCII(s));
  return FromLatin1(s);
}

bool WebString::Equals(const WebString& s) const {
  return Equal(impl_.get(), s.impl_.get());
}

bool WebString::Equals(std::string_view characters) const {
  return Equal(impl_.get(), characters);
}

size_t WebString::Find(const WebString& s) const {
  if (!impl_) {
    return std::string::npos;
  }
  wtf_size_t pos = impl_->Find(s.impl_.get());
  return pos != kNotFound ? pos : std::string::npos;
}

size_t WebString::Find(std::string_view characters) const {
  if (!impl_) {
    return std::string::npos;
  }
  wtf_size_t pos = impl_->Find(characters.data());
  return pos != kNotFound ? pos : std::string::npos;
}

bool WebString::operator<(const WebString& other) const {
  return CodeUnitCompare(impl_.get(), other.impl_.get()) < 0;
}

WebString::WebString(const String& s) : impl_(s.Impl()) {}

WebString& WebString::operator=(const String& s) {
  impl_ = s.Impl();
  return *this;
}

WebString::operator String() const {
  return impl_.get();
}

WebString::operator StringView() const {
  return StringView(impl_.get());
}

WebString::WebString(const AtomicString& s) {
  impl_ = s.Impl();
}

WebString& WebString::operator=(const AtomicString& s) {
  impl_ = s.Impl();
  return *this;
}

WebString::operator AtomicString() const {
  return AtomicString(impl_);
}

}  // namespace blink
