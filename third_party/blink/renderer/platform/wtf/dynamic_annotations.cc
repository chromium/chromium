/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include <stdint.h>

#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"

#if defined(DYNAMIC_ANNOTATIONS_ENABLED) && \
    !defined(DYNAMIC_ANNOTATIONS_EXTERNAL_IMPL)

// Identical code folding(-Wl,--icf=all) countermeasures.
// This makes all Annotate* functions different, which prevents the linker from
// folding them.
#ifdef __COUNTER__
#define DYNAMIC_ANNOTATIONS_IMPL                            \
  volatile uint16_t lineno = (__LINE__ << 8) + __COUNTER__; \
  (void)lineno;
#else
#define DYNAMIC_ANNOTATIONS_IMPL              \
  volatile uint16_t lineno = (__LINE__ << 8); \
  (void)lineno;
#endif

void WTFAnnotateBenignRaceSized(const char*,
                                int,
                                const volatile void*,
                                size_t,
                                const char*) {
  // The TSan runtime hardcodes the function namem "WTFAnnotateBenignRaceSized",
  // that's how it knows this function is magic.
  DYNAMIC_ANNOTATIONS_IMPL
}

#endif  // defined(DYNAMIC_ANNOTATIONS_ENABLED) &&
        // !defined(DYNAMIC_ANNOTATIONS_EXTERNAL_IMPL)
