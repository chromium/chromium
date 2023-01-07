// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/content_decoder_tool/content_decoder_tool.h"

#include <iostream>
#include <memory>
#include <vector>

#include "base/command_line.h"

namespace {

// Print the command line help.
void PrintHelp(const char* command_line_name) {
  std::cout << command_line_name << " content_encoding [content_encoding]..."
            << std::endl
            << std::endl;
  std::cout << "Decodes the stdin into the stdout using an content_encoding "
            << "list given in arguments. This list is expected to be the "
            << "Content-Encoding HTTP response header's value split by ','."
            << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::vector<std::string> content_encodings = command_line.GetArgs();
  if (content_encodings.size() == 0) {
    PrintHelp(argv[0]);
    return 1;
  }
  return !net::ContentDecoderToolProcessInput(content_encodings, &std::cin,
                                              &std::cout);
}
