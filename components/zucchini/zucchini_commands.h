// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_COMMANDS_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_COMMANDS_H_

#include <iosfwd>
#include <vector>

#include "base/files/file_path.h"
#include "components/zucchini/zucchini.h"

// Zucchini commands and tools that can be invoked from command-line.

namespace base {

class CommandLine;

}  // namespace base

// Aggregated parameter for Main*() functions, to simplify interface.
struct MainParams {
  const base::CommandLine& command_line;
  const std::vector<base::FilePath>& file_paths;
  std::ostream& out;
  std::ostream& err;
};

// Signature of a Zucchini Command Function.
using CommandFunction = zucchini::status::Code (*)(MainParams);

// Command Function: Patch generation.
zucchini::status::Code MainGen(MainParams params);

// Command Function: Patch application.
zucchini::status::Code MainApply(MainParams params);

// Command Function: Verify patch format and compatibility.
zucchini::status::Code MainVerify(MainParams params);

// Command Function: Read and dump references from an executable.
zucchini::status::Code MainRead(MainParams params);

// Command Function: Scan an archive file and detect executables.
zucchini::status::Code MainDetect(MainParams params);

// Command Function: Scan two archive files and match detected executables.
zucchini::status::Code MainMatch(MainParams params);

// Command Function: Compute CRC-32 of a file.
zucchini::status::Code MainCrc32(MainParams params);

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_COMMANDS_H_
