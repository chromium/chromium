// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_MOJO_MESSAGE_DISPATCH_CONTEXT_H_
#define CONTENT_TEST_FAKE_MOJO_MESSAGE_DISPATCH_CONTEXT_H_

#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

class FakeMojoMessageDispatchContext {
 public:
  FakeMojoMessageDispatchContext()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {}

 private:
  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;

  DISALLOW_COPY_AND_ASSIGN(FakeMojoMessageDispatchContext);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_MOJO_MESSAGE_DISPATCH_CONTEXT_H_
