// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_PREFS_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_PREFS_H_

#include <memory>

class PrefService;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// Support for preference initialization and management.
namespace shell_prefs {

// Creates a pref service for device-wide preferences stored in |data_dir|.
std::unique_ptr<PrefService> CreateLocalState(const base::FilePath& data_dir);

// Creates a pref service that loads user preferences for |browser_context|.
std::unique_ptr<PrefService> CreateUserPrefService(
    content::BrowserContext* browser_context);

}  // namespace shell_prefs

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_PREFS_H_
