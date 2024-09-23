// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/proto_wrapper_mojom_traits.h"

#include <limits>

#include "base/check_op.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/mojom/base/proto_wrapper.mojom-shared.h"

namespace mojo {

const std::string&
StructTraits<mojo_base::mojom::ProtoWrapperDataView, mojo_base::ProtoWrapper>::
    proto_name(const mojo_base::ProtoWrapper& wrapper) {
  CHECK(wrapper.is_valid());
  return wrapper.proto_name_;
}

mojo_base::BigBuffer& StructTraits<
    mojo_base::mojom::ProtoWrapperDataView,
    mojo_base::ProtoWrapper>::smuggled(mojo_base::ProtoWrapper& wrapper) {
  CHECK(wrapper.is_valid());
  return wrapper.bytes_.value();
}

bool StructTraits<
    mojo_base::mojom::ProtoWrapperDataView,
    mojo_base::ProtoWrapper>::Read(mojo_base::mojom::ProtoWrapperDataView data,
                                   mojo_base::ProtoWrapper* wrapper) {
  std::string proto_name;
  if (!data.ReadProtoName(&proto_name)) {
    return false;
  }
  if (proto_name.empty()) {
    return false;
  }
  mojo_base::BigBuffer smuggled;
  if (!data.ReadSmuggled(&smuggled)) {
    return false;
  }
  if (smuggled.size() > std::numeric_limits<int>::max()) {
    return false;
  }
  wrapper->proto_name_ = std::move(proto_name);
  wrapper->bytes_ = std::move(smuggled);
  return true;
}

}  // namespace mojo
