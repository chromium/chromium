// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "net/dns/nsswitch_reader.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  net::NsswitchReader::FileReadCall file_read_call =
      base::BindLambdaForTesting([input]() { return input; });

  net::NsswitchReader reader;
  reader.set_file_read_call_for_testing(std::move(file_read_call));

  std::vector<net::NsswitchReader::ServiceSpecification> result =
      reader.ReadAndParseHosts();

  return 0;
}
