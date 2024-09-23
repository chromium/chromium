// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>

#include "testing/libfuzzer/proto/string_compare.pb.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

DEFINE_BINARY_PROTO_FUZZER(const string_compare::StringCompare& proto) {
  if (proto.value() == "fish") {
    std::cout << "Found fish\n";
    exit(1);
  }
}
