// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_PLATFORM_H_
#define DEVICE_VR_OPENXR_OPENXR_PLATFORM_H_

// This file exists because the openxr provided openxr_platform header requires
// the presence of certain platform-specific includes before it is included.
// In order to ensure that we maintain those includes properly, Chrome code
// should ensure that it includes this file rather than the third_party version
// directly. This ifdef check is one way that we intend to enforce that.
#ifdef OPENXR_PLATFORM_H_
#error "Do not directly include the external openxr_platform.h"
#endif

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#elif BUILDFLAG(IS_ANDROID)
#include <EGL/egl.h>
#include <jni.h>
#endif

#include "third_party/openxr/src/include/openxr/openxr_platform.h"

#endif  // DEVICE_VR_OPENXR_OPENXR_PLATFORM_H_
