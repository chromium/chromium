// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

namespace extensions {

bool CpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<api::system_cpu::ProcessorInfo>* infos) {
  DCHECK(infos);
  // TODO(crbug.com/42050323): Integrate with platform APIs, when available.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

}  // namespace extensions
