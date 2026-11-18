// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#ifndef IOS_CHROME_BROWSER_FLAGS_ABOUT_FLAGS_H_
#define IOS_CHROME_BROWSER_FLAGS_ABOUT_FLAGS_H_

#include <stddef.h>

#include <string>

#include "base/values.h"
#include "components/webui/flags/flags_state.h"

namespace base {
class CommandLine;
}  // namespace base

namespace flags_ui {
class FlagsStorage;
}

// Returns the global `flags_ui::FlagsState` instance for iOS.
flags_ui::FlagsState& GetGlobalFlagsState();

// Adds all switches from experimental flags to `command_line`.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line);

// Gets the list of feature entries. Entries that are available for the current
// platform are appended to `supported_entries`; all other entries are appended
// to `unsupported_entries`.
void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue& supported_entries,
                           base::ListValue& unsupported_entries);

// Enables or disables the feature with `internal_name` as id.
void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable);

// Reset all flags to the default state by clearing all flags.
void ResetAllFlags(flags_ui::FlagsStorage* flags_storage);

// Returns true if a browser restart is needed for uncommitted changes to take
// effect.
bool IsRestartNeededToCommitChanges();

namespace testing {

// Returns the global set of feature entries.
base::span<const flags_ui::FeatureEntry> GetFeatureEntries();

}  // namespace testing

#endif  // IOS_CHROME_BROWSER_FLAGS_ABOUT_FLAGS_H_
