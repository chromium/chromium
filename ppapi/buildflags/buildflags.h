// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_BUILDFLAGS_BUILDFLAGS_H_
#define PPAPI_BUILDFLAGS_BUILDFLAGS_H_

#include "build/blink_buildflags.h"
#include "build/buildflag.h"  // IWYU pragma: export

#if BUILDFLAG(USE_BLINK)
#include "content/public/common/buildflags.h"
#endif

// IWYU pragma: always_keep

// The ENABLE_PLUGINS build flag is moved into content/public/common but
// many files still include this file, so we statically generate it with
// an include of the content flag generation until all files are adjusted.
#define BUILDFLAG_INTERNAL_ENABLE_PPAPI() (0)

#endif  // PPAPI_BUILDFLAGS_BUILDFLAGS_H_
