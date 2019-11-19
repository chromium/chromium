// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/v8_snapshot_overlay_manifest.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/common/content_descriptor_keys.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace content {

const service_manager::Manifest& GetV8SnapshotOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
#if defined(OS_LINUX)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
        .PreloadFile(
            kV8ContextSnapshotDataDescriptor,
            base::FilePath(FILE_PATH_LITERAL("v8_context_snapshot.bin")))
#else
        .PreloadFile(kV8SnapshotDataDescriptor,
                     base::FilePath(FILE_PATH_LITERAL("snapshot_blob.bin")))
#endif  // defined(USE_V8_CONTEXT_SNAPSHOT)
#elif defined(OS_ANDROID)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
#if defined(ARCH_CPU_64_BITS)
        .PreloadFile(kV8Snapshot64DataDescriptor,
                     base::FilePath(FILE_PATH_LITERAL(
                         "assets/v8_context_snapshot_64.bin")))
#else
        .PreloadFile(kV8Snapshot32DataDescriptor,
                     base::FilePath(FILE_PATH_LITERAL(
                         "assets/v8_context_snapshot_32.bin")))
#endif
#else
#if defined(ARCH_CPU_64_BITS)
        .PreloadFile(
            kV8Snapshot64DataDescriptor,
            base::FilePath(FILE_PATH_LITERAL("assets/snapshot_blob_64.bin")))
#else
        .PreloadFile(
            kV8Snapshot32DataDescriptor,
            base::FilePath(FILE_PATH_LITERAL("assets/snapshot_blob_32.bin")))
#endif
#endif  // defined(USE_V8_CONTEXT_SNAPSHOT)
#endif
        .Build()
  };
  return *manifest;
}

}  // namespace content
