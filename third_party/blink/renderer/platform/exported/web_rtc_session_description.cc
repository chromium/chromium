/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_rtc_session_description.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class WebRTCSessionDescriptionPrivate final
    : public RefCounted<WebRTCSessionDescriptionPrivate> {
  USING_FAST_MALLOC(WebRTCSessionDescription);

 public:
  static scoped_refptr<WebRTCSessionDescriptionPrivate> Create(
      const WebString& type,
      const WebString& sdp);

  WebString GetType() { return type_; }
  void SetType(const WebString& type) { type_ = type; }

  WebString Sdp() { return sdp_; }
  void SetSdp(const WebString& sdp) { sdp_ = sdp; }

 private:
  WebRTCSessionDescriptionPrivate(const WebString& type, const WebString& sdp);

  WebString type_;
  WebString sdp_;
};

scoped_refptr<WebRTCSessionDescriptionPrivate>
WebRTCSessionDescriptionPrivate::Create(const WebString& type,
                                        const WebString& sdp) {
  return base::AdoptRef(new WebRTCSessionDescriptionPrivate(type, sdp));
}

WebRTCSessionDescriptionPrivate::WebRTCSessionDescriptionPrivate(
    const WebString& type,
    const WebString& sdp)
    : type_(type), sdp_(sdp) {}

void WebRTCSessionDescription::Assign(const WebRTCSessionDescription& other) {
  private_ = other.private_;
}

void WebRTCSessionDescription::Reset() {
  private_.Reset();
}

void WebRTCSessionDescription::Initialize(const WebString& type,
                                          const WebString& sdp) {
  private_ = WebRTCSessionDescriptionPrivate::Create(type, sdp);
}

WebString WebRTCSessionDescription::GetType() const {
  DCHECK(!private_.IsNull());
  return private_->GetType();
}

void WebRTCSessionDescription::SetType(const WebString& type) {
  DCHECK(!private_.IsNull());
  return private_->SetType(type);
}

WebString WebRTCSessionDescription::Sdp() const {
  DCHECK(!private_.IsNull());
  return private_->Sdp();
}

void WebRTCSessionDescription::SetSDP(const WebString& sdp) {
  DCHECK(!private_.IsNull());
  return private_->SetSdp(sdp);
}

}  // namespace blink
