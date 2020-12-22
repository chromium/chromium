// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_PLAYER_DELEGATE_MESSAGES_H_
#define CONTENT_COMMON_MEDIA_MEDIA_PLAYER_DELEGATE_MESSAGES_H_

// IPC messages for interactions between the WebMediaPlayerDelegate in the
// renderer process and MediaWebContentsObserver in the browser process.

// TODO(apacible): Mojoify MediaPlayerDelegateMsg, then remove this file.
// https://crbug.com/824965

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/media_content_type.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerDelegateMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaContentType, media::MediaContentType::Max)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFullscreenVideoStatus,
                          blink::WebFullscreenVideoStatus::kMaxValue)

// ----------------------------------------------------------------------------
// Messages from the browser to the renderer requesting playback state changes.
// ----------------------------------------------------------------------------

IPC_MESSAGE_ROUTED0(MediaPlayerDelegateMsg_SuspendAllMediaPlayers)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_UpdateVolumeMultiplier,
                    int /* delegate_id, distinguishes instances */,
                    double /* multiplier */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_BecamePersistentVideo,
                    int /* delegate_id, distinguishes instances */,
                    double /* is_persistent */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_SetAudioSinkId,
                    int /* delegate_id, distinguishes instances */,
                    std::string /* sink_id */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_NotifyPowerExperimentState,
                    int /* delegate_id, distinguishes instances */,
                    bool /* is experiment starting (true) or stopping? */)

// ----------------------------------------------------------------------------
// Messages from the renderer notifying the browser of playback state changes.
// ----------------------------------------------------------------------------

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnMediaPaused,
                    int /* delegate_id, distinguishes instances */,
                    bool /* reached end of stream */)

IPC_MESSAGE_ROUTED4(MediaPlayerDelegateHostMsg_OnMediaMetadataChanged,
                    int /* delegate_id, distinguishes instances */,
                    bool /* has_audio */,
                    bool /* has_video */,
                    media::MediaContentType /* media_content_type */)

IPC_MESSAGE_ROUTED1(MediaPlayerDelegateHostMsg_OnMediaPlaying,
                    int /* delegate_id, distinguishes instances */)

IPC_MESSAGE_ROUTED2(
    MediaPlayerDelegateHostMsg_OnMediaEffectivelyFullscreenChanged,
    int /* delegate_id, distinguishes instances */,
    blink::WebFullscreenVideoStatus /* fullscreen_video_status */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnAudioOutputSinkChanged,
                    int /* delegate_id, distinguishes instances */,
                    std::string /* hashed_device_id */)

#endif  // CONTENT_COMMON_MEDIA_MEDIA_PLAYER_DELEGATE_MESSAGES_H_
