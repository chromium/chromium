// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <iostream>
#include <memory>

#include "base/containers/heap_array.h"
#include "base/environment.h"
#include "base/logging.h"
#include "components/zucchini/buffer_sink.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/fuzzers/file_pair.pb.h"
#include "components/zucchini/patch_writer.h"
#include "components/zucchini/zucchini.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace {

constexpr size_t kMinImageSize = 16;
constexpr size_t kMaxImageSize = 1024;

}  // namespace

struct Environment {
  Environment() {
    // Disable console spamming.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

DEFINE_BINARY_PROTO_FUZZER(const zucchini::fuzzers::FilePair& file_pair) {
  static Environment env;
  // Dump code for debugging.
  if (base::Environment::Create()->HasVar("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << "Imposed Matches: " << file_pair.imposed_matches() << std::endl
              << "Old File: " << file_pair.old_file() << std::endl
              << "New File: " << file_pair.new_or_patch_file() << std::endl;
  }

  // Prepare data.
  zucchini::ConstBufferView old_image(
      reinterpret_cast<const uint8_t*>(file_pair.old_file().data()),
      file_pair.old_file().size());
  zucchini::ConstBufferView new_image(
      reinterpret_cast<const uint8_t*>(file_pair.new_or_patch_file().data()),
      file_pair.new_or_patch_file().size());

  // Restrict image sizes to speed up fuzzing.
  if (old_image.size() < kMinImageSize || old_image.size() > kMaxImageSize ||
      new_image.size() < kMinImageSize || new_image.size() > kMaxImageSize) {
    return;
  }

  // Generate a patch writer.
  zucchini::EnsemblePatchWriter patch_writer(old_image, new_image);

  // Fuzz Target.
  zucchini::GenerateBufferImposed(old_image, new_image,
                                  file_pair.imposed_matches(), &patch_writer);

  // Write to buffer to avoid IO.
  size_t patch_size = patch_writer.SerializedSize();
  auto patch_data = base::HeapArray<uint8_t>::Uninit(patch_size);
  zucchini::BufferSink patch(patch_data.data(), patch_data.size());
  patch_writer.SerializeInto(patch);
}
