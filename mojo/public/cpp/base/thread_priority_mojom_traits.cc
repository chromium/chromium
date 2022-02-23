// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/thread_priority_mojom_traits.h"

#include "base/notreached.h"
#include "base/threading/platform_thread.h"

namespace mojo {

// static
mojo_base::mojom::ThreadPriority
EnumTraits<mojo_base::mojom::ThreadPriority, base::ThreadPriority>::ToMojom(
    base::ThreadPriority thread_priority) {
  switch (thread_priority) {
    case base::ThreadPriority::BACKGROUND:
      return mojo_base::mojom::ThreadPriority::BACKGROUND;
    case base::ThreadPriority::NORMAL:
      return mojo_base::mojom::ThreadPriority::NORMAL;
    case base::ThreadPriority::DISPLAY:
      return mojo_base::mojom::ThreadPriority::DISPLAY;
    case base::ThreadPriority::REALTIME_AUDIO:
      return mojo_base::mojom::ThreadPriority::REALTIME_AUDIO;
  }
  NOTREACHED();
  return mojo_base::mojom::ThreadPriority::BACKGROUND;
}

// static
bool EnumTraits<mojo_base::mojom::ThreadPriority, base::ThreadPriority>::
    FromMojom(mojo_base::mojom::ThreadPriority input,
              base::ThreadPriority* out) {
  switch (input) {
    case mojo_base::mojom::ThreadPriority::BACKGROUND:
      *out = base::ThreadPriority::BACKGROUND;
      return true;
    case mojo_base::mojom::ThreadPriority::NORMAL:
      *out = base::ThreadPriority::NORMAL;
      return true;
    case mojo_base::mojom::ThreadPriority::DISPLAY:
      *out = base::ThreadPriority::DISPLAY;
      return true;
    case mojo_base::mojom::ThreadPriority::REALTIME_AUDIO:
      *out = base::ThreadPriority::REALTIME_AUDIO;
      return true;
  }
  return false;
}

}  // namespace mojo
