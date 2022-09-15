// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamProvider is used to capture media of the types defined in
// MediaStreamType. There is only one MediaStreamProvider instance per media
// type and a MediaStreamProvider instance can have only one registered
// listener.
// The MediaStreamManager is expected to be called on Browser::IO thread and
// the listener will be called on the same thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

enum MediaStreamProviderError {
  kMediaStreamOk = 0,
  kInvalidMediaStreamType,
  kInvalidSession,
  kUnknownSession,
  kDeviceNotAvailable,
  kDeviceAlreadyInUse,
  kUnknownError
};

// Callback class used by MediaStreamProvider.
class MediaStreamProviderListener {
 public:
  // Called by a MediaStreamProvider when a stream has been opened.
  virtual void Opened(blink::mojom::MediaStreamType stream_type,
                      const base::UnguessableToken& capture_session_id) = 0;

  // Called by a MediaStreamProvider when a stream has been closed.
  virtual void Closed(blink::mojom::MediaStreamType stream_type,
                      const base::UnguessableToken& capture_session_id) = 0;

  // Called by a MediaStreamProvider when the device has been aborted due to
  // device error.
  virtual void Aborted(blink::mojom::MediaStreamType stream_type,
                       const base::UnguessableToken& capture_session_id) = 0;

 protected:
  virtual ~MediaStreamProviderListener() {}
};

// Implemented by a manager class providing captured media.
class MediaStreamProvider
    : public base::RefCountedThreadSafe<MediaStreamProvider> {
 public:
  // Registers a listener.
  virtual void RegisterListener(MediaStreamProviderListener* listener) = 0;

  // Unregisters a previously registered listener.
  virtual void UnregisterListener(MediaStreamProviderListener* listener) = 0;

  // Opens the specified device. The device is not started and it is still
  // possible for other applications to open the device before the device is
  // started. |Opened| is called when the device is opened.
  // kInvalidMediaCaptureSessionId is returned on error.
  virtual base::UnguessableToken Open(
      const blink::MediaStreamDevice& device) = 0;

  // Closes the specified device and calls |Closed| when done.
  virtual void Close(const base::UnguessableToken& capture_session_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<MediaStreamProvider>;
  virtual ~MediaStreamProvider() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_PROVIDER_H_
