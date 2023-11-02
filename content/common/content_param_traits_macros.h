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
#include "ipc/ipc_message_macros.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(content::NavigationGesture,
                          content::NavigationGestureLast)

#if BUILDFLAG(IS_MAC)
IPC_ENUM_TRAITS_MAX_VALUE(blink::ScrollerStyle, blink::kScrollerStyleOverlay)
#endif

IPC_ENUM_TRAITS_MAX_VALUE(ui::NativeTheme::SystemThemeColor,
                          ui::NativeTheme::SystemThemeColor::kMaxValue)

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
