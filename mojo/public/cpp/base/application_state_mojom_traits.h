// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_APPLICATION_STATE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_APPLICATION_STATE_MOJOM_TRAITS_H_

#include "base/android/application_status_listener.h"
#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/application_state.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::ApplicationState,
               base::android::ApplicationState> {
  static mojo_base::mojom::ApplicationState ToMojom(
      base::android::ApplicationState input);
  static bool FromMojom(mojo_base::mojom::ApplicationState input,
                        base::android::ApplicationState* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_APPLICATION_STATE_MOJOM_TRAITS_H_
