// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dawn/webgpu_cpp.h>
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"

#ifndef TESTING_LIBFUZZER_FUZZERS_COMMAND_BUFFER_LPM_FUZZER_WEBGPU_SUPPORT_H_
#define TESTING_LIBFUZZER_FUZZERS_COMMAND_BUFFER_LPM_FUZZER_WEBGPU_SUPPORT_H_

namespace gpu::cmdbuf::fuzzing {

// borrowed directly from DawnWireServerFuzzer.cpp for now
// TODO(bookholt): do something different.
class DawnWireSerializerFuzzer : public dawn::wire::CommandSerializer {
 public:
  DawnWireSerializerFuzzer();
  ~DawnWireSerializerFuzzer() override;
  size_t GetMaximumAllocationSize() const override;
  void* GetCmdSpace(size_t size) override;
  bool Flush() override;

 private:
  std::vector<char> buf;
};

}  // namespace gpu::cmdbuf::fuzzing

#endif  // TESTING_LIBFUZZER_FUZZERS_COMMAND_BUFFER_LPM_FUZZER_WEBGPU_SUPPORT_H_
