// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_TEST_UTIL_H_
#define REMOTING_HOST_SETUP_TEST_UTIL_H_

#include "base/files/file.h"

namespace remoting {

// Creates an anonymous, unidirectional pipe, returning true if successful. On
// success, the receives ownership of both files.
bool MakePipe(base::File* read_file, base::File* write_file);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_TEST_UTIL_H_
