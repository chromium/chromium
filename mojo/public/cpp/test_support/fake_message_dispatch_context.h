// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_TEST_SUPPORT_FAKE_MESSAGE_DISPATCH_CONTEXT_H_
#define MOJO_PUBLIC_CPP_TEST_SUPPORT_FAKE_MESSAGE_DISPATCH_CONTEXT_H_

#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

// A fake MessageDispatchContext that can be used in unit tests.  This is
// typically used so that the code under test can call BadMessage() without
// triggering DCHECKs.
class FakeMessageDispatchContext {
 public:
  FakeMessageDispatchContext()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {}
  FakeMessageDispatchContext(const FakeMessageDispatchContext&) = delete;
  FakeMessageDispatchContext operator=(const FakeMessageDispatchContext&) =
      delete;

 private:
  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_TEST_SUPPORT_FAKE_MESSAGE_DISPATCH_CONTEXT_H_
