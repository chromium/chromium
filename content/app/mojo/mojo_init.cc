// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "mojo/core/embedder/embedder.h"

namespace content {

namespace {

class MojoInitializer {
 public:
  MojoInitializer() { mojo::core::Init(); }
};

}  // namespace

void InitializeMojo() {
  // Trivially destructible, so no NoDestructor.
  static MojoInitializer mojo_initializer;
}

}  // namespace content
