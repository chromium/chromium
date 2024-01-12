// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <vector>

#include "base/environment.h"
#include "base/logging.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/fuzzers/file_pair.pb.h"
#include "components/zucchini/patch_reader.h"
#include "components/zucchini/zucchini.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

struct Environment {
  Environment() {
    // Disable console spamming.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

Environment* env = new Environment();

DEFINE_BINARY_PROTO_FUZZER(const zucchini::fuzzers::FilePair& file_pair) {
  // Dump code for debugging.
  if (base::Environment::Create()->HasVar("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << "Old File: " << file_pair.old_file() << std::endl
              << "Patch File: " << file_pair.new_or_patch_file() << std::endl;
  }

  // Prepare data.
  zucchini::ConstBufferView old_image(
      reinterpret_cast<const uint8_t*>(file_pair.old_file().data()),
      file_pair.old_file().size());
  zucchini::ConstBufferView patch_file(
      reinterpret_cast<const uint8_t*>(file_pair.new_or_patch_file().data()),
      file_pair.new_or_patch_file().size());

  // Generate a patch reader.
  auto patch_reader = zucchini::EnsemblePatchReader::Create(patch_file);
  // Abort if the patch can't be read.
  if (!patch_reader.has_value())
    return;

  // Create the underlying new file.
  size_t new_size = patch_reader->header().new_size;
  // Reject unreasonably large "new" files that fuzzed patch may specify.
  if (new_size > 64 * 1024)
    return;
  std::vector<uint8_t> new_data(new_size);
  zucchini::MutableBufferView new_image(new_data.data(), new_size);

  // Fuzz target.
  zucchini::ApplyBuffer(old_image, *patch_reader, new_image);
  // No need to check whether output exist, or if so, whether it's valid.
}
