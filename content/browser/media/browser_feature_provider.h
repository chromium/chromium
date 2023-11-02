// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_BROWSER_FEATURE_PROVIDER_H_
#define CONTENT_BROWSER_MEDIA_BROWSER_FEATURE_PROVIDER_H_

#include "media/learning/impl/feature_provider.h"

namespace content {

// FeatureProvider implementation that handles the features labelled 'browser'
// in //media/learning/common/feature_library.h .
// TODO(liberato): It's likely that this should not be in media/, but rather in
// learning/browser.  This should be refactored if the learning experiment moves
// into its own component.
class BrowserFeatureProvider : public ::media::learning::FeatureProvider {
 public:
  BrowserFeatureProvider(const ::media::learning::LearningTask& task);
  ~BrowserFeatureProvider() override;

  static ::media::learning::SequenceBoundFeatureProvider Create(
      const ::media::learning::LearningTask& task);

  static ::media::learning::FeatureProviderFactoryCB GetFactoryCB();

  // FeatureProvider
  void AddFeatures(::media::learning::FeatureVector features,
                   FeatureVectorCB cb) override;

 private:
  ::media::learning::LearningTask task_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_BROWSER_FEATURE_PROVIDER_H_
