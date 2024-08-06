// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_MOJO_MESSAGE_H_
#define MOJO_CORE_IPCZ_DRIVER_MOJO_MESSAGE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_span.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// The ipcz-based implementation of Mojo message objects. ipcz API exposes no
// notion of message objects, so this is merely heap storage for data and ipcz
// handles.
class MojoMessage {
 public:
  // Even with an input size of 0, MojoAppendMessageData is expected to allocate
  // *some* storage for message data. This constant therefore sets a lower bound
  // on payload allocation size. 32 bytes is chosen since it's the smallest
  // possible Mojo bindings message size (v0 header + 8 byte payload)
  static constexpr size_t kMinBufferSize = 32;

  MojoMessage();
  MojoMessage(std::vector<uint8_t> data, std::vector<IpczHandle> handles);

  ~MojoMessage();

  static MojoMessage* FromHandle(MojoMessageHandle handle) {
    return reinterpret_cast<MojoMessage*>(handle);
  }

  static std::unique_ptr<MojoMessage> TakeFromHandle(MojoMessageHandle handle) {
    return base::WrapUnique(FromHandle(handle));
  }

  MojoMessageHandle handle() const {
    return reinterpret_cast<MojoMessageHandle>(this);
  }

  base::span<const uint8_t> data() const { return data_; }
  base::span<uint8_t> mutable_data() const { return data_; }
  std::vector<IpczHandle>& handles() { return handles_; }
  uintptr_t context() const { return context_; }

  IpczHandle parcel() const { return parcel_.get(); }

  // Sets the received parcel object backing this message.
  void SetParcel(ScopedIpczHandle parcel);

  // Reserves capacity within a new message object, effectively implementing
  // MojoReserveMessageCapacity().
  MojoResult ReserveCapacity(uint32_t payload_buffer_size,
                             uint32_t* buffer_size);

  // Appends data to a new or partially serialized message, effectively
  // implementing MojoAppendMessageData().
  MojoResult AppendData(uint32_t additional_num_bytes,
                        const MojoHandle* handles,
                        uint32_t num_handles,
                        void** buffer,
                        uint32_t* buffer_size,
                        bool commit_size);

  // Retrieves data from a serialized message, effectively implementing
  // MojoGetMessageData();
  IpczResult GetData(void** buffer,
                     uint32_t* num_bytes,
                     MojoHandle* handles,
                     uint32_t* num_handles,
                     bool consume_handles);

  // Finalizes the Message by ensuring that any attached DataPipe objects also
  // attach their portals alongside the existing attachments. This operation is
  // balanced within SetParcel(), where DataPipes extract their portals from
  // the tail end of the attached handles.
  void AttachDataPipePortals();

  // Sets an unserialized message context on this message, with an optional
  // serializer and destructor.
  MojoResult SetContext(uintptr_t context,
                        MojoMessageContextSerializer serializer,
                        MojoMessageContextDestructor destructor);

  // Forcibly serializes this message if it holds an unserialized context.
  MojoResult Serialize();

  // Functions provided to ipcz when boxing MojoMessage objects for lazy
  // serialization.
  static IpczResult SerializeForIpcz(uintptr_t object,
                                     uint32_t,
                                     const void*,
                                     volatile void* data,
                                     size_t* num_bytes,
                                     IpczHandle* handles,
                                     size_t* num_handles);
  static void DestroyForIpcz(uintptr_t object, uint32_t, const void*);

  // Boxes a MojoMessage object for transmission within another message. This is
  // used to support transmission of unserialized MojoMessages through ipcz,
  // with support for lazy serialization if needed.
  static ScopedIpczHandle Box(std::unique_ptr<MojoMessage> message);

  // Constructs a new MojoMessage from `message`, if `message` contains a single
  // box with an application object or subparcel inside of it. In that case the
  // application object or subparcel is interpreted as an embedded MojoMessage,
  // and that MojoMessage is reconstituted and returned. Otherwise this returns
  // null to indicate that `message` is not a wrapper around another
  // MojoMessage.
  static std::unique_ptr<MojoMessage> UnwrapFrom(MojoMessage& message);

 private:
  IpczResult SerializeForIpczImpl(volatile void* data,
                                  size_t* num_bytes,
                                  IpczHandle* handles,
                                  size_t* num_handles);

  // The parcel backing this message, if any.
  ScopedIpczHandle parcel_;

  // A heap buffer of message data, used only when `parcel_` is null.
  using DataPtr = std::unique_ptr<uint8_t>;
  DataPtr data_storage_;
  size_t data_storage_size_ = 0;

  // A view into the message data, whether it's backed by `parcel_` or stored in
  // `data_storage_`.
  base::raw_span<uint8_t, DanglingUntriaged> data_;

  std::vector<IpczHandle> handles_;
  bool handles_consumed_ = false;
  bool size_committed_ = false;

  // Unserialized message state. These values are provided by the application
  // calling MojoSetMessageContext() for lazy serialization. `context_` is an
  // arbitrary opaque value. `serializer_` is invoked when the application must
  // produce a serialized message, with `context_` as an input. `destructor_` is
  // if non-null is called to clean up any application state associated with
  // `context_`.
  //
  // If `context_` is zero, then no unserialized message context has been set by
  // the application.
  uintptr_t context_ = 0;
  MojoMessageContextSerializer serializer_ = nullptr;
  MojoMessageContextDestructor destructor_ = nullptr;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_MOJO_MESSAGE_H_
