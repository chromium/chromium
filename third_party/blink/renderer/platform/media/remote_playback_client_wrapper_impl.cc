// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/media/remote_playback_client_wrapper_impl.h"

#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

RemotePlaybackClientWrapperImpl::RemotePlaybackClientWrapperImpl(
    WebMediaPlayerClient* client)
    : remote_playback_client_(client->RemotePlaybackClient()) {}

RemotePlaybackClientWrapperImpl::~RemotePlaybackClientWrapperImpl() = default;

std::string RemotePlaybackClientWrapperImpl::GetActivePresentationId() {
  if (!remote_playback_client_)
    return std::string();

  // The presentation ID is essentially a GUID preceeded by the "mr_" prefix,
  // which makes it ASCII compatible.
  // If MediaRouterBase::CreatePresentationId() were changed, this line might
  // need to be updated.
  return remote_playback_client_->GetPresentationId().Ascii();
}

}  // namespace blink
