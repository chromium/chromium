// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_suite.h"

#import "base/check.h"
#import "base/files/file_path.h"
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
#if TARGET_OS_SIMULATOR
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

  // Load the resources produced by //ios/web/test:packed_resources if the test
  // has not already loaded other resources (e.g., by //ios/chrome tests).
  if (!ui::ResourceBundle::HasSharedInstance()) {
    base::FilePath resources_pack_path =
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .Append(FILE_PATH_LITERAL("resources.pak"));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(resources_pack_path);
  }
}

void WebTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace web
