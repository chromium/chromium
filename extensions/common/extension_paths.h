// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_PATHS_H_
#define EXTENSIONS_COMMON_EXTENSION_PATHS_H_

// This file declares path keys for extensions.  These can be used with
// the PathService to access various special directories and files.

namespace extensions {

enum {
  PATH_START = 6000,

  // Valid only in development environment
  DIR_TEST_DATA,

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_PATHS_H_
