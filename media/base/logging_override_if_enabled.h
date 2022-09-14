// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LOGGING_OVERRIDE_IF_ENABLED_H_
#define MEDIA_BASE_LOGGING_OVERRIDE_IF_ENABLED_H_

// Provides a way to override DVLOGs to at build time.
// Warning: Do NOT include this file in .h files to avoid unexpected override.
// TODO(xhwang): Provide a way to choose which |verboselevel| to override.

#include "build/build_config.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_LOGGING_OVERRIDE)
#if !defined(DVLOG)
#error This file must be included after base/logging.h.
#endif

#if BUILDFLAG(IS_FUCHSIA)

#define __DVLOG_0 VLOG(0)
#define __DVLOG_1 VLOG(1)
#define __DVLOG_2 VLOG(2)

#else

#define __DVLOG_0 LOG(INFO)
#define __DVLOG_1 LOG(INFO)
#define __DVLOG_2 LOG(INFO)

#endif  // BUILDFLAG(IS_FUCHSIA)

#define __DVLOG_3 EAT_STREAM_PARAMETERS
#define __DVLOG_4 EAT_STREAM_PARAMETERS
#define __DVLOG_5 EAT_STREAM_PARAMETERS

#undef DVLOG
#define DVLOG(verboselevel) __DVLOG_##verboselevel

#endif  // BUILDFLAG(ENABLE_LOGGING_OVERRIDE)

#endif  // MEDIA_BASE_LOGGING_OVERRIDE_IF_ENABLED_H_
