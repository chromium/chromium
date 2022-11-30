// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_THREAD_TYPE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_THREAD_TYPE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/mojom/base/thread_type.mojom-shared.h"

namespace base {
enum class ThreadType;
}

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::ThreadType, base::ThreadType> {
  static mojo_base::mojom::ThreadType ToMojom(base::ThreadType thread_type);
  static bool FromMojom(mojo_base::mojom::ThreadType input,
                        base::ThreadType* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_THREAD_TYPE_MOJOM_TRAITS_H_
