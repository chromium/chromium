// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_VFS_WRAPPER_H_
#define SQL_VFS_WRAPPER_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <string>
#endif

namespace sql {

static constexpr char kVfsWrapperName[] = "VFSWrapper";

// Wraps the default VFS and to add on some functionality. The new wrapper
// becomes the default VFS. This should only be called from
// EnsureSqliteInitialized().
//
// On OSX, the wrapper propagates Time Machine exclusions from the main database
// file to associated files such as journals. <http://crbug.com/23619> and
// <http://crbug.com/25959> and others.
//
// On Fuchsia the wrapper adds in-process file locking (Fuchsia doesn't support
// file locking).
//
// TODO(shess): On Windows, wrap xFetch() with a structured exception handler.
void EnsureVfsWrapper();

// Internal representation of sqlite3_file for VFSWrapper.
struct VfsFile {
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION const sqlite3_io_methods* methods;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION sqlite3_file* wrapped_file;
#if BUILDFLAG(IS_FUCHSIA)
  std::string file_name;
#endif
};

}  // namespace sql

#endif  // SQL_VFS_WRAPPER_H_
