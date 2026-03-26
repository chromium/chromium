// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/process_priority_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
mojo_base::mojom::ProcessPriority
EnumTraits<mojo_base::mojom::ProcessPriority, base::Process::Priority>::ToMojom(
    base::Process::Priority input) {
  switch (input) {
    case base::Process::Priority::kBestEffort:
      return mojo_base::mojom::ProcessPriority::kBestEffort;
    case base::Process::Priority::kUserVisible:
      return mojo_base::mojom::ProcessPriority::kUserVisible;
    case base::Process::Priority::kUserBlocking:
      return mojo_base::mojom::ProcessPriority::kUserBlocking;
  }
  NOTREACHED();
}

// static
base::Process::Priority
EnumTraits<mojo_base::mojom::ProcessPriority, base::Process::Priority>::
    FromMojom(mojo_base::mojom::ProcessPriority input) {
  switch (input) {
    case mojo_base::mojom::ProcessPriority::kBestEffort:
      return base::Process::Priority::kBestEffort;
    case mojo_base::mojom::ProcessPriority::kUserVisible:
      return base::Process::Priority::kUserVisible;
    case mojo_base::mojom::ProcessPriority::kUserBlocking:
      return base::Process::Priority::kUserBlocking;
  }
  NOTREACHED();
}

}  // namespace mojo
