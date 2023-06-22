// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_RAND_H_
#define SANDBOX_WIN_SRC_SANDBOX_RAND_H_

namespace sandbox {

// Generate a random value in |random_value|. Returns true on success.
bool GetRandom(unsigned int* random_value);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_RAND_H_
