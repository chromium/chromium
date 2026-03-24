// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/thread_mojom_traits.h"

#include "base/bit_cast.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"

namespace mojo {

// static
viz::mojom::ThreadType
EnumTraits<viz::mojom::ThreadType, viz::Thread::Type>::ToMojom(
    viz::Thread::Type type) {
  switch (type) {
    case viz::Thread::Type::kMain:
      return viz::mojom::ThreadType::kMain;
    case viz::Thread::Type::kIO:
      return viz::mojom::ThreadType::kIO;
    case viz::Thread::Type::kCompositor:
      return viz::mojom::ThreadType::kCompositor;
    case viz::Thread::Type::kVideo:
      return viz::mojom::ThreadType::kVideo;
    case viz::Thread::Type::kOther:
      return viz::mojom::ThreadType::kOther;
  }
  NOTREACHED();
}

// static
viz::Thread::Type
EnumTraits<viz::mojom::ThreadType, viz::Thread::Type>::FromMojom(
    viz::mojom::ThreadType input) {
  switch (input) {
    case viz::mojom::ThreadType::kMain:
      return viz::Thread::Type::kMain;
    case viz::mojom::ThreadType::kIO:
      return viz::Thread::Type::kIO;
    case viz::mojom::ThreadType::kCompositor:
      return viz::Thread::Type::kCompositor;
    case viz::mojom::ThreadType::kVideo:
      return viz::Thread::Type::kVideo;
    case viz::mojom::ThreadType::kOther:
      return viz::Thread::Type::kOther;
  }
  NOTREACHED();
}

// static
bool StructTraits<viz::mojom::ThreadDataView, viz::Thread>::Read(
    viz::mojom::ThreadDataView data,
    viz::Thread* out) {
  if (!data.ReadType(&out->type)) {
    return false;
  }
  // Bit cast the data to base::PlatformThreadId::UnderlyingType. We do a
  // bitcast instead of trying to match the exact type in mojo because:
  //
  //   1. We'd have to define the mojo type to match the sign, which is possible
  //   but unnecessary,
  //   2. We bit_cast instead of static_cast to explicitly consider only the
  //   size of the transfer medium as semantically important (and get a size
  //   check for free),
  //   3. Even if we did all the above, it's awkward on Windows, because the
  //   base::PlatformThreadId::UnderlyingType is `unsigned long`, which is a
  //   type of the same size but distinct identity to `unsigned int`.
  out->id = base::PlatformThreadId(
      base::bit_cast<base::PlatformThreadId::UnderlyingType>(data.id()));
  return true;
}

}  // namespace mojo
