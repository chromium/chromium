// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_LIFECYCLE_IMPL_H_
#define FUCHSIA_BASE_LIFECYCLE_IMPL_H_

#include <fuchsia/modular/cpp/fidl.h>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/macros.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

namespace cr_fuchsia {

// Implements the fuchsia.modular.Lifecycle protocol, by invoking the supplied
// graceful teardown Callback when Terminate() is called, or when the Lifecycle
// client drops the channel.
class LifecycleImpl : public ::fuchsia::modular::Lifecycle {
 public:
  LifecycleImpl(sys::OutgoingDirectory* outgoing_directory,
                base::OnceClosure on_terminate);
  ~LifecycleImpl() override;

  // fuchsia::modular::Lifecycle implementation.
  void Terminate() override;

 private:
  const base::fuchsia::ScopedServiceBinding<::fuchsia::modular::Lifecycle>
      binding_;

  base::OnceClosure on_terminate_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleImpl);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_LIFECYCLE_IMPL_H_
