// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ACTIVE_INSTALL_DATA_H_
#define EXTENSIONS_BROWSER_ACTIVE_INSTALL_DATA_H_

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Details of an active extension install.
struct ActiveInstallData {
  ActiveInstallData() = default;
  explicit ActiveInstallData(const ExtensionId& extension_id);

  ExtensionId extension_id;
  int percent_downloaded = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ACTIVE_INSTALL_DATA_H_
