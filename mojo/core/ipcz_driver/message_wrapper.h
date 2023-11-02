// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_MESSAGE_WRAPPER_H_
#define MOJO_CORE_IPCZ_DRIVER_MESSAGE_WRAPPER_H_

#include <memory>

#include "mojo/core/ipcz_driver/mojo_message.h"
#include "mojo/core/ipcz_driver/object.h"

namespace mojo::core::ipcz_driver {

// A wrapper used to box a MojoMessage object if transmitted through a pipe
// unserialized. If the driver is forced to serialize this object, it will
// send a separate parcel through the transmitting portal.
class MessageWrapper : public Object<MessageWrapper> {
 public:
  MessageWrapper();

  // Wraps `message` for immediate transmission over `portal`. If this object
  // is forced to serialize, `portal` will be used to transmit the serialized
  // message contents separately. This is safe since it all happens within the
  // extent of MojoWriteMessageIpcz() it happens at all.
  MessageWrapper(std::unique_ptr<MojoMessage> message, IpczHandle portal);

  static Type object_type() { return Type::kMessageWrapper; }

  std::unique_ptr<MojoMessage> TakeMessage() { return std::move(message_); }

  // ObjectBase:
  void Close() override;
  bool IsSerializable() const override;
  bool GetSerializedDimensions(Transport& transmitter,
                               size_t& num_bytes,
                               size_t& num_handles) override;
  bool Serialize(Transport& transmitter,
                 base::span<uint8_t> data,
                 base::span<PlatformHandle> handles) override;

 private:
  ~MessageWrapper() override;

  std::unique_ptr<MojoMessage> message_;
  const IpczHandle portal_ = IPCZ_INVALID_HANDLE;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_MESSAGE_WRAPPER_H_
