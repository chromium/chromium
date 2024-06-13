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
bool EnumTraits<mojo_base::mojom::ProcessPriority, base::Process::Priority>::
    FromMojom(mojo_base::mojom::ProcessPriority input,
              base::Process::Priority* output) {
  switch (input) {
    case mojo_base::mojom::ProcessPriority::kBestEffort:
      *output = base::Process::Priority::kBestEffort;
      return true;
    case mojo_base::mojom::ProcessPriority::kUserVisible:
      *output = base::Process::Priority::kUserVisible;
      return true;
    case mojo_base::mojom::ProcessPriority::kUserBlocking:
      *output = base::Process::Priority::kUserBlocking;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
