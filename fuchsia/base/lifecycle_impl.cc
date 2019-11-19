// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/lifecycle_impl.h"

namespace cr_fuchsia {

LifecycleImpl::LifecycleImpl(sys::OutgoingDirectory* outgoing_directory,
                             base::OnceClosure on_terminate)
    : binding_(outgoing_directory, this),
      on_terminate_(std::move(on_terminate)) {}

LifecycleImpl::~LifecycleImpl() = default;

void LifecycleImpl::Terminate() {
  if (on_terminate_)
    std::move(on_terminate_).Run();
}

}  // namespace cr_fuchsia
