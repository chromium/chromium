// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/message_wrapper.h"

#include <memory>
#include <utility>

#include "mojo/core/ipcz_api.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

MessageWrapper::MessageWrapper() = default;

MessageWrapper::MessageWrapper(std::unique_ptr<MojoMessage> message,
                               IpczHandle portal)
    : message_(std::move(message)), portal_(portal) {}

MessageWrapper::~MessageWrapper() = default;

void MessageWrapper::Close() {}

bool MessageWrapper::IsSerializable() const {
  return true;
}

bool MessageWrapper::GetSerializedDimensions(Transport& transmitter,
                                             size_t& num_bytes,
                                             size_t& num_handles) {
  // No actual data to serialize for this driver object though. When the
  // recipient sees a parcel with a lone, empty MessageWrapper attached, it will
  // be ignored.
  num_bytes = 0;
  num_handles = 0;
  return true;
}

bool MessageWrapper::Serialize(Transport& transmitter,
                               base::span<uint8_t> data,
                               base::span<PlatformHandle> handles) {
  // If this method has been invoked, the driver has been asked to serialize
  // this object. We can go ahead and force message serialization and send the
  // corresponding parcel now.
  //
  // Note that this object may be forwarded multiple times to multiple nodes,
  // but the driver only needs to coerce serialization (and send a serialized
  // parcel) the first time. Any additional forwarding will see `message_` as
  // null during serialization, and nothing needs to be done in that case.
  if (std::unique_ptr<MojoMessage> message = std::move(message_)) {
    message->Serialize();
    message->AttachDataPipePortals();
    const IpczResult result =
        GetIpczAPI().Put(portal_, message->data().data(),
                         message->data().size(), message->handles().data(),
                         message->handles().size(), IPCZ_NO_FLAGS, nullptr);
    if (result == IPCZ_RESULT_OK) {
      // Ownership of attached handles has been relinquished, so ensure that the
      // MojoMessage doesn't attempt to close them on destruction.
      message->handles().clear();
    }
  }
  return true;
}

}  // namespace mojo::core::ipcz_driver
