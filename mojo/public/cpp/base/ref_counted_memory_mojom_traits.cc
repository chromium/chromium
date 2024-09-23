// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/ref_counted_memory_mojom_traits.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

// static
mojo_base::BigBuffer StructTraits<mojo_base::mojom::RefCountedMemoryDataView,
                                  scoped_refptr<base::RefCountedMemory>>::
    data(const scoped_refptr<base::RefCountedMemory>& in) {
  return mojo_base::BigBuffer(base::span(*in));
}

// static
bool StructTraits<mojo_base::mojom::RefCountedMemoryDataView,
                  scoped_refptr<base::RefCountedMemory>>::
    IsNull(const scoped_refptr<base::RefCountedMemory>& input) {
  return !input.get();
}

// static
void StructTraits<mojo_base::mojom::RefCountedMemoryDataView,
                  scoped_refptr<base::RefCountedMemory>>::
    SetToNull(scoped_refptr<base::RefCountedMemory>* out) {
  *out = scoped_refptr<base::RefCountedMemory>();
}

// static
bool StructTraits<mojo_base::mojom::RefCountedMemoryDataView,
                  scoped_refptr<base::RefCountedMemory>>::
    Read(mojo_base::mojom::RefCountedMemoryDataView data,
         scoped_refptr<base::RefCountedMemory>* out) {
  mojo_base::BigBuffer buffer;
  if (!data.ReadData(&buffer)) {
    return false;
  }
  *out = base::MakeRefCounted<base::RefCountedBytes>(buffer);
  return true;
}

}  // namespace mojo
