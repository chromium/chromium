// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESOURCE_MESSAGES_H_
#define CONTENT_COMMON_RESOURCE_MESSAGES_H_

// IPC messages for resource loading.
//
// WE ARE DEPRECATING THIS FILE. DO NOT ADD A NEW MESSAGE.
// NOTE: All messages must send an |int request_id| as their first parameter.

#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "content/common/content_param_traits_macros.h"
#include "content/common/navigation_params.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"

#ifndef INTERNAL_CONTENT_COMMON_RESOURCE_MESSAGES_H_
#define INTERNAL_CONTENT_COMMON_RESOURCE_MESSAGES_H_

#endif  // INTERNAL_CONTENT_COMMON_RESOURCE_MESSAGES_H_

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebMixedContentContextType,
                          blink::WebMixedContentContextType::kLast)

#endif  // CONTENT_COMMON_RESOURCE_MESSAGES_H_
