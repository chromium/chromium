// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the contents of a set of cache files, either
// to stdout or to another set of cache files.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_util.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/tools/dump_cache/dump_files.h"

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  FILE_ACCESS_ERROR,
  UNKNOWN_VERSION,
  TOOL_NOT_FOUND,
};

// Dumps the file headers to stdout.
const char kDumpHeaders[] = "dump-headers";

// Dumps all entries to stdout.
const char kDumpContents[] = "dump-contents";

// Dumps the LRU lists(s).
const char kDumpLists[] = "dump-lists";

// Dumps the entry at the given address (see kDumpAt).
const char kDumpEntry[] = "dump-entry";

// The cache address to dump.
const char kDumpAt[] = "at";

// Dumps the allocation bitmap of a file (see kDumpFile).
const char kDumpAllocation[] = "dump-allocation";

// The file to look at.
const char kDumpFile[] = "file";

int Help() {
  printf("dump_cache path_to_files [options]\n");
  printf("Dumps internal cache structures.\n");
  printf("warning: input files may be modified by this tool\n\n");
  printf("--dump-headers: show file headers\n");
  printf("--dump-contents [-v] [--full-key] [--csv]: list all entries\n");
  printf("--dump-lists: follow the LRU list(s)\n");
  printf(
      "--dump-entry [-v] [--full-key] --at=0xf00: show the data stored at"
      " 0xf00\n");
  printf(
      "--dump-allocation --file=data_0: show the allocation bitmap of"
      " data_0\n");
  printf("--csv: dump in a comma-separated-values format\n");
  printf(
      "--full-key: show up to 160 chars for the key. Use either -v or the"
      " key address for longer keys\n");
  printf("-v: detailed output (verbose)\n");
  return INVALID_ARGUMENT;
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destroyed.
  base::AtExitManager at_exit_manager;

  // base::UnlocalizedTimeFormatWithPattern() depends on ICU.
  base::i18n::InitializeICU();

  base::CommandLine::Init(argc, argv);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 1)
    return Help();

  base::FilePath input_path(args[0]);
  if (input_path.empty())
    return Help();

  if (!CheckFileVersion(input_path)) {
    return FILE_ACCESS_ERROR;
  }

  if (command_line.HasSwitch(kDumpContents))
    return DumpContents(input_path);

  if (command_line.HasSwitch(kDumpLists))
    return DumpLists(input_path);

  if (command_line.HasSwitch(kDumpEntry) && command_line.HasSwitch(kDumpAt))
    return DumpEntryAt(input_path, command_line.GetSwitchValueASCII(kDumpAt));

  if (command_line.HasSwitch(kDumpAllocation) &&
      command_line.HasSwitch(kDumpFile)) {
    base::FilePath name =
        input_path.AppendASCII(command_line.GetSwitchValueASCII(kDumpFile));
    return DumpAllocation(name);
  }

  if (command_line.HasSwitch(kDumpHeaders))
    return DumpHeaders(input_path);

  return Help();
}
