// Copyright 2022 The Chromium Authors
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
  IpczHandle box;
  const IpczBoxContents contents = {
      .size = sizeof(contents),
      .type = IPCZ_BOX_TYPE_DRIVER_OBJECT,
      .object = {.driver_object = ReleaseAsHandle(std::move(object))},
  };
  const IpczResult result =
      GetIpczAPI().Box(GetIpczNode(), &contents, IPCZ_NO_FLAGS, nullptr, &box);
  CHECK_EQ(result, IPCZ_RESULT_OK);
  return box;
}

// static
IpczDriverHandle ObjectBase::PeekBox(IpczHandle box) {
  IpczBoxContents contents = {
      .size = sizeof(contents),
      .object = {.driver_object = IPCZ_INVALID_DRIVER_HANDLE},
  };
  GetIpczAPI().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &contents);
  if (contents.type != IPCZ_BOX_TYPE_DRIVER_OBJECT) {
    return IPCZ_INVALID_DRIVER_HANDLE;
  }
  return contents.object.driver_object;
}

// static
scoped_refptr<ObjectBase> ObjectBase::Unbox(IpczHandle box) {
  IpczBoxContents contents = {.size = sizeof(contents)};
  const IpczResult result =
      GetIpczAPI().Unbox(box, IPCZ_NO_FLAGS, nullptr, &contents);
  DCHECK_EQ(result, IPCZ_RESULT_OK);
  DCHECK_EQ(contents.type, IPCZ_BOX_TYPE_DRIVER_OBJECT);
  return TakeFromHandle(contents.object.driver_object);
}

}  // namespace mojo::core::ipcz_driver
