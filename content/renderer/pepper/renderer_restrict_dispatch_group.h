// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RENDERER_RESTRICT_DISPATCH_GROUP_H_
#define CONTENT_RENDERER_PEPPER_RENDERER_RESTRICT_DISPATCH_GROUP_H_

#include "ipc/ipc_sync_channel.h"

namespace content {

// This represents all dispatch groups used in the renderer. Dispatch groups
// allow channels to restrict in which case incoming messages can re-enter while
// a synchronous message is sent on another channel. See
// IPC::SyncChannel::SetRestrictDispatchChannelGroup.
enum RendererRestrictDispatchGroup {
  kRendererRestrictDispatchGroup_None =
      IPC::SyncChannel::kRestrictDispatchGroup_None,
  kRendererRestrictDispatchGroup_Pepper,
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RENDERER_RESTRICT_DISPATCH_GROUP_H_
