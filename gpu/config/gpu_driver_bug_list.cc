// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_driver_bug_list.h"

#include "base/check_op.h"
#include "gpu/config/gpu_driver_bug_list_autogen.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"

namespace gpu {

namespace {

struct GpuDriverBugWorkaroundInfo {
  GpuDriverBugWorkaroundType type;
  const char* name;
};

const std::array<GpuDriverBugWorkaroundInfo,
                 NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES>
    kFeatureList = {{
#define GPU_OP(type, name) { type, #name },
        GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
    }};

}  // namespace anonymous

GpuDriverBugList::GpuDriverBugList(base::span<const GpuControlList::Entry> data)
    : GpuControlList(data) {}

GpuDriverBugList::~GpuDriverBugList() = default;

// static
std::unique_ptr<GpuDriverBugList> GpuDriverBugList::Create() {
  return Create(GetGpuDriverBugListEntries());
}

// static
std::unique_ptr<GpuDriverBugList> GpuDriverBugList::Create(
    base::span<const GpuControlList::Entry> data) {
  std::unique_ptr<GpuDriverBugList> list(new GpuDriverBugList(data));

  DCHECK_EQ(NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES, kFeatureList.size());
  for (const auto& feature : kFeatureList) {
    list->AddSupportedFeature(feature.name, feature.type);
  }
  return list;
}

std::string GpuDriverBugWorkaroundTypeToString(
    GpuDriverBugWorkaroundType type) {
  if (type < NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES)
    return kFeatureList[type].name;
  else
    return "unknown";
}

// static
void GpuDriverBugList::AppendWorkaroundsFromCommandLine(
    std::set<int>* workarounds,
    const base::CommandLine& command_line) {
  DCHECK(workarounds);
  for (const auto& feature : kFeatureList) {
    if (command_line.HasSwitch(feature.name)) {
      // Check for disabling workaround flag.
      if (command_line.GetSwitchValueASCII(feature.name) == "0") {
        workarounds->erase(feature.type);
        continue;
      }

      // Removing conflicting workarounds.
      switch (feature.type) {
        case FORCE_HIGH_PERFORMANCE_GPU:
          workarounds->erase(FORCE_LOW_POWER_GPU);
          workarounds->insert(FORCE_HIGH_PERFORMANCE_GPU);
          break;
        case FORCE_LOW_POWER_GPU:
          workarounds->erase(FORCE_HIGH_PERFORMANCE_GPU);
          workarounds->insert(FORCE_LOW_POWER_GPU);
          break;
        default:
          workarounds->insert(feature.type);
          break;
      }
    }
  }
}

// static
void GpuDriverBugList::AppendAllWorkarounds(
    std::vector<const char*>* workarounds) {
  DCHECK_EQ(NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES, kFeatureList.size());
  DCHECK(workarounds->empty());
  workarounds->resize(NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES);
  size_t i = 0;
  for (const auto& feature : kFeatureList) {
    (*workarounds)[i++] = feature.name;
  }
}

// static
bool GpuDriverBugList::AreEntryIndicesValid(
    const std::vector<uint32_t>& entry_indices) {
  return GpuControlList::AreEntryIndicesValid(
      entry_indices, GetGpuDriverBugListEntries().size());
}

}  // namespace gpu
