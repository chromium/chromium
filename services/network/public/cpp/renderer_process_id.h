// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RENDERER_PROCESS_ID_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RENDERER_PROCESS_ID_H_

#include "base/types/id_type.h"

namespace network {

// An opaque ID type used to uniquely identify child renderer process instances.
// This mirrors the definition in content/public/common/child_process_id.h to
// avoid a dependency on content.
using RendererProcessId = base::IdType<class RendererProcessTag,
                                       int32_t,
                                       -1,
                                       /*kFirstGeneratedId=*/1,
                                       /*kExtraInvalidValues=*/0>;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RENDERER_PROCESS_ID_H_
