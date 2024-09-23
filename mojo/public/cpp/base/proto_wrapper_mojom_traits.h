// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/mojom/base/proto_wrapper.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_PROTOBUF_SUPPORT_TRAITS)
    StructTraits<mojo_base::mojom::ProtoWrapperDataView,
                 mojo_base::ProtoWrapper> {
  static const std::string& proto_name(const mojo_base::ProtoWrapper& wrapper);
  static mojo_base::BigBuffer& smuggled(mojo_base::ProtoWrapper& wrapper);

  static bool Read(mojo_base::mojom::ProtoWrapperDataView data,
                   mojo_base::ProtoWrapper* message);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_MOJOM_TRAITS_H_
