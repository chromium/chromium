// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/crashpad/crashpad/snapshot/ios/process_snapshot_ios_intermediate_dump.h"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size) {
  crashpad::internal::IOSIntermediateDumpByteArray dump_interface(data, size);
  crashpad::internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  process_snapshot.InitializeWithFileInterface(dump_interface, {});
  return 0;
}
