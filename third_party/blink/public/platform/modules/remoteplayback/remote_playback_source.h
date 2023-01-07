// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_SOURCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_SOURCE_H_

namespace blink {
// TODO(crbug.com/1353987): Convert Url format for Android to use
// `remote-playback:media-element?...`.
// The scheme for RemotePlayback Urls on both Android and Desktop.
constexpr char kRemotePlaybackPresentationUrlScheme[] = "remote-playback";
// The format for RemotePlayback Urls on desktop.
constexpr char kRemotePlaybackDesktopUrlFormat[] =
    "remote-playback:media-session?tab_id=%d&video_codec=%s&audio_codec=%s";
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_SOURCE_H_
