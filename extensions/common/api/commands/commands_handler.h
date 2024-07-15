// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_COMMANDS_COMMANDS_HANDLER_H_
#define EXTENSIONS_COMMON_API_COMMANDS_COMMANDS_HANDLER_H_

#include <memory>
#include <string>

#include "extensions/common/command.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct CommandsInfo : public Extension::ManifestData {
  CommandsInfo();
  ~CommandsInfo() override;

  // Optional list of commands (keyboard shortcuts).
  // These commands are the commands which the extension wants to use, which are
  // not necessarily the ones it can use, as it might be inactive (see also
  // Get*Command[s] in CommandService).
  std::unique_ptr<Command> browser_action_command;
  std::unique_ptr<Command> page_action_command;
  std::unique_ptr<Command> action_command;
  CommandMap named_commands;

  static const Command* GetBrowserActionCommand(const Extension* extension);
  static const Command* GetPageActionCommand(const Extension* extension);
  static const Command* GetActionCommand(const Extension* extension);
  static const CommandMap* GetNamedCommands(const Extension* extension);
};

// Parses the "commands" manifest key.
class CommandsHandler : public ManifestHandler {
 public:
  CommandsHandler();

  CommandsHandler(const CommandsHandler&) = delete;
  CommandsHandler& operator=(const CommandsHandler&) = delete;

  ~CommandsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  // If the extension defines an action (or browser action in manifest versions
  // prior to 3), but no command for it, then we synthesize a generic one, so
  // the user can configure a shortcut for it. No keyboard shortcut will be
  // assigned to it, until the user selects one. A generic command is not set
  // for extensions defining a page action.
  void MaybeSetActionDefault(const Extension* extension, CommandsInfo* info);

  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_COMMANDS_COMMANDS_HANDLER_H_
