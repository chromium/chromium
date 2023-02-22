// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_DRAWING_CONTEXT_H_
#define PRINTING_NATIVE_DRAWING_CONTEXT_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#elif BUILDFLAG(IS_APPLE)
typedef struct CGContext* CGContextRef;
#endif

namespace printing {

#if BUILDFLAG(IS_WIN)
typedef HDC NativeDrawingContext;
#elif BUILDFLAG(IS_APPLE)
typedef CGContextRef NativeDrawingContext;
#else
typedef void* NativeDrawingContext;
#endif

}  // namespace printing

#endif  // PRINTING_NATIVE_DRAWING_CONTEXT_H_
