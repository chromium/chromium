// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_SCREEN_ENUMERATOR_H_
#define MEDIA_CAPTURE_CONTENT_SCREEN_ENUMERATOR_H_

#include "base/functional/callback.h"

namespace blink::mojom {
class StreamDevicesSet;
enum class MediaStreamType;
enum class MediaStreamRequestResult;
}  // namespace blink::mojom

namespace media {

// This class provides an interface for enumeration of all attached screens.
// The screens are returned in a callback all at once (instead of one
// callback per screen as soon as it is discovered).
class ScreenEnumerator {
 public:
  virtual ~ScreenEnumerator() = default;

  // This function triggers enumeration of all available screens and calls
  // the |screens_callback| with all screen as MediaStreamDevices.
  virtual void EnumerateScreens(
      blink::mojom::MediaStreamType stream_type,
      base::OnceCallback<
          void(const blink::mojom::StreamDevicesSet& stream_devices_set,
               blink::mojom::MediaStreamRequestResult result)> screens_callback)
      const = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_SCREEN_ENUMERATOR_H_
