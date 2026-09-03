// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/event_dispatcher_mojom_traits.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom-shared.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"

namespace mojo {

bool StructTraits<extensions::mojom::EventArgsDataView,
                  scoped_refptr<const extensions::EventArgs>>::
    Read(extensions::mojom::EventArgsDataView data,
         scoped_refptr<const extensions::EventArgs>* out) {
  base::ListValue list;
  if (!data.ReadData(&list)) {
    return false;
  }
  *out = base::MakeRefCounted<extensions::EventArgs>(std::move(list));
  return true;
}

}  // namespace mojo
