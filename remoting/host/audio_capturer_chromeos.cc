// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "remoting/host/audio_capturer.h"

namespace remoting {

bool AudioCapturer::IsSupported() {
  return false;
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting
