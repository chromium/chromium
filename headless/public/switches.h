// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_SWITCHES_H_
#define HEADLESS_PUBLIC_SWITCHES_H_

#include "headless/public/headless_export.h"

namespace headless {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

HEADLESS_EXPORT extern const char kAcceptLang[];
HEADLESS_EXPORT extern const char kAuthServerAllowlist[];
HEADLESS_EXPORT extern const char kBlockNewWebContents[];
HEADLESS_EXPORT extern const char kCrashDumpsDir[];
HEADLESS_EXPORT extern const char kDeterministicMode[];
HEADLESS_EXPORT extern const char kDisableCookieEncryption[];
HEADLESS_EXPORT extern const char kDisableCrashReporter[];
HEADLESS_EXPORT extern const char kDisableLazyLoading[];
HEADLESS_EXPORT extern const char kDiskCacheDir[];
HEADLESS_EXPORT extern const char kEnableBeginFrameControl[];
HEADLESS_EXPORT extern const char kEnableCrashReporter[];
HEADLESS_EXPORT extern const char kEnableGPU[];
HEADLESS_EXPORT extern const char kExplicitlyAllowedPorts[];
HEADLESS_EXPORT extern const char kFontRenderHinting[];
HEADLESS_EXPORT extern const char kIncognito[];
HEADLESS_EXPORT extern const char kNoSystemProxyConfigService[];
HEADLESS_EXPORT extern const char kPasswordStore[];
HEADLESS_EXPORT extern const char kProxyBypassList[];
HEADLESS_EXPORT extern const char kProxyServer[];
HEADLESS_EXPORT extern const char kRemoteDebuggingAddress[];
HEADLESS_EXPORT extern const char kUserAgent[];
HEADLESS_EXPORT extern const char kUserDataDir[];
HEADLESS_EXPORT extern const char kWindowSize[];

}  // namespace switches
}  // namespace headless

#endif  // HEADLESS_PUBLIC_SWITCHES_H_
