// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_TEST_RANDOM_MOJO_DELAYS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_TEST_RANDOM_MOJO_DELAYS_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"

namespace mojo {
// Begins issuing temporary pauses on randomly selected Mojo bindings for
// debugging purposes.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS) void BeginRandomMojoDelays();
namespace internal {
class BindingStateBase;
// Adds a binding state base to make it eligible for pausing.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
void MakeBindingRandomlyPaused(scoped_refptr<base::SequencedTaskRunner>,
                               base::WeakPtr<BindingStateBase>);
}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_TEST_RANDOM_MOJO_DELAYS_H_
