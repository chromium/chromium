// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/filter_operations_mojom_traits.h"

#include "cc/paint/filter_operations.h"
#include "services/viz/public/cpp/compositing/filter_operation_mojom_traits.h"

namespace mojo {

// static
const std::vector<cc::FilterOperation>& StructTraits<
    viz::mojom::FilterOperationsDataView,
    cc::FilterOperations>::operations(const cc::FilterOperations& operations) {
  return operations.operations();
}

// static
bool StructTraits<viz::mojom::FilterOperationsDataView, cc::FilterOperations>::
    Read(viz::mojom::FilterOperationsDataView data, cc::FilterOperations* out) {
  std::vector<cc::FilterOperation> operations;
  if (!data.ReadOperations(&operations))
    return false;
  *out = cc::FilterOperations(std::move(operations));
  return true;
}

}  // namespace mojo
