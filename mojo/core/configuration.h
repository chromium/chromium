// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CONFIGURATION_H_
#define MOJO_CORE_CONFIGURATION_H_

#include "mojo/core/embedder/configuration.h"
#include "mojo/core/system_impl_export.h"

namespace mojo {
namespace core {

namespace internal {
MOJO_SYSTEM_IMPL_EXPORT extern Configuration g_configuration;
}  // namespace internal

MOJO_SYSTEM_IMPL_EXPORT inline const Configuration& GetConfiguration() {
  return internal::g_configuration;
}

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CONFIGURATION_H_
