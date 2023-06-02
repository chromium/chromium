// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_PASSKEYS_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_PASSKEYS_H_

#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"

namespace screen_ai {
class ScreenAIServiceRouter;
}  // namespace screen_ai

namespace chrome {
class FileUtilServiceLauncher;
}  // namespace chrome

namespace content {
class ServiceProcessHostPreloadLibraries {
 public:
  using PassKey = base::PassKey<ServiceProcessHostPreloadLibraries>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Service launchers using `ServiceProcessHost::Options::WithPreloadLibraries`
  // should be added here and must be reviewed by the security team.
  friend class screen_ai::ScreenAIServiceRouter;

  // Tests.
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryPreloaded);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryMultiple);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryModName);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryBadPath);
};

class ServiceProcessHostPinUser32 {
 public:
  using PassKey = base::PassKey<ServiceProcessHostPinUser32>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Service launchers using `ServiceProcessHost::Options::WithPinUser32`
  // should be added here and must be reviewed by the security team.
  friend class chrome::FileUtilServiceLauncher;

  // Tests.
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest, PinUser32);
};
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_PASSKEYS_H_
