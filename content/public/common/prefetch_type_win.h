// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PREFETCH_TYPE_WIN_H_
#define CONTENT_PUBLIC_COMMON_PREFETCH_TYPE_WIN_H_

namespace content {

// These are the App Launch PreFetch (ALPF) splits we do based on process
// type and subtype to differentiate file and offset usage within the files
// accessed by the different variations of the browser processes with the
// same process name.

enum class AppLaunchPrefetchType {
  kBrowser,
  kBrowserBackground,
  kCatchAll,
  kCrashpad,
  kExtension,
  kGPU,
  kGPUInfo,
  kPpapi,
  kRenderer,
  kUtilityAudio,
  kUtilityNetworkService,
  kUtilityStorage,
  kUtilityOther
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PREFETCH_TYPE_WIN_H_
