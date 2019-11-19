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
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerDelegateMsgStart

IPC_STRUCT_TRAITS_BEGIN(media_session::MediaPosition)
  IPC_STRUCT_TRAITS_MEMBER(playback_rate_)
  IPC_STRUCT_TRAITS_MEMBER(duration_)
  IPC_STRUCT_TRAITS_MEMBER(position_)
  IPC_STRUCT_TRAITS_MEMBER(last_updated_time_)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaContentType, media::MediaContentType::Max)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFullscreenVideoStatus,
                          blink::WebFullscreenVideoStatus::kMax)

// ----------------------------------------------------------------------------
// Messages from the browser to the renderer requesting playback state changes.
// ----------------------------------------------------------------------------

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_Pause,
                    int /* delegate_id, distinguishes instances */,
                    bool /* triggered_by_user */)

IPC_MESSAGE_ROUTED1(MediaPlayerDelegateMsg_Play,
                    int /* delegate_id, distinguishes instances */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_Muted,
                    int /* delegate_id, distinguishes instances */,
                    bool /* the new muted status */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_SeekForward,
                    int /* delegate_id, distinguishes instances */,
                    base::TimeDelta /* seek_time */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_SeekBackward,
                    int /* delegate_id, distinguishes instances */,
                    base::TimeDelta /* seek_time */)

IPC_MESSAGE_ROUTED0(MediaPlayerDelegateMsg_SuspendAllMediaPlayers)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_UpdateVolumeMultiplier,
                    int /* delegate_id, distinguishes instances */,
                    double /* multiplier */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_BecamePersistentVideo,
                    int /* delegate_id, distinguishes instances */,
                    double /* is_persistent */)

// ----------------------------------------------------------------------------
// Messages from the renderer notifying the browser of playback state changes.
// ----------------------------------------------------------------------------

IPC_MESSAGE_ROUTED1(MediaPlayerDelegateHostMsg_OnMediaDestroyed,
                    int /* delegate_id, distinguishes instances */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnMediaPaused,
                    int /* delegate_id, distinguishes instances */,
                    bool /* reached end of stream */)

IPC_MESSAGE_ROUTED5(MediaPlayerDelegateHostMsg_OnMediaPlaying,
                    int /* delegate_id, distinguishes instances */,
                    bool /* has_video */,
                    bool /* has_audio */,
                    bool /* is_remote */,
                    media::MediaContentType /* media_content_type */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnMutedStatusChanged,
                    int /* delegate_id, distinguishes instances */,
                    bool /* the new muted status */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnMediaPositionStateChanged,
                    int /* delegate_id, distinguishes instances */,
                    media_session::MediaPosition /* the new position state */)

IPC_MESSAGE_ROUTED2(
    MediaPlayerDelegateHostMsg_OnMediaEffectivelyFullscreenChanged,
    int /* delegate_id, distinguishes instances */,
    blink::WebFullscreenVideoStatus /* fullscreen_video_status */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnMediaSizeChanged,
                    int /* delegate_id, distinguishes instances */,
                    gfx::Size /* new size of video */)

#endif  // CONTENT_COMMON_MEDIA_MEDIA_PLAYER_DELEGATE_MESSAGES_H_
