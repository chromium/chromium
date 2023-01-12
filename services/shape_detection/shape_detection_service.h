// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_SERVICE_H_
#define SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_SERVICE_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"

namespace shape_detection {

class ShapeDetectionService : public mojom::ShapeDetectionService {
 public:
  explicit ShapeDetectionService(
      mojo::PendingReceiver<mojom::ShapeDetectionService> receiver);

  ShapeDetectionService(const ShapeDetectionService&) = delete;
  ShapeDetectionService& operator=(const ShapeDetectionService&) = delete;

  ~ShapeDetectionService() override;

  // mojom::ShapeDetectionService implementation:
  void BindBarcodeDetectionProvider(
      mojo::PendingReceiver<mojom::BarcodeDetectionProvider> receiver) override;
  void BindFaceDetectionProvider(
      mojo::PendingReceiver<mojom::FaceDetectionProvider> receiver) override;
  void BindTextDetection(
      mojo::PendingReceiver<mojom::TextDetection> receiver) override;

 private:
  mojo::Receiver<mojom::ShapeDetectionService> receiver_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_SERVICE_H_
