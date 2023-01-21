// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_HANDLE_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_HANDLE_H_

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/zygote/zygote_buildflags.h"

#if !BUILDFLAG(USE_ZYGOTE)
#error "Can not use zygote without USE_ZYGOTE"
#endif

namespace content {

#if BUILDFLAG(IS_POSIX)
class ZygoteCommunication;
#else
// Perhaps other ports may USE_ZYGOTE here somdeday.
#error "Can not use zygote on this platform"
#endif  // BUILDFLAG(IS_POSIX)

// Gets the generic global zygote used to launch sandboxed children.
CONTENT_EXPORT ZygoteCommunication* GetGenericZygote();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_HANDLE_H_
