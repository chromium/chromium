// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/thread_type_mojom_traits.h"

#include "base/notreached.h"
#include "base/threading/platform_thread.h"

namespace mojo {

// static
mojo_base::mojom::ThreadType
EnumTraits<mojo_base::mojom::ThreadType, base::ThreadType>::ToMojom(
    base::ThreadType thread_type) {
  switch (thread_type) {
    case base::ThreadType::kBackground:
      return mojo_base::mojom::ThreadType::kBackground;
    case base::ThreadType::kUtility:
      return mojo_base::mojom::ThreadType::kUtility;
    case base::ThreadType::kResourceEfficient:
      return mojo_base::mojom::ThreadType::kResourceEfficient;
    case base::ThreadType::kDefault:
      return mojo_base::mojom::ThreadType::kDefault;
    case base::ThreadType::kDisplayCritical:
      return mojo_base::mojom::ThreadType::kDisplayCritical;
    case base::ThreadType::kRealtimeAudio:
      return mojo_base::mojom::ThreadType::kRealtimeAudio;
  }
  NOTREACHED();
}

// static
bool EnumTraits<mojo_base::mojom::ThreadType, base::ThreadType>::FromMojom(
    mojo_base::mojom::ThreadType input,
    base::ThreadType* out) {
  switch (input) {
    case mojo_base::mojom::ThreadType::kBackground:
      *out = base::ThreadType::kBackground;
      return true;
    case mojo_base::mojom::ThreadType::kUtility:
      *out = base::ThreadType::kUtility;
      return true;
    case mojo_base::mojom::ThreadType::kResourceEfficient:
      *out = base::ThreadType::kResourceEfficient;
      return true;
    case mojo_base::mojom::ThreadType::kDefault:
      *out = base::ThreadType::kDefault;
      return true;
    case mojo_base::mojom::ThreadType::kDisplayCritical:
      *out = base::ThreadType::kDisplayCritical;
      return true;
    case mojo_base::mojom::ThreadType::kRealtimeAudio:
      *out = base::ThreadType::kRealtimeAudio;
      return true;
  }
  return false;
}

}  // namespace mojo
