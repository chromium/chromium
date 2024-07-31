// Copyright 2018 The Chromium Authors
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
#include "components/zucchini/disassembler_dex.h"

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
  auto disassembler_dex =
      zucchini::Disassembler::Make<zucchini::DisassemblerDex>(image);
  if (!disassembler_dex)
    return 0;
  CHECK_LE(disassembler_dex->size(), image.size());
  zucchini::MutableBufferView mutable_image(mutable_data.data(),
                                            disassembler_dex->size());

  std::vector<zucchini::Reference> references;
  // Read all references in the file.
  auto groups = disassembler_dex->MakeReferenceGroups();
  for (const auto& group : groups) {
    auto reader = group.GetReader(disassembler_dex.get());
    for (auto ref = reader->GetNext(); ref.has_value();
         ref = reader->GetNext()) {
      references.push_back(ref.value());
    }
    reader.reset();
    auto writer = group.GetWriter(mutable_image, disassembler_dex.get());
    for (const auto& ref : references)
      writer->PutNext(ref);
    references.clear();
  }
  return 0;
}
