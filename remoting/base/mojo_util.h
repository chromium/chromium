// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_MOJO_UTIL_H_
#define REMOTING_BASE_MOJO_UTIL_H_

#include "mojo/core/embedder/configuration.h"

namespace remoting {

// Calls mojo::core::Init() with the right configuration for the current
// platform.
void InitializeMojo(const mojo::core::Configuration& config = {});

}  // namespace remoting

#endif  // REMOTING_BASE_MOJO_UTIL_H_
