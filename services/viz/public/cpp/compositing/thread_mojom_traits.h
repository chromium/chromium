// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_THREAD_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_THREAD_MOJOM_TRAITS_H_

#include "components/viz/common/performance_hint_utils.h"
#include "services/viz/public/mojom/compositing/thread.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::ThreadType, viz::Thread::Type> {
  static viz::mojom::ThreadType ToMojom(viz::Thread::Type type);

  static bool FromMojom(viz::mojom::ThreadType input, viz::Thread::Type* out);
};

template <>
struct StructTraits<viz::mojom::ThreadDataView, viz::Thread> {
  static uint32_t id(const viz::Thread& thread) { return thread.id; }

  static viz::mojom::ThreadType type(const viz::Thread& thread) {
    return static_cast<viz::mojom::ThreadType>(thread.type);
  }

  static bool Read(viz::mojom::ThreadDataView data, viz::Thread* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_THREAD_MOJOM_TRAITS_H_
