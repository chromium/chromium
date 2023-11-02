// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/ar_compositor_delegate_provider.h"

namespace webxr {

ArCompositorDelegateProvider::ArCompositorDelegateProvider(
    base::android::JavaRef<jobject>&& j_compositor_delegate_provider)
    : j_compositor_delegate_provider_(
          std::move(j_compositor_delegate_provider)) {}

ArCompositorDelegateProvider::~ArCompositorDelegateProvider() = default;

ArCompositorDelegateProvider::ArCompositorDelegateProvider(
    const ArCompositorDelegateProvider& other) = default;
ArCompositorDelegateProvider& ArCompositorDelegateProvider::operator=(
    const ArCompositorDelegateProvider& other) = default;

base::android::ScopedJavaLocalRef<jobject>
ArCompositorDelegateProvider::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(
      j_compositor_delegate_provider_);
}

}  // namespace webxr
