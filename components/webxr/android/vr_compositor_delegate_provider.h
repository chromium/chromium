// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_VR_COMPOSITOR_DELEGATE_PROVIDER_H_
#define COMPONENTS_WEBXR_ANDROID_VR_COMPOSITOR_DELEGATE_PROVIDER_H_

#include "base/android/scoped_java_ref.h"
#include "device/vr/android/compositor_delegate_provider.h"

namespace webxr {

// Wrapper around Java object that implements VrCompositorDelegateProvider
// interface (see VrCompositorDelegateProvider.java).
class VrCompositorDelegateProvider : public device::CompositorDelegateProvider {
 public:
  explicit VrCompositorDelegateProvider(
      base::android::JavaRef<jobject>&& j_compositor_delegate_provider);
  ~VrCompositorDelegateProvider() override;

  VrCompositorDelegateProvider(const VrCompositorDelegateProvider& other);
  VrCompositorDelegateProvider& operator=(
      const VrCompositorDelegateProvider& other);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_compositor_delegate_provider_;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_VR_COMPOSITOR_DELEGATE_PROVIDER_H_
