// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_CONFIG_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_CONFIG_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "content/public/browser/preloading.h"

namespace content {

namespace test {
class PreloadingConfigOverride;
}  //  namespace test

class CONTENT_EXPORT PreloadingConfig {
 public:
  PreloadingConfig();
  ~PreloadingConfig();

  static PreloadingConfig& GetInstance();

  // Whether the given (|preloading_type|, |predictor|) combination should be
  // held back in order to evaluate how well this type of preloading is
  // performing. This is controlled via field trial configuration.
  bool ShouldHoldback(PreloadingType preloading_type,
                      PreloadingPredictor predictor);

  // Whether the given (|preloading_type|, |predictor|) combination logging
  // should be sampled. Some types of preloading trigger more than others so
  // we randomly drop logging for a fraction of page loads of the more noisy
  // preloading. The sampling rate is configured via field trial.
  double SamplingLikelihood(PreloadingType preloading_type,
                            PreloadingPredictor predictor);

  // Initializes the PreloadingConfig from the FeatureParams. Exported
  // publicly only for tests.
  void ParseConfig();

 private:
  friend class content::test::PreloadingConfigOverride;

  struct Key {
    Key(std::string_view preloading_type, std::string_view predictdor);
    static Key FromEnums(PreloadingType preloading_type,
                         PreloadingPredictor predictor);

    std::string preloading_type_;
    std::string predictor_;
  };

  struct Entry {
    static Entry FromDict(const base::Value::Dict* dict);

    bool holdback_ = false;
    float sampling_likelihood_ = 1.0;
  };

  struct KeyCompare {
    bool operator()(const Key& lhs, const Key& rhs) const;
  };

  // Overrides the PreloadingConfig for testing. Returns the previous override,
  // if any.  The caller is responsible for calling OverrideForTesting with the
  // previous value once they're done.
  static PreloadingConfig* OverrideForTesting(
      PreloadingConfig* config_override);

  // Sets whether the given feature should be held back (disabled) and prevents
  // sampling UKM logs for that feature.
  void SetHoldbackForTesting(PreloadingType preloading_type,
                             PreloadingPredictor predictor,
                             bool holdback);
  void SetHoldbackForTesting(std::string_view preloading_type,
                             std::string_view predictdor,
                             bool holdback);

  base::flat_map<Key, Entry, KeyCompare> entries_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_CONFIG_H_
