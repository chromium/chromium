// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_switches.h"

#if 0
namespace switches {
#if defined(OS_ANDROID)
// Enables the testing interface for PPAPI.
CONTENT_EXPORT const char kEnablePepperTesting[] = "enable-pepper-testing";
CONTENT_EXPORT const char kFilesDir[] = "files-dir";
CONTENT_EXPORT const char kEcLibDir[] = "ec-lib-dir";
CONTENT_EXPORT const char kEcWwwDir[] = "ec-www-dir";
#else
const char kEnablePepperTesting[] = "enable-pepper-testing";
const char kFilesDir[] = "files-dir";
const char kEcLibDir[] = "ec-lib-dir";
const char kEcWwwDir[] = "ec-www-dir";
#endif
}  // namespace switches
#endif
