// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_H_
#define SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"

namespace shape_detection {

class TextDetectionImpl {
 public:
  // Binds TextDetection receiver to an implementation of mojom::TextDetection.
  static void Create(mojo::PendingReceiver<mojom::TextDetection> receiver);

 private:
  ~TextDetectionImpl() = default;

  DISALLOW_COPY_AND_ASSIGN(TextDetectionImpl);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_H_
