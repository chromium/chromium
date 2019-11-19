// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main idea in Courgette is to do patching *under a tranformation*.  The
// input is transformed into a new representation, patching occurs in the new
// repesentation, and then the tranform is reversed to get the patched data.
//
// The idea is applied to pieces (or 'elements') of the whole (or 'ensemble').
// Each of the elements has to go through the same set of steps in lock-step.

// This file contains the code to create the patch.


#include "courgette/ensemble.h"

#include <stddef.h>

#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"

#include "courgette/crc.h"
#include "courgette/difference_estimator.h"
#include "courgette/patch_generator_x86_32.h"
#include "courgette/patcher_x86_32.h"
#include "courgette/region.h"
#include "courgette/simple_delta.h"
#include "courgette/streams.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace courgette {

TransformationPatchGenerator::TransformationPatchGenerator(
    Element* old_element,
    Element* new_element,
    TransformationPatcher* patcher)
    : old_element_(old_element),
      new_element_(new_element),
      patcher_(patcher) {
}

TransformationPatchGenerator::~TransformationPatchGenerator() {
  delete patcher_;
}

// The default implementation of PredictTransformParameters delegates to the
// patcher.
Status TransformationPatchGenerator::PredictTransformParameters(
    SinkStreamSet* prediction) {
  return patcher_->PredictTransformParameters(prediction);
}

// The default implementation of Reform delegates to the patcher.
Status TransformationPatchGenerator::Reform(
    SourceStreamSet* transformed_element,
    SinkStream* reformed_element) {
  return patcher_->Reform(transformed_element, reformed_element);
}

// Makes a TransformationPatchGenerator of the appropriate variety for the
// Element kind.
TransformationPatchGenerator* MakeGenerator(Element* old_element,
                                            Element* new_element) {
  switch (new_element->kind()) {
    case EXE_UNKNOWN:
      break;
    case EXE_WIN_32_X86: {
      TransformationPatchGenerator* generator =
          new PatchGeneratorX86_32(
              old_element,
              new_element,
              new PatcherX86_32(old_element->region()),
              EXE_WIN_32_X86);
      return generator;
    }
    case EXE_ELF_32_X86: {
      TransformationPatchGenerator* generator =
          new PatchGeneratorX86_32(
              old_element,
              new_element,
              new PatcherX86_32(old_element->region()),
              EXE_ELF_32_X86);
      return generator;
    }
    case EXE_ELF_32_ARM: {
      TransformationPatchGenerator* generator =
          new PatchGeneratorX86_32(
              old_element,
              new_element,
              new PatcherX86_32(old_element->region()),
              EXE_ELF_32_ARM);
      return generator;
    }
    case EXE_WIN_32_X64: {
      TransformationPatchGenerator* generator =
          new PatchGeneratorX86_32(
              old_element,
              new_element,
              new PatcherX86_32(old_element->region()),
              EXE_WIN_32_X64);
      return generator;
    }
  }

  LOG(WARNING) << "Unexpected Element::Kind " << old_element->kind();
  return nullptr;
}

// Checks to see if the proposed comparison is 'unsafe'.  Sometimes one element
// from 'old' is matched as the closest element to multiple elements from 'new'.
// Each time this happens, the old element is transformed and serialized.  This
// is a problem when the old element is huge compared with the new element
// because the mutliple serialized copies can be much bigger than the size of
// either ensemble.
//
// The right way to avoid this is to ensure any one element from 'old' is
// serialized once, which requires matching code in the patch application.
//
// This is a quick hack to avoid the problem by prohibiting a big difference in
// size between matching elements.
bool UnsafeDifference(Element* old_element, Element* new_element) {
  double kMaxBloat = 2.0;
  size_t kMinWorrysomeDifference = 2 << 20;  // 2MB
  size_t old_size = old_element->region().length();
  size_t new_size = new_element->region().length();
  size_t low_size = std::min(old_size, new_size);
  size_t high_size = std::max(old_size, new_size);
  if (high_size - low_size < kMinWorrysomeDifference) return false;
  if (high_size < low_size * kMaxBloat) return false;
  return true;
}

