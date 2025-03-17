// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/thread_mojom_traits.h"

#include "base/bit_cast.h"
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
bool EnumTraits<viz::mojom::ThreadType, viz::Thread::Type>::FromMojom(
    viz::mojom::ThreadType input,
    viz::Thread::Type* out) {
  switch (input) {
    case viz::mojom::ThreadType::kMain:
      *out = viz::Thread::Type::kMain;
      return true;
    case viz::mojom::ThreadType::kIO:
      *out = viz::Thread::Type::kIO;
      return true;
    case viz::mojom::ThreadType::kCompositor:
      *out = viz::Thread::Type::kCompositor;
      return true;
    case viz::mojom::ThreadType::kVideo:
      *out = viz::Thread::Type::kVideo;
      return true;
    case viz::mojom::ThreadType::kOther:
      *out = viz::Thread::Type::kOther;
      return true;
  }
  return false;
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
