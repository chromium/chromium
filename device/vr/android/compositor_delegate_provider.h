// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_COMPOSITOR_DELEGATE_PROVIDER_H_
#define DEVICE_VR_ANDROID_COMPOSITOR_DELEGATE_PROVIDER_H_

#include "base/android/scoped_java_ref.h"

namespace device {

// Type safe interface for a thin wrapper around a Java object that talks to the
// Chrome compositor. Primarily exists due to //device code being unable to
// depend on //components code and to allow injection from the latter into the
// former. ArCompositorDelegateProvider(.java) is an example of a type that this
// interface can wrap.
class CompositorDelegateProvider {
 public:
  virtual ~CompositorDelegateProvider() = default;
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_COMPOSITOR_DELEGATE_PROVIDER_H_
