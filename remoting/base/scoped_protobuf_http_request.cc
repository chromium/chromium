// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/scoped_protobuf_http_request.h"

namespace remoting {

ScopedProtobufHttpRequest::ScopedProtobufHttpRequest(
    base::OnceClosure request_invalidator)
    : request_invalidator_(std::move(request_invalidator)) {
  DCHECK(request_invalidator_);
}

ScopedProtobufHttpRequest::~ScopedProtobufHttpRequest() {
  std::move(request_invalidator_).Run();
}

}  // namespace remoting
