// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_LIST_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_LIST_H_

#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "gpu/config/gpu_control_list.h"
#include "gpu/gpu_export.h"

namespace gpu {

class GPU_EXPORT GpuDriverBugList : public GpuControlList {
 public:
  GpuDriverBugList(const GpuDriverBugList&) = delete;
  GpuDriverBugList& operator=(const GpuDriverBugList&) = delete;

  ~GpuDriverBugList() override;

  static std::unique_ptr<GpuDriverBugList> Create();
  static std::unique_ptr<GpuDriverBugList> Create(
      base::span<const GpuControlList::Entry> data);

  // Append |workarounds| with these passed in through the
  // |command_line|.
  static void AppendWorkaroundsFromCommandLine(
      std::set<int>* workarounds,
      const base::CommandLine& command_line);

  // Append |workarounds| with the full list of workarounds.
  // This is needed for correctly passing flags down from
  // the browser process to the GPU process.
  static void AppendAllWorkarounds(std::vector<const char*>* workarounds);

  static bool AreEntryIndicesValid(const std::vector<uint32_t>& entry_indices);

 private:
  explicit GpuDriverBugList(base::span<const GpuControlList::Entry> data);
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_LIST_H_
