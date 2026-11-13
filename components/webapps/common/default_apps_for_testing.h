// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_COMMON_DEFAULT_APPS_FOR_TESTING_H_
#define COMPONENTS_WEBAPPS_COMMON_DEFAULT_APPS_FOR_TESTING_H_

#include "base/auto_reset.h"
#include "base/functional/callback_helpers.h"

namespace webapps {

// Default apps, i.e. preinstalled web apps and default extension apps, are
// disabled if the --disable-default-apps command line switch is set. Unit
// tests can also disable them with `DisableDefaultAppsForTesting()`.
// These functions must only be called on the UI thread.

// Disables default apps until the returned object is destroyed.
[[nodiscard]] base::ScopedClosureRunner DisableDefaultAppsForTesting();

// Enables default apps again until the returned object is destroyed, e.g. for
// tests of default apps which use a `FakeWebAppProvider`, which disables them.
// The --disable-default-apps command line switch still disables them.
[[nodiscard]] base::AutoReset<bool> EnableDefaultAppsForTesting();

// Returns whether default apps are disabled by
// `DisableDefaultAppsForTesting()`.
bool AreDefaultAppsDisabledForTesting();

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_COMMON_DEFAULT_APPS_FOR_TESTING_H_
