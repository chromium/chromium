// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_FEATURE_PROVIDER_H_
#define MEDIA_LEARNING_IMPL_FEATURE_PROVIDER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task.h"

namespace media {
namespace learning {

// Add features to a training example.  If the LearningTask's feature
// description includes feature names that a FeatureProvider knows about, then
// it will replace their value in the examples with whatever value that feature
// should have.  For example, "NetworkType" might be replaced by a value that
// indicates the type of network connection.
class COMPONENT_EXPORT(LEARNING_IMPL) FeatureProvider {
 public:
  using FeatureVectorCB = base::OnceCallback<void(FeatureVector)>;

  FeatureProvider();

  FeatureProvider(const FeatureProvider&) = delete;
  FeatureProvider& operator=(const FeatureProvider&) = delete;

  virtual ~FeatureProvider();

  // Update |features| to include whatever features are specified by |task_|,
  // and call |cb| once they're filled in.
  virtual void AddFeatures(FeatureVector features, FeatureVectorCB cb) = 0;
};

// Since FeatureProviders are often going to thread-hop, provide this typedef.
using SequenceBoundFeatureProvider = base::SequenceBound<FeatureProvider>;

// Factory callback, since things that create implementations will likely be
// elsewhere (e.g., content/) from the things which use them (e.g., here).  May
// return an empty provider if the task doesn't require one.
using FeatureProviderFactoryCB =
    base::RepeatingCallback<SequenceBoundFeatureProvider(const LearningTask&)>;

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_FEATURE_PROVIDER_H_
