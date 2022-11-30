// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_MOJO_MESSAGE_H_
#define MOJO_CORE_IPCZ_DRIVER_MOJO_MESSAGE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
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

  base::span<uint8_t> data() { return data_; }
  std::vector<IpczHandle>& handles() { return handles_; }
  uintptr_t context() const { return context_; }

  IpczHandle validator() const { return validator_; }

  // Sets the contents of this message, as read from a portal by ipcz.
  bool SetContents(std::vector<uint8_t> data,
                   std::vector<IpczHandle> handles,
                   IpczHandle validator);

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
  // balanced within SetContents(), where DataPipes extract their portals from
  // the tail end of the attached handles.
  void AttachDataPipePortals();

  // Sets an unserialized message context on this message, with an optional
  // serializer and destructor.
  MojoResult SetContext(uintptr_t context,
                        MojoMessageContextSerializer serializer,
                        MojoMessageContextDestructor destructor);

  // Forcibly serializes this message if it holds an unserialized context.
  MojoResult Serialize();

 private:
  IpczHandle validator_ = IPCZ_INVALID_HANDLE;
  std::vector<uint8_t> data_storage_;
  base::span<uint8_t> data_;
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
