// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_PATHS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_PATHS_H_

#include "content/common/content_export.h"

// This file declares path keys for the content module.  These can be used with
// the PathService to access various special directories and files.

namespace content {

enum {
  PATH_START = 4000,

  // Path and filename to the executable to use for child processes.
  CHILD_PROCESS_EXE = PATH_START,

  // Valid only in development environment
  DIR_TEST_DATA,

  PATH_END
};

// Call once to register the provider for the path keys defined above.
CONTENT_EXPORT void RegisterPathProvider();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_PATHS_H_
