// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_FEATURES_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUSupportedFeatures : public ScriptWrappable,
                             public SetlikeIterable<String> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GPUSupportedFeatures();
  explicit GPUSupportedFeatures(const Vector<String>& feature_names);

  void AddFeatureName(const String& feature_name);

  bool hasForBinding(ScriptState* script_state,
                     const String& feature,
                     ExceptionState& exception_state) const;

  unsigned size() const { return features_.size(); }

  const HashSet<String>& FeatureNameSet() const { return features_; }

 private:
  HashSet<String> features_;

  class IterationSource final
      : public SetlikeIterable<String>::IterationSource {
   public:
    explicit IterationSource(const HashSet<String>& features);

    bool Next(ScriptState* script_state,
              String& key,
              String& value,
              ExceptionState& exception_state) override;

   private:
    HashSet<String> features_;
    HashSet<String>::iterator iter_;
  };

  // Starts iteration over the Setlike.
  // Needed for SetlikeIterable to work properly.
  GPUSupportedFeatures::IterationSource* StartIteration(
      ScriptState* script_state,
      ExceptionState& exception_state) override {
    return MakeGarbageCollected<GPUSupportedFeatures::IterationSource>(
        features_);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_FEATURES_H_
