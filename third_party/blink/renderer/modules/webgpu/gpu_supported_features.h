// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_FEATURES_H_

#include <bitset>

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sync_iterator_gpu_supported_features.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class GPUSupportedFeatures : public ScriptWrappable,
                             public ValueSyncIterable<GPUSupportedFeatures> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GPUSupportedFeatures();
  explicit GPUSupportedFeatures(const Vector<V8GPUFeatureName>& feature_names);

  void AddFeatureName(const V8GPUFeatureName feature_name);

  bool has(const String& feature) const;
  bool hasForBinding(ScriptState* script_state,
                     const String& feature,
                     ExceptionState& exception_state) const;

  // Fast path, it allows to avoid hash computation from string for
  // checking features.
  bool has(const V8GPUFeatureName::Enum feature) const;

  unsigned size() const { return features_.size(); }

  const HashSet<String>& FeatureNameSet() const { return features_; }

 private:
  HashSet<String> features_;

  // For fast path. Make a copy that is synched with features_ to allow
  // checking features with bitset test by V8GPUFeatureName::Enum.
  std::bitset<V8GPUFeatureName::kEnumSize> features_bitset_;

  class IterationSource final
      : public ValueSyncIterable<GPUSupportedFeatures>::IterationSource {
   public:
    explicit IterationSource(const HashSet<String>& features);

    bool FetchNextItem(ScriptState* script_state,
                       String& value,
                       ExceptionState& exception_state) override;

   private:
    HashSet<String> features_;
    HashSet<String>::iterator iter_;
  };

  // Starts iteration over the Setlike.
  // Needed for ValueSyncIterable to work properly.
  GPUSupportedFeatures::IterationSource* CreateIterationSource(
      ScriptState* script_state,
      ExceptionState& exception_state) override {
    return MakeGarbageCollected<GPUSupportedFeatures::IterationSource>(
        features_);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_FEATURES_H_
