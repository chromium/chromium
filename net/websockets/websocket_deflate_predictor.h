// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_H_
#define NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "net/base/net_export.h"

namespace net {

struct WebSocketFrame;

// WebSocketDeflatePredictor is an interface class used for judging whether
// a WebSocketDeflateStream should compress a message or not.
class NET_EXPORT_PRIVATE WebSocketDeflatePredictor {
 public:
  enum Result {
    // Deflate and send the message.
    DEFLATE,
    // Do not deflate and send the original message.
    DO_NOT_DEFLATE,
    // Try compressing the message and send the smaller one of the original
    // and the compressed message.
    // Returning this result implies that the deflater is running on
    // DoNotTakeOverContext mode and the entire message is visible.
    TRY_DEFLATE,
  };

  virtual ~WebSocketDeflatePredictor() = default;

  // Predicts and returns whether the deflater should deflate the message
  // which begins with |frames[frame_index]| or not.
  // |frames[(frame_index + 1):]| consists of future frames if any.
  // |frames[frame_index]| must be the first frame of a data message,
  // but future frames may contain control message frames.
  // |frames[frame_index]| cannot be recorded yet and all preceding
  // data frames have to be already recorded when this method is called.
  virtual Result Predict(
      const std::vector<std::unique_ptr<WebSocketFrame>>& frames,
      size_t frame_index) = 0;

  // Records frame data for future prediction.
  // Only data frames should be recorded. Do not pass control frames' data.
  // All input data frames for the stream must be recorded in order.
  virtual void RecordInputDataFrame(const WebSocketFrame* frame) = 0;

  // Records frame data for future prediction.
  // Only data frames should be recorded. Do not pass control frames' data.
  // All data frames written by the stream must be recorded in order
  // regardless of whether they are compressed or not.
  virtual void RecordWrittenDataFrame(const WebSocketFrame* frame) = 0;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_H_
