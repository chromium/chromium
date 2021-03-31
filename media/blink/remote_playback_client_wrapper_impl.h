// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_REMOTE_PLAYBACK_CLIENT_WRAPPER_IMPL_H_
#define MEDIA_BLINK_REMOTE_PLAYBACK_CLIENT_WRAPPER_IMPL_H_

#include <string>

#include "media/blink/media_blink_export.h"
#include "media/renderers/remote_playback_client_wrapper.h"

namespace blink {
class WebMediaPlayerClient;
class WebRemotePlaybackClient;
}  // namespace blink

namespace media {

// Wraps a WebRemotePlaybackClient to expose only the methods used by the
// FlingingRendererClientFactory. This avoids dependencies on the blink layer.
class MEDIA_BLINK_EXPORT RemotePlaybackClientWrapperImpl
    : public RemotePlaybackClientWrapper {
 public:
  explicit RemotePlaybackClientWrapperImpl(blink::WebMediaPlayerClient* client);
  ~RemotePlaybackClientWrapperImpl() override;

  std::string GetActivePresentationId() override;

 private:
  blink::WebRemotePlaybackClient* remote_playback_client_;
};

}  // namespace media

#endif  // MEDIA_BLINK_REMOTE_PLAYBACK_CLIENT_WRAPPER_IMPL_H_
