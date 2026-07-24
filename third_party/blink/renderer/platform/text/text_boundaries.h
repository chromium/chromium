/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_BOUNDARIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_BOUNDARIES_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// `chars` should be a string in logical order instead of visual order, since
// these functions use ICU, which works on logical order strings.
// Note: These functions will crash if `position` exceeds the maximum value of
// `int`.

// Finds the start boundary of the word that contains the given `position`.
// As an edge case, if `position` is at or beyond the end of the string,
// it returns the length of the string.
PLATFORM_EXPORT wtf_size_t FindWordStartBoundary(base::span<const UChar> chars,
                                                 wtf_size_t position);

// Finds the end boundary of the word that contains the given `position`.
// As an edge case, if `position` is at or beyond the end of the string,
// it returns the length of the string.
PLATFORM_EXPORT wtf_size_t FindWordEndBoundary(base::span<const UChar> chars,
                                               wtf_size_t position);

// Finds the start of the next word moving backward from `position`.
// A word boundary is considered valid if the character following the break is
// alphanumeric or an underscore. As an edge case, if no such boundary
// is found, it returns 0.
PLATFORM_EXPORT wtf_size_t FindNextWordBackward(base::span<const UChar> chars,
                                                wtf_size_t position);

// Finds the end of the next word moving forward from `position`.
// A word boundary is considered valid if the character preceding the break is
// alphanumeric or an underscore. As an edge case, if no such boundary
// is found, it returns the length of the string.
PLATFORM_EXPORT wtf_size_t FindNextWordForward(base::span<const UChar> chars,
                                               wtf_size_t position);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_BOUNDARIES_H_
