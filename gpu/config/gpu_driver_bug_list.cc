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

const GpuDriverBugWorkaroundInfo kFeatureList[] = {
#define GPU_OP(type, name) { type, #name },
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
};

}  // namespace anonymous

GpuDriverBugList::GpuDriverBugList(base::span<const GpuControlList::Entry> data)
    : GpuControlList(data) {}

GpuDriverBugList::~GpuDriverBugList() = default;

// static
std::unique_ptr<GpuDriverBugList> GpuDriverBugList::Create() {
  return Create(
      base::make_span(kGpuDriverBugListEntries, kGpuDriverBugListEntryCount));
}

// static
std::unique_ptr<GpuDriverBugList> GpuDriverBugList::Create(
    base::span<const GpuControlList::Entry> data) {
  std::unique_ptr<GpuDriverBugList> list(new GpuDriverBugList(data));

  DCHECK_EQ(static_cast<int>(std::size(kFeatureList)),
            NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES);
  for (int i = 0; i < NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES; ++i) {
    list->AddSupportedFeature(kFeatureList[i].name,
                              kFeatureList[i].type);
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
  for (int i = 0; i < NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES; i++) {
    if (command_line.HasSwitch(kFeatureList[i].name)) {
      // Check for disabling workaround flag.
      if (command_line.GetSwitchValueASCII(kFeatureList[i].name) == "0") {
        workarounds->erase(kFeatureList[i].type);
        continue;
      }

      // Removing conflicting workarounds.
      switch (kFeatureList[i].type) {
        case FORCE_HIGH_PERFORMANCE_GPU:
          workarounds->erase(FORCE_LOW_POWER_GPU);
          workarounds->insert(FORCE_HIGH_PERFORMANCE_GPU);
          break;
        case FORCE_LOW_POWER_GPU:
          workarounds->erase(FORCE_HIGH_PERFORMANCE_GPU);
          workarounds->insert(FORCE_LOW_POWER_GPU);
          break;
        default:
          workarounds->insert(kFeatureList[i].type);
          break;
      }
    }
  }
}

// static
void GpuDriverBugList::AppendAllWorkarounds(
    std::vector<const char*>* workarounds) {
  static_assert(std::extent<decltype(kFeatureList)>::value ==
                    NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES,
                "Expected kFeatureList to include all gpu workarounds");

  DCHECK(workarounds->empty());
  workarounds->resize(NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES);
  size_t i = 0;
  for (const GpuDriverBugWorkaroundInfo& feature : kFeatureList)
    (*workarounds)[i++] = feature.name;
}

// static
bool GpuDriverBugList::AreEntryIndicesValid(
    const std::vector<uint32_t>& entry_indices) {
  return GpuControlList::AreEntryIndicesValid(entry_indices,
                                              kGpuDriverBugListEntryCount);
}

}  // namespace gpu
