// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

void WTF::StringTypeAdapter<const char*>::WriteTo(
    base::span<LChar> destination) const {
  destination.copy_from(buffer_);
}

void WTF::StringTypeAdapter<const char*>::WriteTo(
    base::span<UChar> destination) const {
  StringImpl::CopyChars(destination, buffer_);
}

WTF::StringTypeAdapter<const UChar*>::StringTypeAdapter(const UChar* buffer)
    : buffer_(base::span(std::u16string_view(buffer))) {}

void WTF::StringTypeAdapter<const UChar*>::WriteTo(
    base::span<UChar> destination) const {
  destination.copy_from(buffer_);
}

void WTF::StringTypeAdapter<StringView>::WriteTo(
    base::span<LChar> destination) const {
  destination.copy_from(view_.Span8());
}

void WTF::StringTypeAdapter<StringView>::WriteTo(
    base::span<UChar> destination) const {
  VisitCharacters(view_, [destination](auto chars) {
    StringImpl::CopyChars(destination, chars);
  });
}
