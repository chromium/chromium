// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_body_drainer.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream.h"

namespace net {

const int HttpResponseBodyDrainer::kDrainBodyBufferSize;
const int HttpResponseBodyDrainer::kTimeoutInSeconds;

HttpResponseBodyDrainer::HttpResponseBodyDrainer(HttpStream* stream)
    : stream_(stream),
      next_state_(STATE_NONE),
      total_read_(0),
      session_(nullptr) {}

HttpResponseBodyDrainer::~HttpResponseBodyDrainer() = default;

void HttpResponseBodyDrainer::Start(HttpNetworkSession* session) {
  read_buf_ = base::MakeRefCounted<IOBuffer>(kDrainBodyBufferSize);
  next_state_ = STATE_DRAIN_RESPONSE_BODY;
  int rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kTimeoutInSeconds),
                 this,
                 &HttpResponseBodyDrainer::OnTimerFired);
    session_ = session;
    session->AddResponseDrainer(base::WrapUnique(this));
    return;
  }

  Finish(rv);
}

int HttpResponseBodyDrainer::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_DRAIN_RESPONSE_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoDrainResponseBody();
        break;
      case STATE_DRAIN_RESPONSE_BODY_COMPLETE:
        rv = DoDrainResponseBodyComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpResponseBodyDrainer::DoDrainResponseBody() {
  next_state_ = STATE_DRAIN_RESPONSE_BODY_COMPLETE;

  return stream_->ReadResponseBody(
      read_buf_.get(), kDrainBodyBufferSize - total_read_,
      base::BindOnce(&HttpResponseBodyDrainer::OnIOComplete,
                     base::Unretained(this)));
}

int HttpResponseBodyDrainer::DoDrainResponseBodyComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result < 0)
    return result;

  total_read_ += result;
  if (stream_->IsResponseBodyComplete())
    return OK;

  DCHECK_LE(total_read_, kDrainBodyBufferSize);
  if (total_read_ >= kDrainBodyBufferSize)
    return ERR_RESPONSE_BODY_TOO_BIG_TO_DRAIN;

  if (result == 0)
    return ERR_CONNECTION_CLOSED;

  next_state_ = STATE_DRAIN_RESPONSE_BODY;
  return OK;
}

void HttpResponseBodyDrainer::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    timer_.Stop();
    Finish(rv);
  }
}

void HttpResponseBodyDrainer::OnTimerFired() {
  Finish(ERR_TIMED_OUT);
}

void HttpResponseBodyDrainer::Finish(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (session_)
    session_->RemoveResponseDrainer(this);

  if (result < 0 || !stream_->CanReuseConnection()) {
    stream_->Close(true /* no keep-alive */);
  } else {
    DCHECK_EQ(OK, result);
    stream_->Close(false /* keep-alive */);
  }

  delete this;
}

}  // namespace net
