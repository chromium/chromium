// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PLATFORM_FEATURES_H_
#define MEDIA_BASE_PLATFORM_FEATURES_H_

#include "build/build_config.h"
#include "media/base/media_export.h"

// vvvvvvvvvvvvvvvvvvvvvvvvvvv READ ME FIRST vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
//
// This file collects a list of media features that are enabled on a
// per-platform basis.  This allows us to reduce the number of active
// base::Features for shipped features.
//
// For each feature, define a function that returns enabled status depending on
// the platform.  Here are some examples:
//
// 1. Permanently enabled on some platforms and disabled on others.
//
// platform_features.h:
//
// MEDIA_EXPORT constexpr bool IsFooEnabled() {
// #if BUILDFLAG(IS_ANDROID)
//   return true;
// #else
//   return false;
// #endif  // BUILDFLAG(IS_ANDROID)
// }
//
// 2. Permanently enabled on some platforms, in the process of shipping on other
//    platforms.
//
// platform_features.h:
//
// MEDIA_EXPORT bool IsFooEnabled();
//
// platform_features.cc:
//
// bool IsFooEnabled() {
// #if BUILDFLAG(IS_ANDROID)
//   return true;
// #else
//   // TODO(crbug.com/XXX): Default-enable for all platforms.
//   return base::FeatureList::IsEnabled(kFoo);
// #endif  // BUILDFLAG(IS_ANDROID)
// }
//
// The logic for determining feature availability should be simple and based on
// a combination of build flags and base::Feature values.  More complex and/or
// runtime feature detection does not belong here.
//
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^ READ ME FIRST ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

namespace media {

MEDIA_EXPORT bool IsVp9kSVCHWDecodingEnabled();

MEDIA_EXPORT constexpr bool IsVp9kSVCHWEncodingEnabled() {
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

}  // namespace media

#endif  // MEDIA_BASE_PLATFORM_FEATURES_H_
