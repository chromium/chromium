// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "content/public/browser/ax_inspect_factory.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace ui {
struct AXNodeFilter;
struct AXPropertyFilter;
}  // namespace ui

namespace content {

// A helper class for writing accessibility tree dump tests.
class DumpAccessibilityTestHelper {
 public:
  explicit DumpAccessibilityTestHelper(AXInspectFactory::Type type);
  explicit DumpAccessibilityTestHelper(const char* expectation_type);
  ~DumpAccessibilityTestHelper() = default;

  // Returns a path to an expectation file for the current platform. If no
  // suitable expectation file can be found, logs an error message and returns
  // an empty path.
  base::FilePath GetExpectationFilePath(const base::FilePath& test_file_path);

  // Sets up a command line for the test.
  void SetUpCommandLine(base::CommandLine*) const;

  // Describes the test execution flow, which is determined by a sequence of
  // testing directives (instructions).
  struct Scenario {
    explicit Scenario(
        const std::vector<ui::AXPropertyFilter>& default_filters = {});
    Scenario(Scenario&&);
    ~Scenario();

    Scenario& operator=(Scenario&&);

    // A list of URLs of resources that are never expected to load. For example,
    // a broken image url, which otherwise would make a test failing.
    std::vector<std::string> no_load_expected;

    // A list of strings must be present in the formatted tree before the test
    // starts
    std::vector<std::string> wait_for;

    // A list of string indicating an element the default accessible action
    // should be performed at before the test starts.
    std::vector<std::string> default_action_on;

    // A list of JavaScripts functions to be executed consequently. Function
    // may return a value, which has to be present in a formatter tree before
    // the next function evaluated.
    std::vector<std::string> execute;

    // A list of strings indicating that event recording should be terminated
    // when one of them is present in a formatted tree.
    std::vector<std::string> run_until;

    // A list of property filters which defines generated output of a formatted
    // tree.
    std::vector<ui::AXPropertyFilter> property_filters;

    // The node filters indicating subtrees that should be not included into
    // a formatted tree.
    std::vector<ui::AXNodeFilter> node_filters;
  };

  // Parses a given testing scenario. Prepends default property filters if any
  // so the test file filters will take precedence over default filters in case
  // of conflict.
  Scenario ParseScenario(
      const std::vector<std::string>& lines,
      const std::vector<ui::AXPropertyFilter>& default_filters = {});

  // Returns a platform-dependent list of inspect types used in dump tree
  // testing.
  static std::vector<AXInspectFactory::Type> TreeTestPasses();

  // Returns a platform-dependent list of inspect types used in dump events
  // testing.
  static std::vector<AXInspectFactory::Type> EventTestPasses();

  // Loads the given expectation file and returns the contents. An expectation
  // file may be empty, in which case an empty vector is returned.
  // Returns nullopt if the file contains a skip marker.
  static base::Optional<std::vector<std::string>> LoadExpectationFile(
      const base::FilePath& expected_file);

  // Compares the given actual dump against the given expectation and generates
  // a new expectation file if switches::kGenerateAccessibilityTestExpectations
  // has been set. Returns true if the result matches the expectation.
  static bool ValidateAgainstExpectation(
      const base::FilePath& test_file_path,
      const base::FilePath& expected_file,
      const std::vector<std::string>& actual_lines,
      const std::vector<std::string>& expected_lines);

 private:
  enum Directive {
    // No directive.
    kNone,

    // Instructs to not wait for document load for url defined by the
    // directive.
    kNoLoadExpected,

    // Delays a test unitl a string defined by the directive is present
    // in the dump.
    kWaitFor,

    // Delays a test until a string returned by a script defined by the
    // directive is present in the dump.
    kExecuteAndWaitFor,

    // Indicates event recording should continue at least until a specific
    // event has been received.
    kRunUntil,

    // Invokes default action on an accessible object defined by the
    // directive.
    kDefaultActionOn,

    // Property filter directives, see AXPropertyFilter.
    kPropertyFilterAllow,
    kPropertyFilterAllowEmpty,
    kPropertyFilterDeny,

    // Scripting instruction.
    kScript,

    // Node filter directives, see AXNodeFilter.
    kNodeFilter,
  };

  // Parses directives from the given line.
  Directive ParseDirective(const std::string& directive) const;

  // Adds a given directive into a scenario.
  void ProcessDirective(Directive directive,
                        const std::string& value,
                        Scenario* scenario) const;

  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  base::FilePath::StringType GetExpectedFileSuffix() const;

  // Some Platforms expect different outputs depending on the version.
  // Most test outputs are identical but this allows a version specific
  // expected file to be used.
  base::FilePath::StringType GetVersionSpecificExpectedFileSuffix() const;

  FRIEND_TEST_ALL_PREFIXES(DumpAccessibilityTestHelperTest, TestDiffLines);

  // Utility helper that does a comment-aware equality check.
  // Returns array of lines from expected file which are different.
  static std::vector<int> DiffLines(
      const std::vector<std::string>& expected_lines,
      const std::vector<std::string>& actual_lines);

  std::string expectation_type_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
