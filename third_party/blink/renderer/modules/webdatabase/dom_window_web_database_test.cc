// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/dom_window_web_database.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

void OpenWebDatabaseInIFrame(const char* outer_origin,
                             const char* outer_file,
                             const char* inner_origin,
                             const char* inner_file,
                             ExceptionState& exception_state) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(outer_origin), test::CoreTestDataPath(),
      WebString::FromUTF8(outer_file));
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(inner_origin), test::CoreTestDataPath(),
      WebString::FromUTF8(inner_file));
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base::StrCat({outer_origin, outer_file}));
  LocalDOMWindow* local_dom_window =
      To<LocalDOMWindow>(web_view_helper.GetWebView()
                             ->GetPage()
                             ->MainFrame()
                             ->Tree()
                             .FirstChild()
                             ->DomWindow());
  Database* result = DOMWindowWebDatabase::openDatabase(
      *local_dom_window, "", "", "", 0, exception_state);
  EXPECT_EQ(result, nullptr);
  url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
}

TEST(DOMWindowWebDatabaseTest, FirstPartyContextWebSQLIFrame) {
  V8TestingScope scope;
  OpenWebDatabaseInIFrame("http://example.test:0/",
                          "first_party/nested-originA.html",
                          "http://example.test:0/", "first_party/empty.html",
                          scope.GetExceptionState());
  // Insufficient state exists to actually open a database, but this error
  // means it was tried.
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            static_cast<int>(DOMExceptionCode::kInvalidStateError));
}

TEST(DOMWindowWebDatabaseTest, ThirdPartyContextWebSQLIFrame) {
  V8TestingScope scope;
  OpenWebDatabaseInIFrame("http://not-example.test:0/",
                          "first_party/nested-originA.html",
                          "http://example.test:0/", "first_party/empty.html",
                          scope.GetExceptionState());
  // This error means the database opening was rejected.
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            static_cast<int>(DOMExceptionCode::kSecurityError));
}

TEST(DOMWindowWebDatabaseTest,
     ThirdPartyContextWebSQLIFrameAndThrowingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kWebSQLInThirdPartyContextThrowsWhenDisabled);
  V8TestingScope scope;
  OpenWebDatabaseInIFrame("http://not-example.test:0/",
                          "first_party/nested-originA.html",
                          "http://example.test:0/", "first_party/empty.html",
                          scope.GetExceptionState());
  // This case is identical to `ThirdPartyContextWebSQLIFrame`, except that
  // no exception should be thrown whena access is denied.
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(DOMWindowWebDatabaseTest, ThirdPartyContextWebSQLIFrameWithFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebSQLInThirdPartyContextEnabled);
  V8TestingScope scope;
  OpenWebDatabaseInIFrame("http://not-example.test:0/",
                          "first_party/nested-originA.html",
                          "http://example.test:0/", "first_party/empty.html",
                          scope.GetExceptionState());
  // Insufficient state exists to actually open a database, but this error
  // means it was tried.
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            static_cast<int>(DOMExceptionCode::kInvalidStateError));
}

TEST(DOMWindowWebDatabaseTest, ThirdPartyContextWebSQLIFrameWithSwitch) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      blink::switches::kWebSQLInThirdPartyContextEnabled);
  V8TestingScope scope;
  OpenWebDatabaseInIFrame("http://not-example.test:0/",
                          "first_party/nested-originA.html",
                          "http://example.test:0/", "first_party/empty.html",
                          scope.GetExceptionState());
  // Insufficient state exists to actually open a database, but this error
  // means it was tried.
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            static_cast<int>(DOMExceptionCode::kInvalidStateError));
}

TEST(DOMWindowWebDatabaseTest,
     ThirdPartyContextWebSQLIFrameWithFeatureAndSwitch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kWebSQLInThirdPartyContextEnabled);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      blink::switches::kWebSQLInThirdPartyContextEnabled);
  V8TestingScope scope;
  OpenWebDatabaseInIFrame("http://not-example.test:0/",
                          "first_party/nested-originA.html",
                          "http://example.test:0/", "first_party/empty.html",
                          scope.GetExceptionState());
  // Insufficient state exists to actually open a database, but this error
  // means it was tried.
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            static_cast<int>(DOMExceptionCode::kInvalidStateError));
}

}  // namespace blink
