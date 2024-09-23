// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_

#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

namespace extensions {

class ManifestFeature : public SimpleFeature {
 public:
  ManifestFeature();
  ~ManifestFeature() override;

  // TODO(crbug.com/40689631): This should also override IsAvailableToManifest
  // so that a permission or manifest feature can declare dependency on other
  // manifest features.

 protected:
  Feature::Availability IsAvailableToContextImpl(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Feature::Platform platform,
      int context_id,
      bool check_developer_mode,
      const ContextData& context_data) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_
