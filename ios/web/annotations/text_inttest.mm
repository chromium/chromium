// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"

namespace web {

// Test fixture for annotation js.
class AnnotationJsTest : public JavascriptTest {
 public:
  AnnotationJsTest() = default;
  AnnotationJsTest(const AnnotationJsTest&) = delete;
  AnnotationJsTest& operator=(const AnnotationJsTest&) = delete;

 protected:
  void SetUp() override {
    JavascriptTest::SetUp();

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEnableViewportIntents},
        /*disabled_features=*/{});

    AddGCrWebScript();
    AddCommonScript();
    AddMessageScript();
    AddUserScript(@"text_tests");

    // Start with empty web page.
    ASSERT_TRUE(LoadHtml(@"<html><head></head><body></body></html>"));
  }

  // Executes `test_entry` ("gcrWebStubName.jsTestSuiteEntryPoint" ) in the
  // script and outputs the result to `std::cerr`. If all js suite tests are OK
  // the unittest succeeds.
  void TestJavascriptStub(const char* test_entry) {
    NSString* entryPoint =
        [NSString stringWithFormat:@"__gCrWeb.%s();", test_entry];
    id suite_result = test::ExecuteJavaScript(web_view(), entryPoint);
    ASSERT_TRUE(suite_result);
    NSArray<NSDictionary*>* result_array =
        base::apple::ObjCCast<NSArray<NSDictionary*>>(suite_result);
    ASSERT_TRUE(result_array);
    size_t ok = 0;
    for (NSDictionary* result in result_array) {
      ASSERT_TRUE(result);
      if ([result[@"result"] isEqualToString:@"OK"]) {
        std::cerr << "[      OK  ]  "
                  << base::SysNSStringToUTF8(result[@"name"]) << std::endl;
        ok++;
      } else if ([result[@"result"] isEqualToString:@"LOG"]) {
        std::cerr << "[          ]  "
                  << base::SysNSStringToUTF8(result[@"error"]) << std::endl;
        ok++;
      } else {
        std::cerr << "[  FAILED  ]  "
                  << base::SysNSStringToUTF8(result[@"name"]) << " : "
                  << base::SysNSStringToUTF8(result[@"error"]) << std::endl;
      }
    }
    EXPECT_EQ(ok, result_array.count);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AnnotationJsTest, All) {
  TestJavascriptStub("textTests.testAll");
}

}  // namespace web
