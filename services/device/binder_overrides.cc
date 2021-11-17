// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/binder_overrides.h"

#include "base/no_destructor.h"

namespace device {
namespace internal {

GeolocationContextBinder& GetGeolocationContextBinderOverride() {
  static base::NoDestructor<GeolocationContextBinder> binder;
  return *binder;
}

#if defined(OS_ANDROID)
NFCProviderBinder& GetNFCProviderBinderOverride() {
  static base::NoDestructor<NFCProviderBinder> binder;
  return *binder;
}
#endif

}  // namespace internal
}  // namespace device
