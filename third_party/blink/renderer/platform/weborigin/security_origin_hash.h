/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SECURITY_ORIGIN_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SECURITY_ORIGIN_HASH_H_

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This Hash implements the "same origin" equality relation between two origins.
// As such it ignores the domain that might or might not be set on the origin.
// If you need "same origin-domain" equality you'll need to use a different hash
// function.
struct SecurityOriginHash {
  STATIC_ONLY(SecurityOriginHash);
  static unsigned GetHash(const SecurityOrigin* origin) {
    base::Optional<base::UnguessableToken> nonce =
        origin->GetNonceForSerialization();
    size_t nonce_hash = nonce ? base::UnguessableTokenHash()(*nonce) : 0;

    unsigned hash_codes[] = {
      origin->Protocol().Impl() ? origin->Protocol().Impl()->GetHash() : 0,
      origin->Host().Impl() ? origin->Host().Impl()->GetHash() : 0,
      origin->Port(),
#if ARCH_CPU_32_BITS
      nonce_hash,
#elif ARCH_CPU_64_BITS
      static_cast<unsigned>(nonce_hash),
      static_cast<unsigned>(nonce_hash >> 32),
#else
#error "Unknown bits"
#endif
    };
    return StringHasher::HashMemory<sizeof(hash_codes)>(hash_codes);
  }
  static unsigned GetHash(const scoped_refptr<const SecurityOrigin>& origin) {
    return GetHash(origin.get());
  }

  static bool Equal(const SecurityOrigin* a, const SecurityOrigin* b) {
    if (!a || !b)
      return a == b;

    return a->IsSameSchemeHostPort(b);
  }
  static bool Equal(const SecurityOrigin* a,
                    const scoped_refptr<const SecurityOrigin>& b) {
    return Equal(a, b.get());
  }
  static bool Equal(const scoped_refptr<const SecurityOrigin>& a,
                    const SecurityOrigin* b) {
    return Equal(a.get(), b);
  }
  static bool Equal(const scoped_refptr<const SecurityOrigin>& a,
                    const scoped_refptr<const SecurityOrigin>& b) {
    return Equal(a.get(), b.get());
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SECURITY_ORIGIN_HASH_H_
