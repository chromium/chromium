/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"

#include <memory>
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

WebMediaStream::WebMediaStream(MediaStreamDescriptor* media_stream_descriptor)
    : private_(media_stream_descriptor) {}

void WebMediaStream::Reset() {
  private_.Reset();
}

WebString WebMediaStream::Id() const {
  return private_->Id();
}

int WebMediaStream::UniqueId() const {
  return private_->UniqueId();
}

void WebMediaStream::AddObserver(WebMediaStreamObserver* observer) {
  DCHECK(!IsNull());
  private_->AddObserver(observer);
}

void WebMediaStream::RemoveObserver(WebMediaStreamObserver* observer) {
  DCHECK(!IsNull());
  private_->RemoveObserver(observer);
}

WebMediaStream& WebMediaStream::operator=(
    MediaStreamDescriptor* media_stream_descriptor) {
  private_ = media_stream_descriptor;
  return *this;
}

WebMediaStream::operator MediaStreamDescriptor*() const {
  return private_.Get();
}

void WebMediaStream::Assign(const WebMediaStream& other) {
  private_ = other.private_;
}

}  // namespace blink
