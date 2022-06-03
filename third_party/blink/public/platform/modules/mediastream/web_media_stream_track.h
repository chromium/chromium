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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_TRACK_H_

#include "media/mojo/mojom/display_media_information.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

namespace blink {

class MediaStreamComponent;
class WebAudioSourceProvider;
class WebMediaStreamSource;

class WebMediaStreamTrack {
 public:
  enum class FacingMode { kNone, kUser, kEnvironment, kLeft, kRight };

  BLINK_PLATFORM_EXPORT static const char kResizeModeNone[];
  BLINK_PLATFORM_EXPORT static const char kResizeModeRescale[];

  enum class ContentHintType {
    kNone,
    kAudioSpeech,
    kAudioMusic,
    kVideoMotion,
    kVideoDetail,
    kVideoText
  };

  WebMediaStreamTrack() = default;
  WebMediaStreamTrack(const WebMediaStreamTrack& other) { Assign(other); }
  ~WebMediaStreamTrack() { Reset(); }

  WebMediaStreamTrack& operator=(const WebMediaStreamTrack& other) {
    Assign(other);
    return *this;
  }
  BLINK_PLATFORM_EXPORT void Assign(const WebMediaStreamTrack&);

  BLINK_PLATFORM_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebMediaStreamSource Source() const;

  // The lifetime of the WebAudioSourceProvider should outlive the
  // WebMediaStreamTrack, and clients are responsible for calling
  // SetSourceProvider(0) before the WebMediaStreamTrack is going away.
  BLINK_PLATFORM_EXPORT void SetSourceProvider(WebAudioSourceProvider*);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT explicit WebMediaStreamTrack(MediaStreamComponent*);
  BLINK_PLATFORM_EXPORT WebMediaStreamTrack& operator=(MediaStreamComponent*);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<MediaStreamComponent>() const;
  BLINK_PLATFORM_EXPORT operator MediaStreamComponent*() const;
#endif

 private:
  WebPrivatePtr<MediaStreamComponent> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_TRACK_H_
