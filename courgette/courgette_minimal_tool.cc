// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 'courgette_minimal_tool' is not meant to be a serious command-line tool.  It
// has the minimum logic to apply a Courgette patch to a file.  The main purpose
// is to monitor the code size of the patcher.

#include <string>

#include "build/build_config.h"
#include "courgette/courgette.h"
#include "courgette/streams.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

void PrintHelp() {
  fprintf(stderr,
    "Usage:\n"
    "  courgette_minimal_tool <old-file-input> <patch-file-input>"
    " <new-file-output>\n"
    "\n");
}

void UsageProblem(const char* message) {
  fprintf(stderr, "%s\n", message);
  PrintHelp();
  exit(1);
}

void Problem(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(1);
}

#if BUILDFLAG(IS_WIN)
int wmain(int argc, const wchar_t* argv[]) {
#else
int main(int argc, const char* argv[]) {
#endif
  if (argc != 4)
    UsageProblem("bad args");

  courgette::Status status =
      courgette::ApplyEnsemblePatch(argv[1], argv[2], argv[3]);

  if (status != courgette::C_OK) {
    if (status == courgette::C_READ_OPEN_ERROR) Problem("Can't open file.");
    if (status == courgette::C_WRITE_OPEN_ERROR) Problem("Can't open file.");
    if (status == courgette::C_READ_ERROR) Problem("Can't read from file.");
    if (status == courgette::C_WRITE_ERROR) Problem("Can't write to file.");
    Problem("patch failed.");
  }

  return 0;
}
