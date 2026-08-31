// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_MACHINE_DEAL_WIN_H_
#define RLZ_LIB_MACHINE_DEAL_WIN_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>

#include "rlz/lib/rlz_api.h"

// OEM Deal confirmation storage functions.

namespace rlz_lib {

// The maximum length of an access points RLZ in bytes.
const size_t kMaxDccLength = 128;

// Makes the OEM Deal Confirmation code writable by all users on the machine.
// This should be called before calling SetMachineDealCode from a non-admin
// account.
// Access: HKLM write.
bool RLZ_LIB_API CreateMachineState(void);

// Set the OEM Deal Confirmation Code (DCC). This information is used for RLZ
// initialization.
// Access: HKLM write, or
// HKCU read if rlz_lib::CreateMachineState() has been successfully called.
bool RLZ_LIB_API SetMachineDealCode(std::string_view dcc);

// Get the DCC cgi argument string to append to a daily ping. Returns
// std::nullopt if the DCC is missing or empty.
// Should be used only by OEM deal trackers. Applications should use the
// GetMachineDealCode method which has an AccessPoint parameter.
// Access: HKLM read.
std::optional<std::string> RLZ_LIB_API GetMachineDealCodeAsCgi();

// Get the DCC value stored in registry. Returns std::nullopt if the value is
// missing or empty.
// Should be used only by OEM deal trackers. Applications should use the
// GetMachineDealCode method which has an AccessPoint parameter.
// Access: HKLM read.
std::optional<std::string> RLZ_LIB_API GetMachineDealCode();

// Parses a ping response, checks if it is valid and sets the machine DCC
// from the response. The ping must also contain the current DCC value in
// order to be considered valid.
// Access: HKLM write;
//         HKCU write if CreateMachineState() has been successfully called.
bool RLZ_LIB_API SetMachineDealCodeFromPingResponse(std::string_view response);

}  // namespace rlz_lib

#endif  // RLZ_LIB_MACHINE_DEAL_WIN_H_
