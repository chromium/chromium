// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SCOPED_PROTOBUF_HTTP_REQUEST_H_
#define REMOTING_BASE_SCOPED_PROTOBUF_HTTP_REQUEST_H_

#include "base/functional/callback.h"

namespace remoting {

// A helper class that will cancel the pending request when the instance is
// deleted.
class ScopedProtobufHttpRequest {
 public:
  explicit ScopedProtobufHttpRequest(base::OnceClosure request_invalidator);
  virtual ~ScopedProtobufHttpRequest();
  ScopedProtobufHttpRequest(const ScopedProtobufHttpRequest&) = delete;
  ScopedProtobufHttpRequest& operator=(const ScopedProtobufHttpRequest&) =
      delete;

 private:
  base::OnceClosure request_invalidator_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SCOPED_PROTOBUF_HTTP_REQUEST_H_
