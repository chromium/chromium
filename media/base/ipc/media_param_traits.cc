// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ipc/media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "ipc/param_traits_utils.h"
#include "media/base/encryption_scheme.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC
