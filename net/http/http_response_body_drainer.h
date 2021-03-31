// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_BODY_DRAINER_H_
#define NET_HTTP_HTTP_RESPONSE_BODY_DRAINER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/http/http_network_session.h"

namespace net {

class HttpStream;
class IOBuffer;

class NET_EXPORT_PRIVATE HttpResponseBodyDrainer {
 public:
  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small page just a
  // few hundred bytes long.  We set a limit to prevent it from taking too long,
  // since we may as well just create a new socket then.
  static const int kDrainBodyBufferSize = 16384;
  static const int kTimeoutInSeconds = 5;

  explicit HttpResponseBodyDrainer(HttpStream* stream);
  ~HttpResponseBodyDrainer();

  // Starts reading the body until completion, or we hit the buffer limit, or we
  // timeout.  After Start(), |this| will eventually delete itself.  If it
  // doesn't complete immediately, it will add itself to |session|.
  void Start(HttpNetworkSession* session);

 private:
  enum State {
    STATE_DRAIN_RESPONSE_BODY,
    STATE_DRAIN_RESPONSE_BODY_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result);

  int DoDrainResponseBody();
  int DoDrainResponseBodyComplete(int result);

  void OnIOComplete(int result);
  void OnTimerFired();
  void Finish(int result);

  scoped_refptr<IOBuffer> read_buf_;
  const std::unique_ptr<HttpStream> stream_;
  State next_state_;
  int total_read_;
  base::OneShotTimer timer_;
  HttpNetworkSession* session_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseBodyDrainer);
};

}  // namespace net

#endif // NET_HTTP_HTTP_RESPONSE_BODY_DRAINER_H_
