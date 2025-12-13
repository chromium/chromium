// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include <memory>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "ipc/constants.mojom.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"

namespace content {

namespace {

class MojoInitializer {
 public:
  MojoInitializer() {
    mojo::core::Configuration config;
    config.max_message_num_bytes = IPC::mojom::kChannelMaximumMessageSize;
    mojo::core::Init(config);
  }
};

}  // namespace

void InitializeMojo() {
  // Trivially destructible, so no NoDestructor.
  static MojoInitializer mojo_initializer;
}

}  // namespace content
