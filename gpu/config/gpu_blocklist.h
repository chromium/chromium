// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_BLOCKLIST_H_
#define GPU_CONFIG_GPU_BLOCKLIST_H_

#include <memory>

#include "base/containers/span.h"
#include "gpu/config/gpu_control_list.h"

namespace gpu {

class GPU_EXPORT GpuBlocklist : public GpuControlList {
 public:
  GpuBlocklist(const GpuBlocklist&) = delete;
  GpuBlocklist& operator=(const GpuBlocklist&) = delete;

  ~GpuBlocklist() override;

  static std::unique_ptr<GpuBlocklist> Create();
  static std::unique_ptr<GpuBlocklist> Create(
      base::span<const GpuControlList::Entry> data);

  static bool AreEntryIndicesValid(const std::vector<uint32_t>& entry_indices);

 private:
  explicit GpuBlocklist(base::span<const GpuControlList::Entry> data);
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_BLOCKLIST_H_
