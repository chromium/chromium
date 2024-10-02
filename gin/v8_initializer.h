// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_INITIALIZER_H_
#define GIN_V8_INITIALIZER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/gin_export.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/v8_platform.h"
#include "v8/include/v8-callbacks.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/public/v8_snapshot_file_type.h"
#endif

namespace v8 {
class StartupData;
}

namespace gin {

class GIN_EXPORT V8Initializer {
 public:
  // This should be called by IsolateHolder::Initialize().
  static void Initialize(IsolateHolder::ScriptMode mode,
                         const std::string& js_command_line_flags = {},
                         v8::OOMErrorCallback oom_error_callback = nullptr);

  // Get address and size information for currently loaded snapshot.
  // If no snapshot is loaded, the return values are null for addresses
  // and 0 for sizes.
  static void GetV8ExternalSnapshotData(v8::StartupData* snapshot);
  static void GetV8ExternalSnapshotData(const char** snapshot_data_out,
                                        int* snapshot_size_out);

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  // Load V8 snapshot from default resources, if they are available.
  static void LoadV8Snapshot(
      V8SnapshotFileType snapshot_file_type = V8SnapshotFileType::kDefault);

  // Load V8 snapshot from user provided file.
  // The region argument, if non-zero, specifies the portions
  // of the files to be mapped. Since the VM can boot with or without
  // the snapshot, this function does not return a status.
  static void LoadV8SnapshotFromFile(
      base::File snapshot_file,
      base::MemoryMappedFile::Region* snapshot_file_region,
      V8SnapshotFileType snapshot_file_type);

#if BUILDFLAG(IS_ANDROID)
  static base::FilePath GetSnapshotFilePath(
      bool abi_32_bit,
      V8SnapshotFileType snapshot_file_type);
#endif

#endif  // V8_USE_EXTERNAL_STARTUP_DATA

};

}  // namespace gin

#endif  // GIN_V8_INITIALIZER_H_
