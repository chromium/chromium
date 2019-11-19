// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_AUDIO_PROCESSOR_CONTROLS_H_
#define MEDIA_WEBRTC_AUDIO_PROCESSOR_CONTROLS_H_

#include "base/callback.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace base {
class File;
}

namespace media {

class AudioProcessorControls {
 public:
  using GetStatsCB = base::OnceCallback<void(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& stats)>;

  // Request the latest stats from the audio processor. Stats are returned
  // asynchronously through |callback|.
  virtual void GetStats(GetStatsCB callback) = 0;

  // Begin dumping echo cancellation data into |file|.
  virtual void StartEchoCancellationDump(base::File file) = 0;

  // Stop any ongoin dump of echo cancellation data.
  virtual void StopEchoCancellationDump() = 0;

 protected:
  virtual ~AudioProcessorControls() = default;
};

}  // namespace media

#endif  // MEDIA_WEBRTC_AUDIO_PROCESSOR_CONTROLS_H_
