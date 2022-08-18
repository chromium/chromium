// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/object.h"

#include "base/check_op.h"
#include "mojo/core/ipcz_api.h"

namespace mojo::core::ipcz_driver {

ObjectBase::ObjectBase(Type type) : type_(type) {}

ObjectBase::~ObjectBase() = default;

void ObjectBase::Close() {}

bool ObjectBase::IsSerializable() const {
  return false;
}

bool ObjectBase::GetSerializedDimensions(Transport& transmitter,
                                         size_t& num_bytes,
                                         size_t& num_handles) {
  return false;
}

bool ObjectBase::Serialize(Transport& transmitter,
                           base::span<uint8_t> data,
                           base::span<PlatformHandle> handles) {
  return false;
}

// static
IpczHandle ObjectBase::Box(scoped_refptr<ObjectBase> object) {
  IpczDriverHandle handle = ReleaseAsHandle(std::move(object));
  IpczHandle box;
  const IpczResult result =
      GetIpczAPI().Box(GetIpczNode(), handle, IPCZ_NO_FLAGS, nullptr, &box);
  CHECK_EQ(result, IPCZ_RESULT_OK);
  return box;
}

// static
IpczDriverHandle ObjectBase::PeekBox(IpczHandle box) {
  IpczDriverHandle handle = IPCZ_INVALID_DRIVER_HANDLE;
  GetIpczAPI().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &handle);
  return handle;
}

// static
scoped_refptr<ObjectBase> ObjectBase::Unbox(IpczHandle box) {
  IpczDriverHandle handle = IPCZ_INVALID_DRIVER_HANDLE;
  GetIpczAPI().Unbox(box, IPCZ_NO_FLAGS, nullptr, &handle);
  return TakeFromHandle(handle);
}

}  // namespace mojo::core::ipcz_driver
