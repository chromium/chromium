// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_PROCESS_ID_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_PROCESS_ID_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "mojo/public/mojom/base/process_id.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::ProcessIdDataView, base::ProcessId> {
  static uint32_t pid(const base::ProcessId& process_id) {
    return static_cast<uint32_t>(process_id);
  }

  static bool Read(mojo_base::mojom::ProcessIdDataView data,
                   base::ProcessId* process_id);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_PROCESS_ID_MOJOM_TRAITS_H_
