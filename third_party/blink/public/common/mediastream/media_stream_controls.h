// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_STREAM_CONTROLS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_STREAM_CONTROLS_H_

#include <string>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace blink {

// Names for media stream source capture types.
// These are values set via the "chromeMediaSource" constraint.
BLINK_COMMON_EXPORT extern const char kMediaStreamSourceTab[];
BLINK_COMMON_EXPORT extern const char
    kMediaStreamSourceScreen[]; /* video only */
BLINK_COMMON_EXPORT extern const char kMediaStreamSourceDesktop[];
BLINK_COMMON_EXPORT extern const char
    kMediaStreamSourceSystem[]; /* audio only */

struct BLINK_COMMON_EXPORT TrackControls {
  TrackControls();
  explicit TrackControls(bool request, mojom::MediaStreamType type);
  explicit TrackControls(const TrackControls& other);
  ~TrackControls();

  bool requested = false;

  // Represents the requested  stream type.
  mojom::MediaStreamType stream_type = mojom::MediaStreamType::NO_SERVICE;

  // An empty string represents the default device.
  // A nonempty string represents a specific device.
  std::string device_id;
};

// StreamControls describes what is sent to the browser process
// from the renderer process in order to control the opening of a device
// pair. This may result in opening one audio and/or one video device.
// This has to be a struct with public members in order to allow it to
// be sent in the mojo IPC.
struct BLINK_COMMON_EXPORT StreamControls {
  StreamControls();
  StreamControls(bool request_audio, bool request_video);
  ~StreamControls();

  TrackControls audio;
  TrackControls video;
  // Hotword functionality (chromeos only)
  // TODO(crbug.com/577627): this is now never set and needs to be removed.
  bool hotword_enabled = false;
  bool disable_local_echo = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_STREAM_CONTROLS_H_
