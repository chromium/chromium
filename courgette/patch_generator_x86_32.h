// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_PATCH_GENERATOR_X86_32_H_
#define COURGETTE_PATCH_GENERATOR_X86_32_H_

#include "base/logging.h"
#include "courgette/courgette_flow.h"
#include "courgette/ensemble.h"
#include "courgette/patcher_x86_32.h"

namespace courgette {

// PatchGeneratorX86_32 is the universal patch generator for all executables,
// performing transformation and adjustment. The executable type is determined
// by the program detector.
class PatchGeneratorX86_32 : public TransformationPatchGenerator {
 public:
  PatchGeneratorX86_32(Element* old_element,
                       Element* new_element,
                       PatcherX86_32* patcher,
                       ExecutableType kind)
      : TransformationPatchGenerator(old_element, new_element, patcher),
        kind_(kind) {
  }

  PatchGeneratorX86_32(const PatchGeneratorX86_32&) = delete;
  PatchGeneratorX86_32& operator=(const PatchGeneratorX86_32&) = delete;

  virtual ExecutableType Kind() { return kind_; }

  Status WriteInitialParameters(SinkStream* parameter_stream) {
    if (!parameter_stream->WriteSizeVarint32(
            old_element_->offset_in_ensemble()) ||
        !parameter_stream->WriteSizeVarint32(old_element_->region().length())) {
      return C_STREAM_ERROR;
    }
    return C_OK;
    // TODO(sra): Initialize |patcher_| with these parameters.
  }

  Status PredictTransformParameters(SinkStreamSet* prediction) {
    return TransformationPatchGenerator::PredictTransformParameters(prediction);
  }

  Status CorrectedTransformParameters(SinkStreamSet* parameters) {
    // No code needed to write an 'empty' parameter set.
    return C_OK;
  }

  // The format of a transformed_element is a serialized EncodedProgram. Steps:
  // - Form Disassembler for the old and new Elements.
  // - Extract AssemblyPrograms from old and new Disassemblers.
  // - Adjust the new AssemblyProgram to make it as much like the old one as
  //   possible.
  // - Serialize old and new Disassembler to EncodedProgram, using the old
  //   AssemblyProgram and the adjusted new AssemblyProgram.
  // The steps are performed in an order to reduce peak memory.
  Status Transform(SourceStreamSet* corrected_parameters,
                   SinkStreamSet* old_transformed_element,
                   SinkStreamSet* new_transformed_element) {
    // Don't expect any corrected parameters.
    if (!corrected_parameters->Empty())
      return C_GENERAL_ERROR;

    // Flow graph and process sequence (DA = Disassembler, AP = AssemblyProgram,
    // EP = EncodedProgram, Adj = Adjusted):
    //   [1 Old DA] --> [2 Old AP]   [6 New AP] <-- [5 New DA]
    //       |            |   |          |              |
    //       v            |   |          v (move)       v
    //   [3 Old EP] <-----+   +->[7 Adj New AP] --> [8 New EP]
    //   (4 Write)                                  (9 Write)
    CourgetteFlow flow;
    RegionBuffer old_buffer(old_element_->region());
    RegionBuffer new_buffer(new_element_->region());
    flow.ReadDisassemblerFromBuffer(flow.OLD, old_buffer);                  // 1
    flow.CreateAssemblyProgramFromDisassembler(flow.OLD, true);             // 2
    flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.OLD);  // 3
    flow.DestroyDisassembler(flow.OLD);
    flow.WriteSinkStreamSetFromEncodedProgram(flow.OLD,
                                              old_transformed_element);  // 4
    flow.DestroyEncodedProgram(flow.OLD);
    flow.ReadDisassemblerFromBuffer(flow.NEW, new_buffer);       // 5
    flow.CreateAssemblyProgramFromDisassembler(flow.NEW, true);  // 6
    flow.AdjustNewAssemblyProgramToMatchOld();                   // 7
    flow.DestroyAssemblyProgram(flow.OLD);
    flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.NEW);  // 8
    flow.DestroyAssemblyProgram(flow.NEW);
    flow.DestroyDisassembler(flow.NEW);
    flow.WriteSinkStreamSetFromEncodedProgram(flow.NEW,
                                              new_transformed_element);  // 9
    if (flow.failed()) {
      LOG(ERROR) << flow.message() << " (" << old_element_->Name() << " => "
                 << new_element_->Name() << ")";
    }
    return flow.status();
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    return TransformationPatchGenerator::Reform(transformed_element,
                                                reformed_element);
  }

 private:
  virtual ~PatchGeneratorX86_32() { }

  ExecutableType kind_;
};

}  // namespace courgette

#endif  // COURGETTE_PATCH_GENERATOR_X86_32_H_
