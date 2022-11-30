// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_TRUST_UTIL_H_
#define REMOTING_HOST_WIN_TRUST_UTIL_H_

#include "base/files/file_path.h"

namespace remoting {

// Validates the signature for the provided |binary_path| and returns true if
// the binary is trusted. Note that this always returns true on non-official
// builds.
bool IsBinaryTrusted(const base::FilePath& binary_path);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_TRUST_UTIL_H_
