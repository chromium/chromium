// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_HANDLE_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_HANDLE_H_

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/zygote/zygote_buildflags.h"

#if !BUILDFLAG(USE_ZYGOTE_HANDLE)
#error "Can not use zygote handles without USE_ZYGOTE_HANDLE"
#endif

namespace content {

#if BUILDFLAG(IS_POSIX)
class ZygoteCommunication;
using ZygoteHandle = ZygoteCommunication*;
#else
// Perhaps other ports may USE_ZYGOTE_HANDLE here somdeday.
#error "Can not use zygote handles on this platform"
#endif  // BUILDFLAG(IS_POSIX)

// Gets the generic global zygote used to launch sandboxed children.
CONTENT_EXPORT ZygoteHandle GetGenericZygote();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_HANDLE_H_
