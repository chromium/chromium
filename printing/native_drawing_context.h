// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_DRAWING_CONTEXT_H_
#define PRINTING_NATIVE_DRAWING_CONTEXT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
typedef struct CGContext* CGContextRef;
#endif

namespace printing {

#if defined(OS_WIN)
typedef HDC NativeDrawingContext;
#elif defined(OS_MACOSX)
typedef CGContextRef NativeDrawingContext;
#else
typedef void* NativeDrawingContext;
#endif

}  // namespace printing

#endif  // PRINTING_NATIVE_DRAWING_CONTEXT_H_
