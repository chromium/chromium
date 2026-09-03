// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_EVENT_DISPATCHER_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_EVENT_DISPATCHER_MOJOM_TRAITS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/event_args.h"
#include "extensions/common/mojom/event_dispatcher.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::EventArgsDataView,
                    scoped_refptr<const extensions::EventArgs>> {
  static const base::ListValue& data(
      const scoped_refptr<const extensions::EventArgs>& in) {
    CHECK(in);
    return in->data;
  }

  static bool Read(extensions::mojom::EventArgsDataView data,
                   scoped_refptr<const extensions::EventArgs>* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_EVENT_DISPATCHER_MOJOM_TRAITS_H_