// FindGenerators finds TransformationPatchGenerators for the elements of
// |new_ensemble|.  For each element of |new_ensemble| we find the closest
// matching element from |old_ensemble| and use that as the basis for
// differential compression.  The elements have to be the same kind so as to
// support transformation into the same kind of 'new representation'.
//
Status FindGenerators(Ensemble* old_ensemble, Ensemble* new_ensemble,
                      std::vector<TransformationPatchGenerator*>* generators) {
  base::Time start_find_time = base::Time::Now();
  old_ensemble->FindEmbeddedElements();
  new_ensemble->FindEmbeddedElements();
  VLOG(1) << "done FindEmbeddedElements "
          << (base::Time::Now() - start_find_time).InSecondsF();

  std::vector<Element*> old_elements(old_ensemble->elements());
  std::vector<Element*> new_elements(new_ensemble->elements());

  VLOG(1) << "old has " << old_elements.size() << " elements";
  VLOG(1) << "new has " << new_elements.size() << " elements";

  DifferenceEstimator difference_estimator;
  std::vector<DifferenceEstimator::Base*> bases;

  base::Time start_bases_time = base::Time::Now();
  for (size_t i = 0;  i < old_elements.size();  ++i) {
    bases.push_back(
        difference_estimator.MakeBase(old_elements[i]->region()));
  }
  VLOG(1) << "done make bases "
          << (base::Time::Now() - start_bases_time).InSecondsF() << "s";

  for (size_t new_index = 0;  new_index < new_elements.size();  ++new_index) {
    Element* new_element = new_elements[new_index];
    DifferenceEstimator::Subject* new_subject =
        difference_estimator.MakeSubject(new_element->region());

    // Search through old elements to find the best match.
    //
    // TODO(sra): This is O(N x M), i.e. O(N^2) since old_ensemble and
    // new_ensemble probably have a very similar structure.  We can make the
    // search faster by making the comparison provided by DifferenceEstimator
    // more nuanced, returning early if the measured difference is greater than
    // the current best.  This will be most effective if we can arrange that the
    // first elements we try to match are likely the 'right' ones.  We could
    // prioritize elements that are of a similar size or similar position in the
    // sequence of elements.
    //
    Element* best_old_element = nullptr;
    size_t best_difference = std::numeric_limits<size_t>::max();
    for (size_t old_index = 0;  old_index < old_elements.size();  ++old_index) {
      Element* old_element = old_elements[old_index];
      // Elements of different kinds are incompatible.
      if (old_element->kind() != new_element->kind())
        continue;

      if (UnsafeDifference(old_element, new_element))
        continue;

      base::Time start_compare = base::Time::Now();
      DifferenceEstimator::Base* old_base = bases[old_index];
      size_t difference = difference_estimator.Measure(old_base, new_subject);

      VLOG(1) << "Compare " << old_element->Name()
              << " to " << new_element->Name()
              << " --> " << difference
              << " in " << (base::Time::Now() - start_compare).InSecondsF()
              << "s";
      if (difference == 0) {
        VLOG(1) << "Skip " << new_element->Name()
                << " - identical to " << old_element->Name();
        best_difference = 0;
        best_old_element = nullptr;
        break;
      }
      if (difference < best_difference) {
        best_difference = difference;
        best_old_element = old_element;
      }
    }

    if (best_old_element) {
      VLOG(1) << "Matched " << best_old_element->Name()
              << " to " << new_element->Name()
              << " --> " << best_difference;
      TransformationPatchGenerator* generator =
          MakeGenerator(best_old_element, new_element);
      if (generator)
        generators->push_back(generator);
    }
  }

  VLOG(1) << "done FindGenerators found " << generators->size()
          << " in " << (base::Time::Now() - start_find_time).InSecondsF()
          << "s";

  return C_OK;
}

void FreeGenerators(std::vector<TransformationPatchGenerator*>* generators) {
  for (size_t i = 0;  i < generators->size();  ++i) {
    delete (*generators)[i];
  }
  generators->clear();
}

