// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_UTILS_H_
#define MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_UTILS_H_

#include "base/component_export.h"

namespace mojo {

class SharedMemoryUtils {
 public:
  COMPONENT_EXPORT(MOJO_BASE) static void InstallBaseHooks();
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_UTILS_H_
