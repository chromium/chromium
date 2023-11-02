// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_REF_COUNTED_MEMORY_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_REF_COUNTED_MEMORY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/ref_counted_memory.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::RefCountedMemoryDataView,
                 scoped_refptr<base::RefCountedMemory>> {
  static mojo_base::BigBuffer data(
      const scoped_refptr<base::RefCountedMemory>& in);

  static bool IsNull(const scoped_refptr<base::RefCountedMemory>& input);
  static void SetToNull(scoped_refptr<base::RefCountedMemory>* out);

  static bool Read(mojo_base::mojom::RefCountedMemoryDataView data,
                   scoped_refptr<base::RefCountedMemory>* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_REF_COUNTED_MEMORY_MOJOM_TRAITS_H_
