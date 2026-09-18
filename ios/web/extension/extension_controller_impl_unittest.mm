// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/extension/extension_controller_impl.h"

#import <WebKit/WebKit.h>

#import "base/functional/bind.h"
#import "base/test/test_future.h"
#import "ios/web/extension/gpc_web_extension/gpc_web_extension.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace web {

class ExtensionControllerImplTest : public WebTest {
 public:
  ExtensionControllerImplTest() = default;
  ~ExtensionControllerImplTest() override = default;
};

// Tests that a newly created ExtensionController has no built-in extension
// loaded.
TEST_F(ExtensionControllerImplTest, TestInitialState) API_AVAILABLE(ios(18.4)) {
  std::unique_ptr<ExtensionController> controller =
      ExtensionController::Create();
  ASSERT_TRUE(controller);
  EXPECT_FALSE(controller->IsBuiltInExtensionLoaded(BuiltInExtension::kGPC));
}

// Tests loading and unloading a built-in extension into
// ExtensionControllerImpl.
TEST_F(ExtensionControllerImplTest, TestLoadAndUnloadBuiltInExtension)
API_AVAILABLE(ios(18.4)) {
  std::unique_ptr<ExtensionController> controller =
      ExtensionController::Create();
  ASSERT_TRUE(controller);

  base::test::TestFuture<bool> load_future;
  controller->LoadBuiltInExtension(BuiltInExtension::kGPC,
                                   load_future.GetCallback());
  EXPECT_TRUE(load_future.Get());
  EXPECT_TRUE(controller->IsBuiltInExtensionLoaded(BuiltInExtension::kGPC));

  // Test unloading the built-in extension.
  base::test::TestFuture<bool> unload_future;
  controller->UnloadBuiltInExtension(BuiltInExtension::kGPC,
                                     unload_future.GetCallback());
  EXPECT_TRUE(unload_future.Get());
  EXPECT_FALSE(controller->IsBuiltInExtensionLoaded(BuiltInExtension::kGPC));

  // Test unloading when the extension is not loaded returns false.
  base::test::TestFuture<bool> unload_not_loaded_future;
  controller->UnloadBuiltInExtension(BuiltInExtension::kGPC,
                                     unload_not_loaded_future.GetCallback());
  EXPECT_FALSE(unload_not_loaded_future.Get());
}

// Tests that GPCWebExtension singleton returns the same instance and can load
// and cache the WKWebExtension.
TEST_F(ExtensionControllerImplTest, TestGPCWebExtensionSingleton)
API_AVAILABLE(ios(18.4)) {
  GPCWebExtension* extension_mgr = GPCWebExtension::GetInstance();
  ASSERT_TRUE(extension_mgr);
  EXPECT_EQ(extension_mgr, GPCWebExtension::GetInstance());

  base::test::TestFuture<WKWebExtension*> future;
  extension_mgr->CreateWKWebExtension(future.GetCallback());
  WKWebExtension* extension = future.Get();
  EXPECT_TRUE(extension != nil);
  EXPECT_EQ(extension, extension_mgr->GetWKWebExtension());
}

}  // namespace web
