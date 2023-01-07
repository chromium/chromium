// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_producer.h"

#include "media/capture/mojom/video_capture_buffer.mojom.h"

namespace video_capture {

MockProducer::MockProducer(mojo::PendingReceiver<mojom::Producer> receiver)
    : receiver_(this, std::move(receiver)) {}

MockProducer::~MockProducer() = default;

void MockProducer::OnNewBuffer(int32_t buffer_id,
                               media::mojom::VideoBufferHandlePtr buffer_handle,
                               OnNewBufferCallback callback) {
  DoOnNewBuffer(buffer_id, &buffer_handle, callback);
}

}  // namespace video_capture
