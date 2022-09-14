// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_IMPL_H_
#define NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "net/base/net_export.h"
#include "net/websockets/websocket_deflate_predictor.h"

namespace net {

struct WebSocketFrame;

class NET_EXPORT_PRIVATE WebSocketDeflatePredictorImpl
    : public WebSocketDeflatePredictor {
 public:
  ~WebSocketDeflatePredictorImpl() override = default;

  Result Predict(const std::vector<std::unique_ptr<WebSocketFrame>>& frames,
                 size_t frame_index) override;
  void RecordInputDataFrame(const WebSocketFrame* frame) override;
  void RecordWrittenDataFrame(const WebSocketFrame* frame) override;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_IMPL_H_
