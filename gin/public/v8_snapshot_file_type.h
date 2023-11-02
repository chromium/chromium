// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_V8_SNAPSHOT_FILE_TYPE_H_
#define GIN_PUBLIC_V8_SNAPSHOT_FILE_TYPE_H_

#if !defined(V8_USE_EXTERNAL_STARTUP_DATA)
#error Don't include this header unless v8_external_startup_data is true.
#endif

#include "gin/gin_export.h"

namespace gin {

// Indicates which file to load as a snapshot blob image.
enum class V8SnapshotFileType {
  kDefault,

  // Snapshot augmented with customized contexts, which can be deserialized
  // using v8::Context::FromSnapshot.
  kWithAdditionalContext,
};

// Returns the V8SnapshotFileType used when loading the snapshot. This must
// be called after loading the snapshot.
// NOTE: this is implemented in v8_initializer.cc
GIN_EXPORT V8SnapshotFileType GetLoadedSnapshotFileType();

}  // namespace gin

#endif  // GIN_PUBLIC_V8_SNAPSHOT_FILE_TYPE_H_
