// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/downloader_params.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

// A static bridge to connect between the native and java code for on-device
// model.
class OnDeviceModelBridge {
 public:
  // Creates a new AiCoreSession instance. Caller is responsible for
  // creating a global ref if it needs to be stored.
  static base::android::ScopedJavaLocalRef<jobject> CreateSession(
      optimization_guide::proto::ModelExecutionFeature feature,
      on_device_model::mojom::SessionParamsPtr params);

  // Creates a new AiCoreModelDownloader instance. Caller is responsible for
  // creating a global ref if it needs to be stored.
  static base::android::ScopedJavaLocalRef<jobject> CreateModelDownloader(
      optimization_guide::proto::ModelExecutionFeature feature,
      mojom::DownloaderParamsPtr params);
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_H_
