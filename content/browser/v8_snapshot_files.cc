// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/v8_snapshot_files.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

std::map<std::string, base::FilePath> GetV8SnapshotFilesToPreload(
    base::CommandLine& process_command_line) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
  return {{kV8ContextSnapshotDataDescriptor,
           base::FilePath(FILE_PATH_LITERAL(V8_CONTEXT_SNAPSHOT_FILENAME))}};
#else
  return {{kV8SnapshotDataDescriptor,
           base::FilePath(FILE_PATH_LITERAL("snapshot_blob.bin"))}};
#endif
#elif defined(OS_ANDROID)
#if defined(INCLUDE_BOTH_V8_SNAPSHOTS) || !defined(USE_V8_CONTEXT_SNAPSHOT)
#if defined(INCLUDE_BOTH_V8_SNAPSHOTS)
  if (base::FeatureList::IsEnabled(features::kUseContextSnapshot)) {
    process_command_line.AppendSwitch(switches::kUseContextSnapshotSwitch);
    // For USE_V8_CONTEXT_SNAPSHOT, the renderer reads the files directly.
    return {};
  }
#endif
  return {{kV8Snapshot64DataDescriptor,
           base::FilePath(FILE_PATH_LITERAL("assets/snapshot_blob_64.bin"))},
          {kV8Snapshot32DataDescriptor,
           base::FilePath(FILE_PATH_LITERAL("assets/snapshot_blob_32.bin"))}};
#elif defined(USE_V8_CONTEXT_SNAPSHOT)
  // For USE_V8_CONTEXT_SNAPSHOT, the renderer reads the files directly.
  return {};
#endif
#else
  return {};
#endif
}

}  // namespace content
