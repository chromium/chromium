// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_PROCESS_PRIORITY_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_PROCESS_PRIORITY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/process/process.h"
#include "mojo/public/mojom/base/process_priority.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::ProcessPriority, base::Process::Priority> {
  static mojo_base::mojom::ProcessPriority ToMojom(
      base::Process::Priority input);
  static bool FromMojom(mojo_base::mojom::ProcessPriority input,
                        base::Process::Priority* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_PROCESS_PRIORITY_MOJOM_TRAITS_H_
