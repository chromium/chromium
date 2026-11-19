// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace {

bool g_result = true;
int g_call_count = 0;

}  // namespace

extern "C" {

__attribute__((visibility("default"))) void SetFakeSodaPreSandboxInitResult(
    bool result) {
  g_result = result;
}

__attribute__((visibility("default"))) int
GetFakeSodaPreSandboxInitCallCount() {
  return g_call_count;
}

__attribute__((visibility("default"))) bool SodaPreSandboxInit() {
  ++g_call_count;
  return g_result;
}

}  // extern "C"
