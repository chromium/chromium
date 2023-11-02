/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_source.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

WebMediaStreamSource::WebMediaStreamSource(
    MediaStreamSource* media_stream_source)
    : private_(media_stream_source) {}

WebMediaStreamSource& WebMediaStreamSource::operator=(
    MediaStreamSource* media_stream_source) {
  private_ = media_stream_source;
  return *this;
}

void WebMediaStreamSource::Assign(const WebMediaStreamSource& other) {
  private_ = other.private_;
}

void WebMediaStreamSource::Reset() {
  private_.Reset();
}

WebMediaStreamSource::operator MediaStreamSource*() const {
  return private_.Get();
}

void WebMediaStreamSource::Initialize(
    const WebString& id,
    Type type,
    const WebString& name,
    bool remote,
    std::unique_ptr<WebPlatformMediaStreamSource> platform_source) {
  private_ = MakeGarbageCollected<MediaStreamSource>(
      id, static_cast<MediaStreamSource::StreamType>(type), name, remote,
      std::move(platform_source));
}

WebString WebMediaStreamSource::Id() const {
  DCHECK(!private_.IsNull());
  return private_.Get()->Id();
}

WebMediaStreamSource::Type WebMediaStreamSource::GetType() const {
  DCHECK(!private_.IsNull());
  return static_cast<Type>(private_.Get()->GetType());
}

void WebMediaStreamSource::SetReadyState(ReadyState state) {
  DCHECK(!private_.IsNull());
  private_->SetReadyState(static_cast<MediaStreamSource::ReadyState>(state));
}

WebMediaStreamSource::ReadyState WebMediaStreamSource::GetReadyState() const {
  DCHECK(!private_.IsNull());
  return static_cast<ReadyState>(private_->GetReadyState());
}

WebPlatformMediaStreamSource* WebMediaStreamSource::GetPlatformSource() const {
  DCHECK(!private_.IsNull());
  return private_->GetPlatformSource();
}

}  // namespace blink
