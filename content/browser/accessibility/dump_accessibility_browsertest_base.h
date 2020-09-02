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
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/test/content_browser_test.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class BrowserAccessibility;

// Base class for an accessibility browsertest that takes an HTML file as
// input, loads it into a tab, dumps some accessibility data in text format,
// then compares that text to an expectation file in the same directory.
//
// The system was inspired by WebKit/Blink LayoutTests, but customized for
// testing accessibility in Chromium.
//
// See content/test/data/accessibility/readme.md for an overview.
class DumpAccessibilityTestBase : public ContentBrowserTest,
                                  public ::testing::WithParamInterface<size_t> {
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
  virtual void AddDefaultFilters(
      std::vector<AccessibilityTreeFormatter::PropertyFilter>*
          property_filters) = 0;

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

  // Parse the test html file and parse special directives, usually
  // beginning with an '@' and inside an HTML comment, that control how the
  // test is run and how the results are interpreted.
  //
  // When the accessibility tree is dumped as text, each node and each attribute
  // is run through filters before being appended to the string. An "allow"
  // filter specifies attribute strings that should be dumped, and a "deny"
  // filter specifies strings or nodes that should be suppressed. As an example,
  // @MAC-ALLOW:AXSubrole=* means that the AXSubrole attribute should be
  // printed, while @MAC-ALLOW:AXSubrole=AXList* means that any subrole
  // beginning with the text "AXList" should be printed.
  //
  // The @WAIT-FOR:text directive allows the test to specify that the document
  // may dynamically change after initial load, and the test is to wait
  // until the given string (e.g., "text") appears in the resulting dump.
  // A test can make some changes to the document, then append a magic string
  // indicating that the test is done, and this framework will wait for that
  // string to appear before comparing the results. There can be multiple
  // @WAIT-FOR: directives.
  void ParseHtmlForExtraDirectives(const std::string& test_html,
                                   std::vector<std::string>* no_load_expected,
                                   std::vector<std::string>* wait_for,
                                   std::vector<std::string>* execute,
                                   std::vector<std::string>* run_until,
                                   std::vector<std::string>* default_action_on);

  void RunTestForPlatform(const base::FilePath file_path, const char* file_dir);

  // Retrieve the accessibility node that matches the accessibility name. There
  // is an optional search_root parameter that defaults to the document root if
  // not provided.
  BrowserAccessibility* FindNode(const std::string& name,
                                 BrowserAccessibility* search_root = nullptr);

  // Retrieve the browser accessibility manager object for the current web
  // contents.
  BrowserAccessibilityManager* GetManager();

  // The default property filters plus the property filters loaded from the test
  // file.
  std::vector<AccessibilityTreeFormatter::PropertyFilter> property_filters_;

  // The node filters loaded from the test file.
  std::vector<AccessibilityTreeFormatter::NodeFilter> node_filters_;

  // The current tree-formatter and event-recorder factories.
  AccessibilityTreeFormatter::FormatterFactory formatter_factory_;
  AccessibilityEventRecorder::EventRecorderFactory event_recorder_factory_;

  // The current AccessibilityTreeFormatter.
  std::unique_ptr<AccessibilityTreeFormatter> formatter_;

  // Whether we should enable accessibility after navigating to the page,
  // otherwise we enable it first.
  bool enable_accessibility_after_navigating_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          const std::string& name);

  void WaitForAXTreeLoaded(WebContentsImpl* web_contents,
                           const std::vector<std::string>& no_load_expected,
                           const std::vector<std::string>& wait_for);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
