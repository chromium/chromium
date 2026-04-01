// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_ID_MOJOM_TRAITS_H_
#define CONTENT_COMMON_CHILD_PROCESS_ID_MOJOM_TRAITS_H_

#include "content/common/web_contents_ns_view_bridge.mojom-shared.h"
#include "content/public/common/child_process_id.h"

namespace mojo {

template <>
struct StructTraits<remote_cocoa::mojom::ChildProcessIdDataView,
                    content::ChildProcessId> {
  static int32_t id(content::ChildProcessId c) { return c.GetUnsafeValue(); }
  static bool Read(remote_cocoa::mojom::ChildProcessIdDataView data,
                   content::ChildProcessId* out) {
    *out = content::ChildProcessId::FromUnsafeValue(data.id());
    return true;
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_CHILD_PROCESS_ID_MOJOM_TRAITS_H_
