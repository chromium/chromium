// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_MIGRATION_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_MIGRATION_H_

#include <string>

namespace base {
class FilePath;
}  // namespace base

namespace sessions {
class TabRestoreService;
}  // namespace sessions

namespace ios::sessions {

// Migrates session named `name` in `path` from legacy to optimized.
//
// If the legacy session was successfully loaded, but could not be
// converted (e.g. the disk is full and new file cannot be written),
// then `restore_service` is used to record the tabs as recently
// closed.
//
// In all case, any files corresponding to the legacy storage are
// deleted when the migration completes, either successfully or with
// error.
void MigrateNamedSessionToOptimized(
    const base::FilePath& path,
    const std::string& name,
    ::sessions::TabRestoreService* restore_service);

// Migrates session named `name` in `path` from optimized to legacy.
//
// If the optimized session was successfully loaded, but could not be
// converted (e.g. the disk is full and new file cannot be written),
// then `restore_service` is used to record the tabs as recently
// closed.
//
// In all case, any files corresponding to the optimized storage are
// deleted when the migration completes, either successfully or with
// error.
void MigrateNamedSessionToLegacy(
    const base::FilePath& directory,
    const std::string& name,
    ::sessions::TabRestoreService* restore_service);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_MIGRATION_H_
