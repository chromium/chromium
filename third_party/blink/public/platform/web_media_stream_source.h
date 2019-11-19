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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MEDIA_STREAM_SOURCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MEDIA_STREAM_SOURCE_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_vector.h"

#include "third_party/blink/public/platform/web_private_ptr.h"
#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/heap/handle.h"  // nogncheck
#endif

namespace blink {

class MediaStreamSource;
class WebAudioDestinationConsumer;
class WebPlatformMediaStreamSource;
class WebString;

class WebMediaStreamSource {
 public:

  enum Type { kTypeAudio, kTypeVideo };

  enum ReadyState {
    kReadyStateLive = 0,
    kReadyStateMuted = 1,
    kReadyStateEnded = 2
  };

  enum class EchoCancellationMode { kDisabled, kBrowser, kAec3, kSystem };

  struct Capabilities {
    // WebVector is used to store an optional range for the below numeric
    // fields. All of them should have 0 or 2 values representing min/max.
    WebVector<uint32_t> width;
    WebVector<uint32_t> height;
    WebVector<double> aspect_ratio;
    WebVector<double> frame_rate;
    WebVector<bool> echo_cancellation;
    WebVector<WebString> echo_cancellation_type;
    WebVector<bool> auto_gain_control;
    WebVector<bool> noise_suppression;
    WebVector<int32_t> sample_size;
    WebVector<int32_t> channel_count;
    WebVector<int32_t> sample_rate;
    WebVector<double> latency;

    WebMediaStreamTrack::FacingMode facing_mode =
        WebMediaStreamTrack::FacingMode::kNone;
    WebString device_id;
    WebString group_id;
  };

  WebMediaStreamSource() = default;
  WebMediaStreamSource(const WebMediaStreamSource& other) { Assign(other); }
  ~WebMediaStreamSource() { Reset(); }

  WebMediaStreamSource& operator=(const WebMediaStreamSource& other) {
    Assign(other);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Assign(const WebMediaStreamSource&);

  BLINK_PLATFORM_EXPORT void Initialize(const WebString& id,
                                        Type,
                                        const WebString& name,
                                        bool remote);
  BLINK_PLATFORM_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebString Id() const;
  BLINK_PLATFORM_EXPORT Type GetType() const;
  BLINK_PLATFORM_EXPORT WebString GetName() const;
  BLINK_PLATFORM_EXPORT bool Remote() const;

  BLINK_PLATFORM_EXPORT void SetGroupId(const WebString& group_id);
  BLINK_PLATFORM_EXPORT WebString GroupId() const;

  BLINK_PLATFORM_EXPORT void SetReadyState(ReadyState);
  BLINK_PLATFORM_EXPORT ReadyState GetReadyState() const;

  BLINK_PLATFORM_EXPORT WebPlatformMediaStreamSource* GetPlatformSource() const;
  BLINK_PLATFORM_EXPORT void SetPlatformSource(
      std::unique_ptr<WebPlatformMediaStreamSource>);

  BLINK_PLATFORM_EXPORT void SetAudioProcessingProperties(
      EchoCancellationMode echo_cancellation_mode,
      bool auto_gain_control,
      bool noise_supression);

  BLINK_PLATFORM_EXPORT void SetCapabilities(const Capabilities&);

  // Only used if if this is a WebAudio source.
  // The WebAudioDestinationConsumer is not owned, and has to be disposed of
  // separately after calling removeAudioConsumer.
  BLINK_PLATFORM_EXPORT bool RequiresAudioConsumer() const;
  BLINK_PLATFORM_EXPORT void AddAudioConsumer(WebAudioDestinationConsumer*);
  BLINK_PLATFORM_EXPORT bool RemoveAudioConsumer(WebAudioDestinationConsumer*);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebMediaStreamSource(MediaStreamSource*);
  BLINK_PLATFORM_EXPORT WebMediaStreamSource& operator=(MediaStreamSource*);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<MediaStreamSource>() const;
  BLINK_PLATFORM_EXPORT operator MediaStreamSource*() const;
#endif

 private:
  WebPrivatePtr<MediaStreamSource> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MEDIA_STREAM_SOURCE_H_
