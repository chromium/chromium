// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_

#include <string>
#include <vector>

#include "base/debug/leak_annotations.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/public/test/content_browser_test.h"

namespace content {

// Base class for an accessibility browsertest that takes an HTML file as
// input, loads it into a tab, dumps some accessibility data in text format,
// then compares that text to an expectation file in the same directory.
//
// The system was inspired by WebKit/Blink LayoutTests, but customized for
// testing accessibility in Chromium.
//
// See content/test/data/accessibility/readme.md for an overview.
class DumpAccessibilityTestBase : public ContentBrowserTest {
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
      std::vector<AccessibilityTreeFormatter::Filter>* filters) = 0;

  // This gets called if the diff didn't match; the test can print
  // additional useful info.
  virtual void OnDiffFailed() {}

  //
  // Helpers
  //

  // Dump the whole accessibility tree, without applying any filters,
  // and return it as a string.
  base::string16 DumpUnfilteredAccessibilityTreeAsString();

  // Utility helper that does a comment-aware equality check.
  // Returns array of lines from expected file which are different.
  std::vector<int> DiffLines(const std::vector<std::string>& expected_lines,
                             const std::vector<std::string>& actual_lines);

  // Parse the test html file and parse special directives, usually
  // beginning with an '@' and inside an HTML comment, that control how the
  // test is run and how the results are interpreted.
  //
  // When the accessibility tree is dumped as text, each attribute is
  // run through filters before being appended to the string. An "allow"
  // filter specifies attribute strings that should be dumped, and a "deny"
  // filter specifies strings that should be suppressed. As an example,
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
  void ParseHtmlForExtraDirectives(
      const std::string& test_html,
      std::vector<AccessibilityTreeFormatter::Filter>* filters,
      std::vector<std::string>* wait_for,
      std::vector<std::string>* run_until);

  // Create the right AccessibilityTreeFormatter subclass.
  std::unique_ptr<AccessibilityTreeFormatter>
  CreateAccessibilityTreeFormatter();

  void RunTestForPlatform(const base::FilePath file_path, const char* file_dir);

  // The default filters plus the filters loaded from the test file.
  std::vector<AccessibilityTreeFormatter::Filter> filters_;

#if defined(LEAK_SANITIZER) && !defined(OS_NACL)
  // http://crbug.com/568674
  ScopedLeakSanitizerDisabler lsan_disabler;
#endif

  // The current AccessibilityTreeFormatter.
  std::unique_ptr<AccessibilityTreeFormatter> formatter_;

  // Whether we're doing a native pass or internal/blink tree pass.
  bool is_blink_pass_;

  // Whether we should enable accessibility after navigating to the page,
  // otherwise we enable it first.
  bool enable_accessibility_after_navigating_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
