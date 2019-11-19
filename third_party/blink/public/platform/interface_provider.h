// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERFACE_PROVIDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERFACE_PROVIDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// Implementations of blink::InterfaceProvider should be thread safe. As such it
// is okay to call |GetInterface| from any thread, without the thread hopping
// that would have been necesary with service_manager::InterfaceProvider.
class BLINK_PLATFORM_EXPORT InterfaceProvider {
 public:
  virtual void GetInterface(const char* name,
                            mojo::ScopedMessagePipeHandle) = 0;

  template <typename Interface>
  void GetInterface(mojo::PendingReceiver<Interface> receiver) {
    GetInterface(Interface::Name_, receiver.PassPipe());
  }
};

}  // namespace blink

#endif
