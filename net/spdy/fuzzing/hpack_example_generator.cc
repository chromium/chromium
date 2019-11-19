// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/spdy/fuzzing/hpack_fuzz_util.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_encoder.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace {

// Target file for generated HPACK header sets.
const char kFileToWrite[] = "file-to-write";

// Number of header sets to generate.
const char kExampleCount[] = "example-count";

}  // namespace

using spdy::HpackFuzzUtil;
using std::map;

// Generates a configurable number of header sets (using HpackFuzzUtil), and
// sequentially encodes each header set with an HpackEncoder. Encoded header
// sets are written to the output file in length-prefixed blocks.
int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(kFileToWrite) ||
      !command_line.HasSwitch(kExampleCount)) {
    LOG(ERROR) << "Usage: " << argv[0] << " --" << kFileToWrite
               << "=/path/to/file.out"
               << " --" << kExampleCount << "=1000";
    return -1;
  }
  std::string file_to_write = command_line.GetSwitchValueASCII(kFileToWrite);

  int example_count = 0;
  base::StringToInt(command_line.GetSwitchValueASCII(kExampleCount),
                    &example_count);

  DVLOG(1) << "Writing output to " << file_to_write;
  base::File file_out(base::FilePath::FromUTF8Unsafe(file_to_write),
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  CHECK(file_out.IsValid()) << file_out.error_details();

  HpackFuzzUtil::GeneratorContext context;
  HpackFuzzUtil::InitializeGeneratorContext(&context);
  spdy::HpackEncoder encoder(spdy::ObtainHpackHuffmanTable());

  for (int i = 0; i != example_count; ++i) {
    spdy::SpdyHeaderBlock headers =
        HpackFuzzUtil::NextGeneratedHeaderSet(&context);

    std::string buffer;
    CHECK(encoder.EncodeHeaderSet(headers, &buffer));

    std::string prefix = HpackFuzzUtil::HeaderBlockPrefix(buffer.size());

    CHECK_LT(0, file_out.WriteAtCurrentPos(prefix.data(), prefix.size()));
    CHECK_LT(0, file_out.WriteAtCurrentPos(buffer.data(), buffer.size()));
  }
  CHECK(file_out.Flush());
  DVLOG(1) << "Generated " << example_count << " blocks.";
  return 0;
}
