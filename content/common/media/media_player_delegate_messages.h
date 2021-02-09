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
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerDelegateMsgStart

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

IPC_MESSAGE_ROUTED2(MediaPlayerDelegateMsg_NotifyPowerExperimentState,
                    int /* delegate_id, distinguishes instances */,
                    bool /* is experiment starting (true) or stopping? */)

#endif  // CONTENT_COMMON_MEDIA_MEDIA_PLAYER_DELEGATE_MESSAGES_H_
