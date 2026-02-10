// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_args.h"
#include "sandbox/win/src/ipc_tags.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  uint32_t output_size = 0;
  std::unique_ptr<sandbox::CrossCallParamsEx> params(
      sandbox::CrossCallParamsEx::CreateFromBuffer(const_cast<uint8_t*>(data),
                                                   size, &output_size));

  if (!params.get()) {
    return 0;
  }
  sandbox::IPCArgs args;
  args.Initialize(params.get());
  return 0;
}
