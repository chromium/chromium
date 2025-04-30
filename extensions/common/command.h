// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_COMMAND_H_
#define EXTENSIONS_COMMON_COMMAND_H_

#include <map>
#include <string>
#include <string_view>

#include "base/values.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

namespace extensions {

class Command : public ui::Command {
 public:
  Command() = default;
  Command(std::string_view command_name,
          std::u16string_view description,
          std::string_view accelerator,
          bool global);
  Command(const Command& other) = default;
  ~Command() override = default;

  // The platform value for the Command.
  static std::string CommandPlatform();

  // Parse a string as an accelerator. If the accelerator is unparsable then
  // a generic ui::Accelerator object will be returns (with key_code Unknown).
  static ui::Accelerator StringToAccelerator(std::string_view accelerator,
                                             std::string_view command_name);

  // Return true if the `command_name` is one of the following action events:
  // Action Command Event, Browser Action Command Event, Page Action Command
  // Event.
  static bool IsActionRelatedCommand(std::string_view command_name);

  // Parse the command.
  bool Parse(const base::Value::Dict& command,
             std::string_view command_name,
             int index,
             std::u16string* error);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_COMMAND_H_
