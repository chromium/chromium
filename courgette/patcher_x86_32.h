// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_PATCHER_X86_32_H_
#define COURGETTE_PATCHER_X86_32_H_

#include <stdint.h>

#include "base/logging.h"
#include "courgette/courgette_flow.h"
#include "courgette/ensemble.h"
#include "courgette/region.h"
#include "courgette/streams.h"

namespace courgette {

// PatcherX86_32 is the universal patcher for all executables. The executable
// type is determined by the program detector.
class PatcherX86_32 : public TransformationPatcher {
 public:
  explicit PatcherX86_32(const Region& region)
      : ensemble_region_(region),
        base_offset_(0),
        base_length_(0) {
  }

  PatcherX86_32(const PatcherX86_32&) = delete;
  PatcherX86_32& operator=(const PatcherX86_32&) = delete;

  Status Init(SourceStream* parameter_stream) {
    if (!parameter_stream->ReadVarint32(&base_offset_))
      return C_BAD_TRANSFORM;
    if (!parameter_stream->ReadVarint32(&base_length_))
      return C_BAD_TRANSFORM;

    if (base_offset_ > ensemble_region_.length())
      return C_BAD_TRANSFORM;
    if (base_length_ > ensemble_region_.length() - base_offset_)
      return C_BAD_TRANSFORM;

    return C_OK;
  }

  Status PredictTransformParameters(SinkStreamSet* predicted_parameters) {
    // No code needed to write an 'empty' predicted parameter set.
    return C_OK;
  }

  Status Transform(SourceStreamSet* corrected_parameters,
                   SinkStreamSet* transformed_element) {
    if (!corrected_parameters->Empty())
      return C_GENERAL_ERROR;  // Don't expect any corrected parameters.

    CourgetteFlow flow;
    RegionBuffer only_buffer(
        Region(ensemble_region_.start() + base_offset_, base_length_));
    flow.ReadDisassemblerFromBuffer(flow.ONLY, only_buffer);
    flow.CreateAssemblyProgramFromDisassembler(flow.ONLY, false);
    flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.ONLY);
    flow.DestroyAssemblyProgram(flow.ONLY);
    flow.DestroyDisassembler(flow.ONLY);
    flow.WriteSinkStreamSetFromEncodedProgram(flow.ONLY, transformed_element);
    if (flow.failed())
      LOG(ERROR) << flow.message();
    return flow.status();
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    CourgetteFlow flow;
    flow.ReadEncodedProgramFromSourceStreamSet(flow.ONLY, transformed_element);
    flow.WriteExecutableFromEncodedProgram(flow.ONLY, reformed_element);
    if (flow.failed())
      LOG(ERROR) << flow.message();
    return flow.status();
  }

 private:
  Region ensemble_region_;

  uint32_t base_offset_;
  uint32_t base_length_;
};

}  // namespace courgette

#endif  // COURGETTE_PATCHER_X86_32_H_
