// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// NOLINT(build/header_guard)

#include "build/build_config.h"
#undef CONTENT_COMMON_FRAME_MESSAGES_H_
#include "content/common/frame_messages.h"
#ifndef CONTENT_COMMON_FRAME_MESSAGES_H_
#error "Failed to include content/common/frame_messages.h"
#endif
#include "media/media_buildflags.h"

#if defined(OS_ANDROID)
#undef CONTENT_COMMON_GIN_JAVA_BRIDGE_MESSAGES_H_
#include "content/common/gin_java_bridge_messages.h"
#ifndef CONTENT_COMMON_GIN_JAVA_BRIDGE_MESSAGES_H_
#error "Failed to include content/common/gin_java_bridge_messages.h"
#endif
#endif  // defined(OS_ANDROID)
