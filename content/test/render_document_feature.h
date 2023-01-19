// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RENDER_DOCUMENT_FEATURE_H_
#define CONTENT_TEST_RENDER_DOCUMENT_FEATURE_H_

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class ScopedFeatureList;
}  // namespace test
}  // namespace base

namespace content {

void InitAndEnableRenderDocumentFeature(
    base::test::ScopedFeatureList* feature_list,
    std::string level);

// The list of values to test for the "level" parameter.
std::vector<std::string> RenderDocumentFeatureLevelValues();

// Returns a list containing only the highest level of RenderDocument enabled
// in the "level" parameter. This is useful for RenderDocument tests that expect
// to enable the mode via this parameter, even if this is the only mode being
// tested.
std::vector<std::string> RenderDocumentFeatureFullyEnabled();

// Returns the name for |render_document_level| that's valid for test params
// (only contains alphanumeric characters or underscores).
std::string GetRenderDocumentLevelNameForTestParams(
    std::string render_document_level);

}  // namespace content

#endif  // CONTENT_TEST_RENDER_DOCUMENT_FEATURE_H_
