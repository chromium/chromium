// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/process_id_mojom_traits.h"

namespace mojo {

bool StructTraits<mojo_base::mojom::ProcessIdDataView, base::ProcessId>::Read(
    mojo_base::mojom::ProcessIdDataView data,
    base::ProcessId* process_id) {
  *process_id = static_cast<base::ProcessId>(data.pid());
  return true;
}

}  // namespace mojo