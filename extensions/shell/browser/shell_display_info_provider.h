// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_

#include "extensions/browser/display_info_provider_base.h"

namespace extensions {

class ShellDisplayInfoProvider : public DisplayInfoProviderBase {
 public:
  ShellDisplayInfoProvider();

  ShellDisplayInfoProvider(const ShellDisplayInfoProvider&) = delete;
  ShellDisplayInfoProvider& operator=(const ShellDisplayInfoProvider&) = delete;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_