////////////////////////////////////////////////////////////////////////////////

Status GenerateEnsemblePatch(SourceStream* base,
                             SourceStream* update,
                             SinkStream* final_patch) {
  VLOG(1) << "start GenerateEnsemblePatch";
  base::Time start_time = base::Time::Now();

  Region old_region(base->Buffer(), base->Remaining());
  Region new_region(update->Buffer(), update->Remaining());
  Ensemble old_ensemble(old_region, "old");
  Ensemble new_ensemble(new_region, "new");
  std::vector<TransformationPatchGenerator*> generators;
  Status generators_status = FindGenerators(&old_ensemble, &new_ensemble,
                                            &generators);
  if (generators_status != C_OK)
    return generators_status;

  SinkStreamSet patch_streams;

  SinkStream* tranformation_descriptions      = patch_streams.stream(0);
  SinkStream* parameter_correction            = patch_streams.stream(1);
  SinkStream* transformed_elements_correction = patch_streams.stream(2);
  SinkStream* ensemble_correction             = patch_streams.stream(3);

  size_t number_of_transformations = generators.size();
  if (!tranformation_descriptions->WriteSizeVarint32(number_of_transformations))
    return C_STREAM_ERROR;

  for (size_t i = 0;  i < number_of_transformations;  ++i) {
    ExecutableType kind = generators[i]->Kind();
    if (!tranformation_descriptions->WriteVarint32(kind))
      return C_STREAM_ERROR;
  }

  for (size_t i = 0;  i < number_of_transformations;  ++i) {
    Status status =
        generators[i]->WriteInitialParameters(tranformation_descriptions);
    if (status != C_OK)
      return status;
  }

  //
  // Generate sub-patch for parameters.
  //
  SinkStreamSet predicted_parameters_sink;
  SinkStreamSet corrected_parameters_sink;

  for (size_t i = 0;  i < number_of_transformations;  ++i) {
    SinkStreamSet single_predicted_parameters;
    Status status;
    status = generators[i]->PredictTransformParameters(
        &single_predicted_parameters);
    if (status != C_OK)
      return status;
    if (!predicted_parameters_sink.WriteSet(&single_predicted_parameters))
      return C_STREAM_ERROR;

    SinkStreamSet single_corrected_parameters;
    status = generators[i]->CorrectedTransformParameters(
        &single_corrected_parameters);
    if (status != C_OK)
      return status;
    if (!corrected_parameters_sink.WriteSet(&single_corrected_parameters))
      return C_STREAM_ERROR;
  }

  SinkStream linearized_predicted_parameters;
  SinkStream linearized_corrected_parameters;

  if (!predicted_parameters_sink.CopyTo(&linearized_predicted_parameters))
    return C_STREAM_ERROR;
  if (!corrected_parameters_sink.CopyTo(&linearized_corrected_parameters))
    return C_STREAM_ERROR;

  SourceStream predicted_parameters_source;
  SourceStream corrected_parameters_source;
  predicted_parameters_source.Init(linearized_predicted_parameters);
  corrected_parameters_source.Init(linearized_corrected_parameters);

  Status delta1_status = GenerateSimpleDelta(&predicted_parameters_source,
                                             &corrected_parameters_source,
                                             parameter_correction);
  if (delta1_status != C_OK)
    return delta1_status;

  //
  // Generate sub-patch for elements.
  //
  corrected_parameters_source.Init(linearized_corrected_parameters);
  SourceStreamSet corrected_parameters_source_set;
  if (!corrected_parameters_source_set.Init(&corrected_parameters_source))
    return C_STREAM_ERROR;

  SinkStreamSet predicted_transformed_elements;
  SinkStreamSet corrected_transformed_elements;

  for (size_t i = 0;  i < number_of_transformations;  ++i) {
    SourceStreamSet single_parameters;
    if (!corrected_parameters_source_set.ReadSet(&single_parameters))
      return C_STREAM_ERROR;
    SinkStreamSet single_predicted_transformed_element;
    SinkStreamSet single_corrected_transformed_element;
    Status status = generators[i]->Transform(
        &single_parameters,
        &single_predicted_transformed_element,
        &single_corrected_transformed_element);
    if (status != C_OK)
      return status;
    if (!single_parameters.Empty())
      return C_STREAM_NOT_CONSUMED;
    if (!predicted_transformed_elements.WriteSet(
            &single_predicted_transformed_element))
      return C_STREAM_ERROR;
    if (!corrected_transformed_elements.WriteSet(
            &single_corrected_transformed_element))
      return C_STREAM_ERROR;
  }

  if (!corrected_parameters_source_set.Empty())
    return C_STREAM_NOT_CONSUMED;

  SinkStream linearized_predicted_transformed_elements;
  SinkStream linearized_corrected_transformed_elements;

  if (!predicted_transformed_elements.CopyTo(
          &linearized_predicted_transformed_elements))
    return C_STREAM_ERROR;
  if (!corrected_transformed_elements.CopyTo(
          &linearized_corrected_transformed_elements))
    return C_STREAM_ERROR;

  SourceStream predicted_transformed_elements_source;
  SourceStream corrected_transformed_elements_source;
  predicted_transformed_elements_source
      .Init(linearized_predicted_transformed_elements);
  corrected_transformed_elements_source
      .Init(linearized_corrected_transformed_elements);

  Status delta2_status =
      GenerateSimpleDelta(&predicted_transformed_elements_source,
                          &corrected_transformed_elements_source,
                          transformed_elements_correction);
  if (delta2_status != C_OK)
    return delta2_status;

  // Last use, free storage.
  linearized_predicted_transformed_elements.Retire();

  //
  // Generate sub-patch for whole enchilada.
  //
  SinkStream predicted_ensemble;

  if (!predicted_ensemble.Write(base->Buffer(), base->Remaining()))
    return C_STREAM_ERROR;

  SourceStreamSet corrected_transformed_elements_source_set;
  corrected_transformed_elements_source
      .Init(linearized_corrected_transformed_elements);
  if (!corrected_transformed_elements_source_set
      .Init(&corrected_transformed_elements_source))
    return C_STREAM_ERROR;

  for (size_t i = 0;  i < number_of_transformations;  ++i) {
    SourceStreamSet single_corrected_transformed_element;
    if (!corrected_transformed_elements_source_set.ReadSet(
            &single_corrected_transformed_element))
      return C_STREAM_ERROR;
    Status status = generators[i]->Reform(&single_corrected_transformed_element,
                                          &predicted_ensemble);
    if (status != C_OK)
      return status;
    if (!single_corrected_transformed_element.Empty())
      return C_STREAM_NOT_CONSUMED;
  }

  if (!corrected_transformed_elements_source_set.Empty())
    return C_STREAM_NOT_CONSUMED;

  // No more references to this stream's buffer.
  linearized_corrected_transformed_elements.Retire();

  FreeGenerators(&generators);

  size_t final_patch_input_size = predicted_ensemble.Length();
  SourceStream predicted_ensemble_source;
  predicted_ensemble_source.Init(predicted_ensemble);
  Status delta3_status = GenerateSimpleDelta(&predicted_ensemble_source,
                                             update,
                                             ensemble_correction);
  if (delta3_status != C_OK)
    return delta3_status;

  //
  // Final output stream has a header followed by a StreamSet.
  //
  if (!final_patch->WriteVarint32(CourgettePatchFile::kMagic) ||
      !final_patch->WriteVarint32(CourgettePatchFile::kVersion) ||
      !final_patch->WriteVarint32(CalculateCrc(old_region.start(),
                                               old_region.length())) ||
      !final_patch->WriteVarint32(CalculateCrc(new_region.start(),
                                               new_region.length())) ||
      !final_patch->WriteSizeVarint32(final_patch_input_size) ||
      !patch_streams.CopyTo(final_patch)) {
    return C_STREAM_ERROR;
  }

  VLOG(1) << "done GenerateEnsemblePatch "
          << (base::Time::Now() - start_time).InSecondsF() << "s";

  return C_OK;
}

}  // namespace
