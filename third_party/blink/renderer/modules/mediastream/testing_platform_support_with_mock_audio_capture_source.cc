// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/testing_platform_support_with_mock_audio_capture_source.h"

namespace blink {

scoped_refptr<media::AudioCapturerSource>
AudioCapturerSourceTestingPlatformSupport::NewAudioCapturerSource(
    WebLocalFrame* web_frame,
    const media::AudioSourceParameters& params) {
  return mock_audio_capturer_source_;
}

}  // namespace blink
