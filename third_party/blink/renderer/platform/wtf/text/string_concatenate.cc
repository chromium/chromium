// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

WTF::StringTypeAdapter<const char*>::StringTypeAdapter(const LChar* buffer,
                                                       size_t length)
    : buffer_(buffer), length_(base::checked_cast<unsigned>(length)) {}

void WTF::StringTypeAdapter<const char*>::WriteTo(LChar* destination) const {
  memcpy(destination, buffer_, length_);
}

void WTF::StringTypeAdapter<const char*>::WriteTo(UChar* destination) const {
  StringImpl::CopyChars(destination, buffer_, length_);
}

WTF::StringTypeAdapter<const UChar*>::StringTypeAdapter(const UChar* buffer)
    : buffer_(buffer), length_(LengthOfNullTerminatedString(buffer)) {}

void WTF::StringTypeAdapter<const UChar*>::WriteTo(UChar* destination) const {
  memcpy(destination, buffer_, length_ * sizeof(UChar));
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
