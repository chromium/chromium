// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

WTF::StringTypeAdapter<char*>::StringTypeAdapter(char* buffer, size_t length)
    : buffer_(buffer), length_(base::checked_cast<unsigned>(length)) {}

void WTF::StringTypeAdapter<char*>::WriteTo(LChar* destination) const {
  for (unsigned i = 0; i < length_; ++i)
    destination[i] = static_cast<LChar>(buffer_[i]);
}

void WTF::StringTypeAdapter<char*>::WriteTo(UChar* destination) const {
  for (unsigned i = 0; i < length_; ++i) {
    unsigned char c = buffer_[i];
    destination[i] = c;
  }
}

WTF::StringTypeAdapter<LChar*>::StringTypeAdapter(LChar* buffer)
    : buffer_(buffer),
      length_(base::checked_cast<wtf_size_t>(
          strlen(reinterpret_cast<char*>(buffer)))) {}

void WTF::StringTypeAdapter<LChar*>::WriteTo(LChar* destination) const {
  memcpy(destination, buffer_, length_ * sizeof(LChar));
}

void WTF::StringTypeAdapter<LChar*>::WriteTo(UChar* destination) const {
  StringImpl::CopyChars(destination, buffer_, length_);
}

WTF::StringTypeAdapter<const UChar*>::StringTypeAdapter(const UChar* buffer)
    : buffer_(buffer), length_(LengthOfNullTerminatedString(buffer)) {}

void WTF::StringTypeAdapter<const UChar*>::WriteTo(UChar* destination) const {
  memcpy(destination, buffer_, length_ * sizeof(UChar));
}

WTF::StringTypeAdapter<const char*>::StringTypeAdapter(const char* buffer)
    : buffer_(buffer),
      length_(base::checked_cast<wtf_size_t>(strlen(buffer))) {}

void WTF::StringTypeAdapter<const char*>::WriteTo(LChar* destination) const {
  memcpy(destination, buffer_, static_cast<size_t>(length_) * sizeof(LChar));
}

void WTF::StringTypeAdapter<const char*>::WriteTo(UChar* destination) const {
  for (unsigned i = 0; i < length_; ++i) {
    unsigned char c = buffer_[i];
    destination[i] = c;
  }
}

WTF::StringTypeAdapter<const LChar*>::StringTypeAdapter(const LChar* buffer)
    : buffer_(buffer),
      length_(base::checked_cast<wtf_size_t>(
          strlen(reinterpret_cast<const char*>(buffer)))) {}

void WTF::StringTypeAdapter<const LChar*>::WriteTo(LChar* destination) const {
  memcpy(destination, buffer_, static_cast<size_t>(length_) * sizeof(LChar));
}

void WTF::StringTypeAdapter<const LChar*>::WriteTo(UChar* destination) const {
  StringImpl::CopyChars(destination, buffer_, length_);
}

void WTF::StringTypeAdapter<StringView>::WriteTo(LChar* destination) const {
  DCHECK(Is8Bit());
  StringImpl::CopyChars(destination, view_.Characters8(), view_.length());
}

void WTF::StringTypeAdapter<StringView>::WriteTo(UChar* destination) const {
  if (Is8Bit())
    StringImpl::CopyChars(destination, view_.Characters8(), view_.length());
  else
    StringImpl::CopyChars(destination, view_.Characters16(), view_.length());
}
