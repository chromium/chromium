// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_FUCHSIA_DIR_SCHEME_H_
#define FUCHSIA_BASE_FUCHSIA_DIR_SCHEME_H_

namespace cr_fuchsia {

// URL scheme used to access content directories.
extern const char kFuchsiaDirScheme[];

// Registers kFuchsiaDirScheme as a standard URL scheme.
void RegisterFuchsiaDirScheme();

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_FUCHSIA_DIR_SCHEME_H_
