// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Library functions related to the OEM Deal Confirmation Code.

#ifndef RLZ_WIN_LIB_MACHINE_DEAL_H_
#define RLZ_WIN_LIB_MACHINE_DEAL_H_

#include <optional>
#include <string>
#include <string_view>

#include "rlz/lib/rlz_enums.h"

namespace rlz_lib {

class MachineDealCode {
 public:
  // Set the OEM Deal Confirmation Code (DCC). This information is used for RLZ
  // initalization. Must have write access to HKLM - SYSTEM or admin, unless
  // rlz_lib::CreateMachineState() has been successfully called.
  static bool Set(std::string_view dcc);

  // Parses a ping response, checks if it is valid and sets the machine DCC
  // from the response. The response should also contain the current value of
  // the DCC to be considered valid.
  // Write access to HKLM (system / admin) needed, unless
  // rlz_lib::CreateMachineState() has been successfully called.
  static bool SetFromPingResponse(std::string_view response);

  // Gets the new DCC to set from a ping response. Returns true if the ping
  // response is valid. Sets has_new_dcc true if there is a new DCC value.
  static bool GetNewCodeFromPingResponse(std::string_view response,
                                         bool* has_new_dcc,
                                         std::string* new_dcc);

  // Get the DCC cgi argument string to append to a daily or financial ping.
  // Returns std::nullopt if the DCC is missing or empty.
  static std::optional<std::string> GetAsCgi();

  // Get the machine code. Returns std::nullopt if missing or empty.
  static std::optional<std::string> Get();

 protected:
  // Clear the DCC value. Only for testing purposes.
  // Requires write access to HKLM, unless rlz_lib::CreateMachineState() has
  // been successfully called.
  static bool Clear();

  MachineDealCode() {}
  ~MachineDealCode() {}
};

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_MACHINE_DEAL_H_
