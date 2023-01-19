// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/render_document_feature.h"

#include "base/test/scoped_feature_list.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/content_features.h"

namespace content {

void InitAndEnableRenderDocumentFeature(
    base::test::ScopedFeatureList* feature_list,
    std::string level) {
  std::map<std::string, std::string> parameters;
  parameters[kRenderDocumentLevelParameterName] = level;
  feature_list->InitAndEnableFeatureWithParameters(features::kRenderDocument,
                                                   parameters);
}

std::vector<std::string> RenderDocumentFeatureLevelValues() {
  // TODO(https://crbug.com/936696, rakina): Handle the
  // RenderDocumentLevel::kAllFrames case.
  return {
      GetRenderDocumentLevelName(RenderDocumentLevel::kCrashedFrame),
      GetRenderDocumentLevelName(RenderDocumentLevel::kSubframe),
  };
}

std::vector<std::string> RenderDocumentFeatureFullyEnabled() {
  return {
      GetRenderDocumentLevelName(RenderDocumentLevel::kAllFrames),
  };
}

std::string GetRenderDocumentLevelNameForTestParams(
    std::string render_document_level) {
  if (render_document_level ==
      GetRenderDocumentLevelName(RenderDocumentLevel::kCrashedFrame)) {
    return "RDCrashedFrame";
  }
  return "RDSubframe";
}

}  // namespace content
