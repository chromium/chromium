// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_MESSAGE_PUMP_TYPE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_MESSAGE_PUMP_TYPE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/message_loop/message_pump_type.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/message_pump_type.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::MessagePumpType, base::MessagePumpType> {
  static mojo_base::mojom::MessagePumpType ToMojom(base::MessagePumpType input);
  static bool FromMojom(mojo_base::mojom::MessagePumpType input,
                        base::MessagePumpType* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_MESSAGE_PUMP_TYPE_MOJOM_TRAITS_H_
