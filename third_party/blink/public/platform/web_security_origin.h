/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SECURITY_ORIGIN_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SECURITY_ORIGIN_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/origin.h"

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#endif

namespace blink {

class SecurityOrigin;
class WebURL;

class WebSecurityOrigin {
 public:
  ~WebSecurityOrigin() { Reset(); }

  WebSecurityOrigin() = default;
  WebSecurityOrigin(const WebSecurityOrigin& s) { Assign(s); }
  WebSecurityOrigin& operator=(const WebSecurityOrigin& s) {
    Assign(s);
    return *this;
  }

  BLINK_PLATFORM_EXPORT static WebSecurityOrigin CreateFromString(
      const WebString&);
  BLINK_PLATFORM_EXPORT static WebSecurityOrigin Create(const WebURL&);
  BLINK_PLATFORM_EXPORT static WebSecurityOrigin CreateUniqueOpaque();

  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebSecurityOrigin&);

  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebString Protocol() const;
  BLINK_PLATFORM_EXPORT WebString Host() const;
  BLINK_PLATFORM_EXPORT uint16_t Port() const;

  // |Port()| will return 0 if the port is the default for an origin. This
  // method instead returns the effective port, even if it is the default port
  // (e.g. "http" => 80).
  BLINK_PLATFORM_EXPORT uint16_t EffectivePort() const;

  // A unique WebSecurityOrigin is the least privileged WebSecurityOrigin.
  BLINK_PLATFORM_EXPORT bool IsOpaque() const;

  // Returns true if this WebSecurityOrigin can script objects in the given
  // SecurityOrigin. For example, call this function before allowing
  // script from one security origin to read or write objects from
  // another SecurityOrigin.
  BLINK_PLATFORM_EXPORT bool CanAccess(const WebSecurityOrigin&) const;

  // Returns true if this WebSecurityOrigin can read content retrieved from
  // the given URL. For example, call this function before allowing script
  // from a given security origin to receive contents from a given URL.
  BLINK_PLATFORM_EXPORT bool CanRequest(const WebURL&) const;

  // Returns true if this WebSecurityOrigin can display content from the given
  // URL (e.g., in an iframe or as an image). For example, web sites generally
  // cannot display content from the user's files system.
  BLINK_PLATFORM_EXPORT bool CanDisplay(const WebURL&) const;

  // Returns true if the origin loads resources either from the local
  // machine or over the network from a
  // cryptographically-authenticated origin, as described in
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy.
  BLINK_PLATFORM_EXPORT bool IsPotentiallyTrustworthy() const;

  // Returns a string representation of the WebSecurityOrigin.  The empty
  // WebSecurityOrigin is represented by "null".  The representation of a
  // non-empty WebSecurityOrigin resembles a standard URL.
  BLINK_PLATFORM_EXPORT WebString ToString() const;

  // Returns true if this WebSecurityOrigin can access usernames and
  // passwords stored in password manager.
  BLINK_PLATFORM_EXPORT bool CanAccessPasswordManager() const;

  // This method implements HTML's "same origin" check, which verifies equality
  // of opaque origins, or exact (scheme,host,port) matches. Note that
  // `document.domain` does not come into play for this comparison.
  //
  // This method does not take the "universal access" flag into account. It does
  // take the "local access" flag into account, considering `file:` origins that
  // set the flag to be same-origin with all other `file:` origins that set the
  // flag.
  //
  // https://html.spec.whatwg.org/#same-origin
  BLINK_PLATFORM_EXPORT bool IsSameOriginWith(const WebSecurityOrigin&) const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebSecurityOrigin(scoped_refptr<const SecurityOrigin>);
  BLINK_PLATFORM_EXPORT WebSecurityOrigin& operator=(
      scoped_refptr<const SecurityOrigin>);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<const SecurityOrigin>() const;
  BLINK_PLATFORM_EXPORT const SecurityOrigin* Get() const;
#endif
  BLINK_PLATFORM_EXPORT WebSecurityOrigin(const url::Origin&);
  BLINK_PLATFORM_EXPORT operator url::Origin() const;

#if DCHECK_IS_ON()
  BLINK_PLATFORM_EXPORT bool operator==(const WebSecurityOrigin&) const;
#endif

 private:
  WebPrivatePtr<const SecurityOrigin> private_;
};

}  // namespace blink

#endif
