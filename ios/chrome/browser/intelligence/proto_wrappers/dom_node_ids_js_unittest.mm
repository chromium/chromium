// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestDivId[] = "test-div";

class DomNodeIdsJavascriptTest : public web::JavascriptTest {
 protected:
  DomNodeIdsJavascriptTest() {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
  }

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddUserScript(@"dom_node_ids_test");

    NSString* html = base::SysUTF8ToNSString(base::StringPrintf(R"(
        <html>
          <body>
            <div id='%s'>Test Div</div>
          </body>
        </html>
    )",
                                                                kTestDivId));
    ASSERT_TRUE(LoadHtml(html));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that calling getOrCreateNodeId multiple times for the same node
// returns the same ID.
TEST_F(DomNodeIdsJavascriptTest, GetOrCreateNodeId_Idempotent) {
  id create_result_1 = web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          __gCrWeb.getRegisteredApi('dom_node_ids_test')
                  .getFunction('getOrCreateNodeId')
                    (document.getElementById('%s'));
      )",
                                                             kTestDivId)));

  ASSERT_TRUE(create_result_1);
  int node_id_1 = [create_result_1 intValue];

  id create_result_2 = web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          __gCrWeb.getRegisteredApi('dom_node_ids_test')
                  .getFunction('getOrCreateNodeId')
                    (document.getElementById('%s'));
      )",
                                                             kTestDivId)));

  ASSERT_TRUE(create_result_2);
  int node_id_2 = [create_result_2 intValue];

  EXPECT_EQ(node_id_1, node_id_2);
}

// Tests that getNodeById successfully retrieves a node when given a valid ID.
TEST_F(DomNodeIdsJavascriptTest, GetNodeById) {
  id create_result = web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          __gCrWeb.getRegisteredApi('dom_node_ids_test')
                  .getFunction('getOrCreateNodeId')
                    (document.getElementById('%s'));
      )",
                                                             kTestDivId)));

  ASSERT_TRUE(create_result);
  int node_id = [create_result intValue];

  NSString* get_node_script =
      base::SysUTF8ToNSString(base::StringPrintf(R"(
      __gCrWeb.getRegisteredApi('dom_node_ids_test')
              .getFunction('getNodeById')
                (%d, window) === document.getElementById('%s');
  )",
                                                 node_id, kTestDivId));
  id get_result = web::test::ExecuteJavaScript(web_view(), get_node_script);

  EXPECT_TRUE([get_result boolValue]);
}

// Tests that getNodeById returns null when given an ID that was never created.
TEST_F(DomNodeIdsJavascriptTest, GetNodeById_InvalidId) {
  NSString* get_node_script = base::SysUTF8ToNSString(R"(
      __gCrWeb.getRegisteredApi('dom_node_ids_test')
              .getFunction('getNodeById')(123, window) === null;
  )");
  id get_result = web::test::ExecuteJavaScript(web_view(), get_node_script);

  EXPECT_TRUE([get_result boolValue]);
}

// Tests that getNodeById returns null and cleans up stale references when the
// node has been deleted and garbage collected.
TEST_F(DomNodeIdsJavascriptTest, GetNodeById_ElementDeleted) {
  id create_result = web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          __gCrWeb.getRegisteredApi('dom_node_ids_test')
                  .getFunction('getOrCreateNodeId')
                    (document.getElementById('%s'));
      )",
                                                             kTestDivId)));

  ASSERT_TRUE(create_result);
  int node_id = [create_result intValue];

  // Delete the element
  (void)web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          document.getElementById('%s').remove();
      )",
                                                             kTestDivId)));

  // Force a garbage collected state by overriding the WeakRef for testing.
  (void)web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          (() => {
            const manager = window[Symbol.for('__gCrWebDomNodeIdManager')];
            manager.domNodeReverseMap.set(%d, {deref: () => undefined});
          })();
      )",
                                                             node_id)));

  // Try to retrieve the node by ID, which should be null (and map cleaned up)
  NSString* get_node_script =
      base::SysUTF8ToNSString(base::StringPrintf(R"(
      (() => {
        const node = __gCrWeb.getRegisteredApi('dom_node_ids_test')
                             .getFunction('getNodeById')(%d, window);
        const manager = window[Symbol.for('__gCrWebDomNodeIdManager')];
        const inMap = manager.domNodeReverseMap.has(%d);
        return node === null && !inMap; // Both should be true
      })();
  )",
                                                 node_id, node_id));

  id get_result = web::test::ExecuteJavaScript(web_view(), get_node_script);

  EXPECT_TRUE([get_result boolValue]);
}

}  // namespace
