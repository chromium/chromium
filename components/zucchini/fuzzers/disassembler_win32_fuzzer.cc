// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/disassembler_win32.h"
#include "components/zucchini/fuzzers/fuzz_utils.h"

namespace {

struct Environment {
  Environment() {
    // Disable console spamming.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  if (!size)
    return 0;
  // Prepare data.
  std::vector<uint8_t> mutable_data(data, data + size);
  zucchini::ConstBufferView image(mutable_data.data(), mutable_data.size());

  // One of x86 or x64 should return a non-nullptr if the data is valid.
  auto disassembler_win32x86 =
      zucchini::Disassembler::Make<zucchini::DisassemblerWin32X86>(image);
  if (disassembler_win32x86) {
    zucchini::ReadAndWriteReferences(std::move(disassembler_win32x86),
                                     &mutable_data);
    return 0;
  }

  auto disassembler_win32x64 =
      zucchini::Disassembler::Make<zucchini::DisassemblerWin32X64>(image);
  if (disassembler_win32x64)
    zucchini::ReadAndWriteReferences(std::move(disassembler_win32x64),
                                     &mutable_data);
  return 0;
}
