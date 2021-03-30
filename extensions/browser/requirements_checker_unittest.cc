// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/requirements_checker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/gpu_data_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/preload_check.h"
#include "extensions/browser/preload_check_test_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Whether this build supports the window.shape requirement.
const bool kSupportsWindowShape =
#if defined(USE_AURA)
    true;
#else
    false;
#endif

// Returns true if a WebGL check might not fail immediately.
bool MightSupportWebGL() {
  return content::GpuDataManager::GetInstance()->GpuAccessAllowed(nullptr);
}

const char kFeaturesKey[] = "requirements.3D.features";
const char kFeatureWebGL[] = "webgl";
const char kFeatureCSS3d[] = "css3d";

}  // namespace

class RequirementsCheckerTest : public ExtensionsTest {
 public:
  RequirementsCheckerTest() {
    manifest_dict_ = std::make_unique<base::DictionaryValue>();
  }

  ~RequirementsCheckerTest() override {}

  void CreateExtension() {
    manifest_dict_->SetString("name", "dummy name");
    manifest_dict_->SetString("version", "1");
    manifest_dict_->SetInteger("manifest_version", 2);

    std::string error;
    extension_ =
        Extension::Create(base::FilePath(), mojom::ManifestLocation::kUnpacked,
                          *manifest_dict_, Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension_.get()) << error;
  }

 protected:
  void StartChecker() {
    checker_ = std::make_unique<RequirementsChecker>(extension_);
    // TODO(michaelpg): This should normally not have to be async. Use Run()
    // instead of RunUntilComplete() after crbug.com/708354 is addressed.
    runner_.RunUntilComplete(checker_.get());
  }

  void RequireWindowShape() {
    manifest_dict_->SetBoolean("requirements.window.shape", true);
  }

  void RequireFeature(const char feature[]) {
    if (!manifest_dict_->HasKey(kFeaturesKey))
      manifest_dict_->Set(kFeaturesKey, std::make_unique<base::ListValue>());
    base::ListValue* features_list = nullptr;
    ASSERT_TRUE(manifest_dict_->GetList(kFeaturesKey, &features_list));
    features_list->AppendString(feature);
  }

  std::unique_ptr<RequirementsChecker> checker_;
  PreloadCheckRunner runner_;

 private:
  scoped_refptr<Extension> extension_;
  std::unique_ptr<base::DictionaryValue> manifest_dict_;
};

// Tests no requirements.
TEST_F(RequirementsCheckerTest, RequirementsEmpty) {
  CreateExtension();
  StartChecker();
  EXPECT_TRUE(runner_.called());
  EXPECT_EQ(0u, runner_.errors().size());
  EXPECT_TRUE(checker_->GetErrorMessage().empty());
}

// Tests fulfilled requirements.
TEST_F(RequirementsCheckerTest, RequirementsSuccess) {
  if (kSupportsWindowShape)
    RequireWindowShape();

  RequireFeature(kFeatureCSS3d);

  CreateExtension();
  StartChecker();
  EXPECT_TRUE(runner_.called());
  EXPECT_EQ(0u, runner_.errors().size());
  EXPECT_TRUE(checker_->GetErrorMessage().empty());
}

// Tests multiple requirements failing (on some builds).
TEST_F(RequirementsCheckerTest, RequirementsFailMultiple) {
  size_t expected_errors = 0u;
  if (!kSupportsWindowShape) {
    RequireWindowShape();
    expected_errors++;
  }
  if (!MightSupportWebGL()) {
    RequireFeature(kFeatureWebGL);
    expected_errors++;
  }
  // css3d should always succeed.
  RequireFeature(kFeatureCSS3d);

  CreateExtension();
  StartChecker();
  EXPECT_TRUE(runner_.called());
  EXPECT_EQ(expected_errors, runner_.errors().size());
  EXPECT_EQ(expected_errors == 0, checker_->GetErrorMessage().empty());
}

// Tests a requirement that might fail asynchronously.
TEST_F(RequirementsCheckerTest, RequirementsFailWebGL) {
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();
  RequireFeature(kFeatureWebGL);
  CreateExtension();
  StartChecker();

  // TODO(michaelpg): Check that the runner actually finishes, which requires
  // waiting for the GPU check to succeed: crbug.com/706204.
  if (runner_.errors().size()) {
    EXPECT_THAT(runner_.errors(), testing::UnorderedElementsAre(
                                      PreloadCheck::WEBGL_NOT_SUPPORTED));
    EXPECT_FALSE(checker_->GetErrorMessage().empty());
  }
}

}  // namespace extensions
