// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_producer.h"

namespace video_capture {

MockProducer::MockProducer(mojom::ProducerRequest request)
    : binding_(this, std::move(request)) {}

MockProducer::~MockProducer() = default;

void MockProducer::OnNewBuffer(int32_t buffer_id,
                               media::mojom::VideoBufferHandlePtr buffer_handle,
                               OnNewBufferCallback callback) {
  DoOnNewBuffer(buffer_id, &buffer_handle, callback);
}

}  // namespace video_capture
