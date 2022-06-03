// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_THREAD_PRIORITY_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_THREAD_PRIORITY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/mojom/base/thread_priority.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::ThreadPriority, base::ThreadPriority> {
  static mojo_base::mojom::ThreadPriority ToMojom(
      base::ThreadPriority thread_priority);
  static bool FromMojom(mojo_base::mojom::ThreadPriority input,
                        base::ThreadPriority* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_THREAD_PRIORITY_MOJOM_TRAITS_H_
