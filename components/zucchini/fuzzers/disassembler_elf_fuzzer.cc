// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/disassembler_elf.h"
#include "components/zucchini/fuzzers/fuzz_utils.h"

namespace {

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  if (!size)
    return 0;
  // Prepare data.
  std::vector<uint8_t> mutable_data(data, data + size);
  zucchini::ConstBufferView image(mutable_data.data(), mutable_data.size());

  // Create disassembler. Early exit on failure.
  auto disassembler_elf_x64 =
      zucchini::Disassembler::Make<zucchini::DisassemblerElfX64>(image);
  if (disassembler_elf_x64) {
    zucchini::ReadAndWriteReferences(std::move(disassembler_elf_x64),
                                     &mutable_data);
    return 0;
  }

  auto disassembler_elf_x86 =
      zucchini::Disassembler::Make<zucchini::DisassemblerElfX86>(image);
  if (disassembler_elf_x86)
    zucchini::ReadAndWriteReferences(std::move(disassembler_elf_x86),
                                     &mutable_data);
  return 0;
}
