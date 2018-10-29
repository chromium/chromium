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

#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/media_content_type.h"
#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerDelegateMsgStart

IPC_STRUCT_TRAITS_BEGIN(blink::PictureInPictureControlInfo::Icon)
  IPC_STRUCT_TRAITS_MEMBER(src)
  IPC_STRUCT_TRAITS_MEMBER(sizes)
  IPC_STRUCT_TRAITS_MEMBER(type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::PictureInPictureControlInfo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(icons)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaContentType, media::MediaContentType::Max)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFullscreenVideoStatus,
                          blink::WebFullscreenVideoStatus::kMax)

// ----------------------------------------------------------------------------
// Messages from the browser to the renderer requesting playback state changes.
// ----------------------------------------------------------------------------

IPC_MESSAGE_ROUTED1(MediaPlayerDelegateMsg_Pause,
                    int /* delegate_id, distinguishes instances */)

IPC_MESSAGE_ROUTED1(MediaPlayerDelegateMsg_Play,
                    int /* delegate_id, distinguishes instances */)

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

IPC_MESSAGE_ROUTED1(MediaPlayerDelegateMsg_EndPictureInPictureMode,
                    int /* delegate_id, distinguishes instances */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_ClickPictureInPictureControl,
                    int /* delegate_id, distinguishes instances */,
                    std::string /* control_id */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_OnPictureInPictureWindowResize,
                    int /* delegate_id, distinguishes instances */,
                    gfx::Size /* window_size */)

// ----------------------------------------------------------------------------
// Messages from the browser to the renderer acknowledging changes happened.
// ----------------------------------------------------------------------------

IPC_MESSAGE_ROUTED3(MediaPlayerDelegateMsg_OnPictureInPictureModeStarted_ACK,
                    int /* delegate id */,
                    int /* request_id */,
                    gfx::Size /* window_size */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_OnPictureInPictureModeEnded_ACK,
                    int /* delegate id */,
                    int /* request_id */)

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

IPC_MESSAGE_ROUTED2(
    MediaPlayerDelegateHostMsg_OnMediaEffectivelyFullscreenChanged,
    int /* delegate_id, distinguishes instances */,
    blink::WebFullscreenVideoStatus /* fullscreen_video_status */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnMediaSizeChanged,
                    int /* delegate_id, distinguishes instances */,
                    gfx::Size /* new size of video */)

IPC_MESSAGE_ROUTED5(MediaPlayerDelegateHostMsg_OnPictureInPictureModeStarted,
                    int /* delegate id */,
                    viz::SurfaceId /* surface_id */,
                    gfx::Size /* natural_size */,
                    int /* request_id */,
                    bool /* show_play_pause_button */)

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateHostMsg_OnPictureInPictureModeEnded,
                    int /* delegate id */,
                    int /* request_id */)

IPC_MESSAGE_ROUTED4(MediaPlayerDelegateHostMsg_OnPictureInPictureSurfaceChanged,
                    int /* delegate id */,
                    viz::SurfaceId /* surface_id */,
                    gfx::Size /* natural_size */,
                    bool /* show_play_pause_button */)

IPC_MESSAGE_ROUTED2(
    MediaPlayerDelegateHostMsg_OnSetPictureInPictureCustomControls,
    int /* delegate id */,
    std::vector<blink::PictureInPictureControlInfo> /* custom controls */)

#endif  // CONTENT_COMMON_MEDIA_MEDIA_PLAYER_DELEGATE_MESSAGES_H_
