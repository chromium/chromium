// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/shape_detection_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#if defined(OS_WIN)
#include "services/shape_detection/barcode_detection_provider_impl.h"
#include "services/shape_detection/face_detection_provider_win.h"
#elif defined(OS_MACOSX)
#include <dlfcn.h>
#include "services/shape_detection/barcode_detection_provider_mac.h"
#include "services/shape_detection/face_detection_provider_mac.h"
#else
#include "services/shape_detection/barcode_detection_provider_impl.h"
#include "services/shape_detection/face_detection_provider_impl.h"
#endif
#include "services/shape_detection/text_detection_impl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "services/shape_detection/shape_detection_jni_headers/InterfaceRegistrar_jni.h"
#endif

namespace shape_detection {

ShapeDetectionService::ShapeDetectionService(
    mojo::PendingReceiver<mojom::ShapeDetectionService> receiver)
    : receiver_(this, std::move(receiver)) {
#if defined(OS_MACOSX)
  if (__builtin_available(macOS 10.13, *)) {
    vision_framework_ =
        dlopen("/System/Library/Frameworks/Vision.framework/Vision", RTLD_LAZY);
  }
#endif
}

ShapeDetectionService::~ShapeDetectionService() {
#if defined(OS_MACOSX)
  if (__builtin_available(macOS 10.13, *)) {
    if (vision_framework_)
      dlclose(vision_framework_);
  }
#endif
}

void ShapeDetectionService::BindBarcodeDetectionProvider(
    mojo::PendingReceiver<mojom::BarcodeDetectionProvider> receiver) {
#if defined(OS_ANDROID)
  Java_InterfaceRegistrar_bindBarcodeDetectionProvider(
      base::android::AttachCurrentThread(),
      receiver.PassPipe().release().value());
#elif defined(OS_MACOSX)
  BarcodeDetectionProviderMac::Create(std::move(receiver));
#else
  BarcodeDetectionProviderImpl::Create(std::move(receiver));
#endif
}

void ShapeDetectionService::BindFaceDetectionProvider(
    mojo::PendingReceiver<mojom::FaceDetectionProvider> receiver) {
#if defined(OS_ANDROID)
  Java_InterfaceRegistrar_bindFaceDetectionProvider(
      base::android::AttachCurrentThread(),
      receiver.PassPipe().release().value());
#elif defined(OS_MACOSX)
  FaceDetectionProviderMac::Create(std::move(receiver));
#elif defined(OS_WIN)
  FaceDetectionProviderWin::Create(std::move(receiver));
#else
  FaceDetectionProviderImpl::Create(std::move(receiver));
#endif
}

void ShapeDetectionService::BindTextDetection(
    mojo::PendingReceiver<mojom::TextDetection> receiver) {
#if defined(OS_ANDROID)
  Java_InterfaceRegistrar_bindTextDetection(
      base::android::AttachCurrentThread(),
      receiver.PassPipe().release().value());
#else
  TextDetectionImpl::Create(std::move(receiver));
#endif
}

}  // namespace shape_detection
