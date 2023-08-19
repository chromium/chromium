// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_suite.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "components/crash/core/common/objc_zombie.h"
#import "ios/testing/verify_custom_webkit.h"
#import "ios/web/public/navigation/url_schemes.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/resource/resource_bundle.h"

namespace web {

WebTestSuite::WebTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv),
      web_client_(std::make_unique<FakeWebClient>()) {
  CHECK(IsCustomWebKitLoadedIfRequested());
#if TARGET_IPHONE_SIMULATOR
  DCHECK(ObjcEvilDoers::ZombieEnable(true, 10000));
#endif
}

WebTestSuite::~WebTestSuite() {
  // Verify again at the end of the test run, in case some frameworks were not
  // yet loaded when the constructor ran.
  CHECK(IsCustomWebKitLoadedIfRequested());
}

void WebTestSuite::Initialize() {
  base::TestSuite::Initialize();

  RegisterWebSchemes();

  // Force unittests to run using en-US so if testing string output will work
  // regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  base::PathService::Get(base::DIR_ASSETS, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::kScaleFactorNone);
}

void WebTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace web
