// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/dump_accessibility_test_helper.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;
class DumpAccessibilityTestHelper;

// Base class for an accessibility browsertest that takes an HTML file as
// input, loads it into a tab, dumps some accessibility data in text format,
// then compares that text to an expectation file in the same directory.
//
// The system was inspired by WebKit/Blink LayoutTests, but customized for
// testing accessibility in Chromium.
//
// See content/test/data/accessibility/readme.md for an overview.
class DumpAccessibilityTestBase
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<AXInspectFactory::Type> {
 public:
  DumpAccessibilityTestBase();
  ~DumpAccessibilityTestBase() override;

  // Given a path to an HTML file relative to the test directory,
  // loads the HTML, loads the accessibility tree, calls Dump(), then
  // compares the output to the expected result and has the test succeed
  // or fail based on the diff.
  void RunTest(const base::FilePath file_path, const char* file_dir);

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void SetUp() override;

  //
  // For subclasses to override:
  //

  // This is called by RunTest after the document has finished loading,
  // including the load complete accessibility event. The subclass should
  // dump whatever that specific test wants to dump, returning the result
  // as a sequence of strings.
  virtual std::vector<std::string> Dump(
      std::vector<std::string>& run_until) = 0;

  // Add the default filters that are applied to all tests.
  virtual std::vector<ui::AXPropertyFilter> DefaultFilters() const = 0;

  // This gets called if the diff didn't match; the test can print
  // additional useful info.
  virtual void OnDiffFailed() {}

  // Choose which feature flags to enable or disable.
  virtual void ChooseFeatures(std::vector<base::Feature>* enabled_features,
                              std::vector<base::Feature>* disabled_features);

  //
  // Helpers
  //

  // Dump the whole accessibility tree, without applying any filters,
  // and return it as a string.
  std::string DumpUnfilteredAccessibilityTreeAsString();

  void RunTestForPlatform(const base::FilePath file_path, const char* file_dir);

  // Retrieve the accessibility node that matches the accessibility name. There
  // is an optional search_root parameter that defaults to the document root if
  // not provided.
  BrowserAccessibility* FindNode(const std::string& name,
                                 BrowserAccessibility* search_root = nullptr);

  // Retrieve the browser accessibility manager object for the current web
  // contents.
  BrowserAccessibilityManager* GetManager();

  std::unique_ptr<ui::AXTreeFormatter> CreateFormatter() const;

  // Test scenario loaded from the test file.
  DumpAccessibilityTestHelper::Scenario scenario_;

  // Whether we should enable accessibility after navigating to the page,
  // otherwise we enable it first.
  bool enable_accessibility_after_navigating_;

  // Whether we should enable extra mac nodes when running a test.
  bool disable_extra_mac_nodes_for_testing_ = false;

  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  DumpAccessibilityTestHelper test_helper_;

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          const std::string& name);

  std::vector<std::string> CollectAllFrameUrls(
      WebContentsImpl* web_contents,
      const std::vector<std::string>& skip_urls);

  void WaitForAXTreeLoaded(WebContentsImpl* web_contents);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
