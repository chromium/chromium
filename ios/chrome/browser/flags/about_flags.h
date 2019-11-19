// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#ifndef IOS_CHROME_BROWSER_FLAGS_ABOUT_FLAGS_H_
#define IOS_CHROME_BROWSER_FLAGS_ABOUT_FLAGS_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "components/flags_ui/flags_state.h"

namespace base {
class CommandLine;
class ListValue;
}  // namespace base

namespace flags_ui {
class FlagsStorage;
}

// Reads the state from |flags_storage| and adds the command line flags
// belonging to the active feature entries to |command_line| in addition
// to the flags from experimental settings.
void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line);

// Registers variations parameter values selected for features in about:flags.
// The selected flags are retrieved from |flags_storage|, the registered
// variation parameters are connected to their corresponding features in
// |feature_list|. Returns the (possibly empty) list of additional variation ids
// to register in the MetricsService that come from variations selected using
// chrome://flags.
std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list);

// Gets the list of feature entries. Entries that are available for the current
// platform are appended to |supported_entries|; all other entries are appended
// to |unsupported_entries|.
void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries);

// Enables or disables the current with id |internal_name|.
void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable);

// Reset all flags to the default state by clearing all flags.
void ResetAllFlags(flags_ui::FlagsStorage* flags_storage);

namespace testing {

// Returns the global set of feature entries.
const flags_ui::FeatureEntry* GetFeatureEntries(size_t* count);

}  // namespace testing

#endif  // IOS_CHROME_BROWSER_FLAGS_ABOUT_FLAGS_H_
