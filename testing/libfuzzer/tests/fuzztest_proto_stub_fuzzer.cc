// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/string_compare_fuzzable.pb.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

static void Stub(const fuzzable::string_compare::StringCompare& input) {}

FUZZ_TEST(FuzzTestProtoStub, Stub);
