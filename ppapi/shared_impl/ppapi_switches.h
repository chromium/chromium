// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_SWITCHES_H_
#define PPAPI_SHARED_IMPL_PPAPI_SWITCHES_H_

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace switches {
#if defined(OS_ANDROID)
PPAPI_SHARED_EXPORT extern const char kEnablePepperTesting[];
PPAPI_SHARED_EXPORT extern const char kFilesDir[];
PPAPI_SHARED_EXPORT extern const char kEcLibDir[];
PPAPI_SHARED_EXPORT extern const char kEcWwwDir[];
#else
extern const char kEnablePepperTesting[];
extern const char kFilesDir[];
extern const char kEcLibDir[];
extern const char kEcWwwDir[];
#endif

}  // namespace switches

#endif  // PPAPI_SHARED_IMPL_PPAPI_SWITCHES_H_
