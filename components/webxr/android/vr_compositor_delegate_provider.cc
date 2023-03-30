// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/vr_compositor_delegate_provider.h"

namespace webxr {

VrCompositorDelegateProvider::VrCompositorDelegateProvider(
    base::android::JavaRef<jobject>&& j_compositor_delegate_provider)
    : j_compositor_delegate_provider_(
          std::move(j_compositor_delegate_provider)) {}

VrCompositorDelegateProvider::~VrCompositorDelegateProvider() = default;

VrCompositorDelegateProvider::VrCompositorDelegateProvider(
    const VrCompositorDelegateProvider& other) = default;
VrCompositorDelegateProvider& VrCompositorDelegateProvider::operator=(
    const VrCompositorDelegateProvider& other) = default;

base::android::ScopedJavaLocalRef<jobject>
VrCompositorDelegateProvider::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(
      j_compositor_delegate_provider_);
}

}  // namespace webxr
