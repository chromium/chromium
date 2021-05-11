// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGES_H_
#define CONTENT_COMMON_FRAME_MESSAGES_H_

// IPC messages for interacting with frames.

#include <stddef.h>
#include <stdint.h>

#include "content/common/buildflags.h"
#include "content/common/common_param_traits_macros.h"
#include "content/common/content_export.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "third_party/blink/public/common/navigation/impression.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_STRUCT_TRAITS_BEGIN(blink::Impression)
  IPC_STRUCT_TRAITS_MEMBER(conversion_destination)
  IPC_STRUCT_TRAITS_MEMBER(reporting_origin)
  IPC_STRUCT_TRAITS_MEMBER(impression_data)
  IPC_STRUCT_TRAITS_MEMBER(expiry)
  IPC_STRUCT_TRAITS_MEMBER(priority)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_FRAME_MESSAGES_H_
