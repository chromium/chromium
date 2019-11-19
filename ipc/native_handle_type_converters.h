// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_NATIVE_HANDLE_CONVERTER_H_
#define IPC_NATIVE_HANDLE_CONVERTER_H_

#include "ipc/ipc_message_attachment.h"
#include "mojo/public/cpp/bindings/type_converter.h"  // nogncheck
#include "mojo/public/interfaces/bindings/native_struct.mojom-shared.h"

namespace mojo {

template <>
struct TypeConverter<IPC::MessageAttachment::Type,
                     native::SerializedHandleType> {
  static IPC::MessageAttachment::Type Convert(
      native::SerializedHandleType type);
};

template <>
struct TypeConverter<native::SerializedHandleType,
                     IPC::MessageAttachment::Type> {
  static native::SerializedHandleType Convert(
      IPC::MessageAttachment::Type type);
};

}  // namespace mojo

#endif  // IPC_NATIVE_HANDLE_CONVERTER_H_
