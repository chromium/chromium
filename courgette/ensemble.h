// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main idea in Courgette is to do patching *under a tranformation*.  The
// input is transformed into a new representation, patching occurs in the new
// repesentation, and then the tranform is reversed to get the patched data.
//
// The idea is applied to pieces (or 'Elements') of the whole (or 'Ensemble').
// Each of the elements has to go through the same set of steps in lock-step,
// but there may be many different kinds of elements, which have different
// transformation.
//
// This file declares all the main types involved in creating and applying a
// patch with this structure.

#ifndef COURGETTE_ENSEMBLE_H_
#define COURGETTE_ENSEMBLE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "courgette/courgette.h"
#include "courgette/region.h"
#include "courgette/streams.h"

namespace courgette {

// Forward declarations:
class Ensemble;

// An Element is a region of an Ensemble with an identifyable kind.
//
class Element {
 public:
  Element(ExecutableType kind,
          Ensemble* ensemble,
          const Region& region);

  virtual ~Element();

  ExecutableType kind() const { return kind_; }
  const Region& region() const { return region_; }

  // The name is used only for debugging and logging.
  virtual std::string Name() const;

  // Returns the byte position of this Element relative to the start of
  // containing Ensemble.
  size_t offset_in_ensemble() const;

 private:
  ExecutableType kind_;
  Ensemble* ensemble_;
  Region region_;

  DISALLOW_COPY_AND_ASSIGN(Element);
};


class Ensemble {
 public:
  Ensemble(const Region& region, const char* name)
      : region_(region), name_(name) {}
  ~Ensemble();

  const Region& region() const { return region_; }
  const std::string& name() const { return name_; }

  // Scans the region to find Elements within the region().
  Status FindEmbeddedElements();

  // Returns the elements found by 'FindEmbeddedElements'.
  const std::vector<Element*>& elements() const { return elements_; }


 private:
  Region region_;       // The memory, owned by caller, containing the
                        // Ensemble's data.
  std::string name_;    // A debugging/logging name for the Ensemble.

  std::vector<Element*> elements_;        // Embedded elements discovered.
  std::vector<Element*> owned_elements_;  // For deallocation.

  DISALLOW_COPY_AND_ASSIGN(Ensemble);
};

inline size_t Element::offset_in_ensemble() const {
  return region().start() - ensemble_->region().start();
}

// The 'CourgettePatchFile' is class is a 'namespace' for the constants that
// appear in a Courgette patch file.
struct CourgettePatchFile {
  //
  // The Courgette patch format interleaves the data for N embedded Elements.
  //
  // Format of a patch file:
  //  header:
  //    magic
  //    version
  //    source-checksum
  //    target-checksum
  //    final-patch-input-size (an allocation hint)
  //  multiple-streams:
  //    stream 0:
  //      number-of-transformed-elements (N) - varint32
  //      transformation-1-method-id
  //      transformation-2-method-id
  //      ...
  //      transformation-1-initial-parameters
  //      transformation-2-initial-parameters
  //      ...
  //    stream 1:
  //      correction:
  //        transformation-1-parameters
  //        transformation-2-parameters
  //        ...
  //    stream 2:
  //      correction:
  //        transformed-element-1
  //        transformed-element-2
  //        ...
  //    stream 3:
  //      correction:
  //        base-file
  //        element-1
  //        element-2
  //        ...

  static const uint32_t kMagic = 'C' | ('o' << 8) | ('u' << 16);

  static const uint32_t kVersion = 20110216;
};

// For any transform you would implement both a TransformationPatcher and a
// TransformationPatchGenerator.
//
// TransformationPatcher is the interface which abstracts out the actual
// transformation used on an Element.  The patching itself happens outside the
// actions of a TransformationPatcher.  There are four steps.
//
// The first step is an Init step.  The parameters to the Init step identify the
// element, for example, range of locations within the original ensemble that
// correspond to the element.
//
// PredictTransformParameters, explained below.
//
// The two final steps are 'Transform' - to transform the element into a new
// representation, and to 'Reform' - to transform from the new representation
// back to the original form.
//
// The Transform step takes some parameters.  This allows the transform to be
// customized to the particular element, or to receive some assistance in the
// analysis required to perform the transform.  The transform parameters might
// be extensive but mostly predicable, so preceeding Transform is a
// PredictTransformParameters step.
//
class TransformationPatcher {
 public:
  virtual ~TransformationPatcher() {}

  // First step: provides parameters for the patching.  This would at a minimum
  // identify the element within the ensemble being patched.
  virtual Status Init(SourceStream* parameter_stream) = 0;

  // Second step: predicts transform parameters.
  virtual Status PredictTransformParameters(
      SinkStreamSet* predicted_parameters) = 0;

  // Third step: transforms element from original representation into alternate
  // representation.
  virtual Status Transform(SourceStreamSet* corrected_parameters,
                           SinkStreamSet* transformed_element) = 0;

  // Final step: transforms element back from alternate representation into
  // original representation.
  virtual Status Reform(SourceStreamSet* transformed_element,
                        SinkStream* reformed_element) = 0;
};

// TransformationPatchGenerator is the interface which abstracts out the actual
// transformation used (and adjustment used) when differentially compressing one
// Element from the |new_ensemble| against a corresponding element in the
// |old_ensemble|.
//
// This is not a pure interface.  There is a small amount of inheritance
// implementation for the fields and actions common to all
// TransformationPatchGenerators.
//
// When TransformationPatchGenerator is subclassed, there will be a
// corresponding subclass of TransformationPatcher.
//
class TransformationPatchGenerator {
 public:
  TransformationPatchGenerator(Element* old_element,
                               Element* new_element,
                               TransformationPatcher* patcher);

  virtual ~TransformationPatchGenerator();

  // Returns the TransformationMethodId that identies this transformation.
  virtual ExecutableType Kind() = 0;

  // Writes the parameters that will be passed to TransformationPatcher::Init.
  virtual Status WriteInitialParameters(SinkStream* parameter_stream) = 0;

  // Predicts the transform parameters for the |old_element|.  This must match
  // exactly the output that will be produced by the PredictTransformParameters
  // method of the corresponding subclass of TransformationPatcher.  This method
  // is not pure. The default implementation delegates to the patcher to
  // guarantee matching output.
  virtual Status PredictTransformParameters(SinkStreamSet* prediction);

  // Writes the desired parameters for the transform of the old element from the
  // file representation to the alternate representation.
  virtual Status CorrectedTransformParameters(SinkStreamSet* parameters) = 0;

  // Writes both |old_element| and |new_element| in the new representation.
  // |old_corrected_parameters| will match the |corrected_parameters| passed to
  // the Transform method of the corresponding sublcass of
  // TransformationPatcher.
  //
  // The output written to |old_transformed_element| must match exactly the
  // output written by the Transform method of the corresponding subclass of
  // TransformationPatcher.
  virtual Status Transform(SourceStreamSet* old_corrected_parameters,
                           SinkStreamSet* old_transformed_element,
                           SinkStreamSet* new_transformed_element) = 0;

  // Transforms the new transformed_element back from the alternate
  // representation into the original file format.  This must match exactly the
  // output that will be produced by the corresponding subclass of
  // TransformationPatcher::Reform.  This method is not pure. The default
  // implementation delegates to the patcher.
  virtual Status Reform(SourceStreamSet* transformed_element,
                        SinkStream* reformed_element);

 protected:
  Element* old_element_;
  Element* new_element_;
  TransformationPatcher* patcher_;
};

}  // namespace
#endif  // COURGETTE_ENSEMBLE_H_
