// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/navigation_gesture.h"
#include "ipc/param_traits.h"
#include "ipc/param_traits_macros.h"
#include "ipc/param_traits_utils.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(content::NavigationGesture,
                          content::NavigationGestureLast)

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
