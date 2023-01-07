// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_COMMON_MODULAR_LIFECYCLE_IMPL_H_
#define FUCHSIA_WEB_RUNNERS_COMMON_MODULAR_LIFECYCLE_IMPL_H_

#include <fuchsia/modular/cpp/fidl.h>

#include "base/fuchsia/scoped_service_binding.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

namespace cr_fuchsia {

// Implements the fuchsia.modular.Lifecycle protocol, by invoking the supplied
// graceful teardown Callback when Terminate() is called, or when the Lifecycle
// client drops the channel.
class LifecycleImpl final : public ::fuchsia::modular::Lifecycle {
 public:
  LifecycleImpl(sys::OutgoingDirectory* outgoing_directory,
                base::OnceClosure on_terminate);
  ~LifecycleImpl() override;

  LifecycleImpl(const LifecycleImpl&) = delete;
  LifecycleImpl& operator=(const LifecycleImpl&) = delete;

  // fuchsia::modular::Lifecycle implementation.
  void Terminate() override;

 private:
  const base::ScopedServiceBinding<::fuchsia::modular::Lifecycle> binding_;

  base::OnceClosure on_terminate_;
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_WEB_RUNNERS_COMMON_MODULAR_LIFECYCLE_IMPL_H_
