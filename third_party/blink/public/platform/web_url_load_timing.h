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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOAD_TIMING_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOAD_TIMING_H_

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"  // nogncheck
#endif

namespace blink {

class ResourceLoadTiming;

// The browser-side equivalent to this struct is content::ResourceLoadTiming.
// TODO(dcheng): Migrate this struct over to Mojo so it doesn't need to be
// duplicated in //content and //third_party/blink.
class WebURLLoadTiming {
 public:
  ~WebURLLoadTiming() { Reset(); }

  WebURLLoadTiming() = default;
  WebURLLoadTiming(const WebURLLoadTiming& d) { Assign(d); }
  WebURLLoadTiming& operator=(const WebURLLoadTiming& d) {
    Assign(d);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Initialize();
  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebURLLoadTiming&);

  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT base::TimeTicks RequestTime() const;
  BLINK_PLATFORM_EXPORT void SetRequestTime(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks ProxyStart() const;
  BLINK_PLATFORM_EXPORT void SetProxyStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks ProxyEnd() const;
  BLINK_PLATFORM_EXPORT void SetProxyEnd(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks DnsStart() const;
  BLINK_PLATFORM_EXPORT void SetDNSStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks DnsEnd() const;
  BLINK_PLATFORM_EXPORT void SetDNSEnd(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks ConnectStart() const;
  BLINK_PLATFORM_EXPORT void SetConnectStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks ConnectEnd() const;
  BLINK_PLATFORM_EXPORT void SetConnectEnd(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks WorkerStart() const;
  BLINK_PLATFORM_EXPORT void SetWorkerStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks WorkerReady() const;
  BLINK_PLATFORM_EXPORT void SetWorkerReady(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks SendStart() const;
  BLINK_PLATFORM_EXPORT void SetSendStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks SendEnd() const;
  BLINK_PLATFORM_EXPORT void SetSendEnd(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks ReceiveHeadersStart() const;
  BLINK_PLATFORM_EXPORT void SetReceiveHeadersStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks ReceiveHeadersEnd() const;
  BLINK_PLATFORM_EXPORT void SetReceiveHeadersEnd(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks SslStart() const;
  BLINK_PLATFORM_EXPORT void SetSSLStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks SslEnd() const;
  BLINK_PLATFORM_EXPORT void SetSSLEnd(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks PushStart() const;
  BLINK_PLATFORM_EXPORT void SetPushStart(base::TimeTicks);

  BLINK_PLATFORM_EXPORT base::TimeTicks PushEnd() const;
  BLINK_PLATFORM_EXPORT void SetPushEnd(base::TimeTicks);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebURLLoadTiming(scoped_refptr<ResourceLoadTiming>);
  BLINK_PLATFORM_EXPORT WebURLLoadTiming& operator=(
      scoped_refptr<ResourceLoadTiming>);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<ResourceLoadTiming>() const;
  BLINK_PLATFORM_EXPORT WebURLLoadTiming DeepCopy() const;
  BLINK_PLATFORM_EXPORT bool operator==(const WebURLLoadTiming&) const;
#endif

 private:
  WebPrivatePtr<ResourceLoadTiming> private_;
};

}  // namespace blink

namespace WTF {
#if INSIDE_BLINK
template <>
struct CrossThreadCopier<blink::WebURLLoadTiming> {
  STATIC_ONLY(CrossThreadCopier);
  typedef blink::WebURLLoadTiming Type;
  PLATFORM_EXPORT static Type Copy(const blink::WebURLLoadTiming&);
};
#endif
}  // namespace WTF

#endif
