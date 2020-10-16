// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_
#define CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/inspect.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class CommandLine;
}

namespace ui {
class AXPlatformNodeDelegate;
}

namespace content {

class AccessibilityTestExpectationsLocator {
 public:
  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  virtual base::FilePath::StringType GetExpectedFileSuffix() = 0;

  // Some Platforms expect different outputs depending on the version.
  // Most test outputs are identical but this allows a version specific
  // expected file to be used.
  virtual base::FilePath::StringType GetVersionSpecificExpectedFileSuffix() = 0;

 protected:
  virtual ~AccessibilityTestExpectationsLocator() = default;
};

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class CONTENT_EXPORT AccessibilityTreeFormatter
    : public AccessibilityTestExpectationsLocator {
 public:
  using AXTreeSelector = ui::AXTreeSelector;
  using AXPropertyFilter = ui::AXPropertyFilter;
  using AXNodeFilter = ui::AXNodeFilter;

  // Create the appropriate native subclass of AccessibilityTreeFormatter.
  static std::unique_ptr<AccessibilityTreeFormatter> Create();

  // Get a set of factory methods to create tree-formatters, one for each test
  // pass; see |DumpAccessibilityTestBase|.
  using FormatterFactory = std::unique_ptr<AccessibilityTreeFormatter> (*)();
  using CommandLineHelper = void (*)(base::CommandLine* command_line);
  struct TestPass {
    const char* name;
    FormatterFactory create_formatter;
    CommandLineHelper set_up_command_line;
  };
  static std::vector<TestPass> GetTestPasses();

  virtual void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) = 0;

  static bool MatchesPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters,
      const std::string& text,
      bool default_result);

  // Check if the given dictionary matches any of the supplied AXNodeFilter(s).
  static bool MatchesNodeFilters(const std::vector<AXNodeFilter>& node_filters,
                                 const base::DictionaryValue& dict);

  // Build an accessibility tree for any window.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForWindow(gfx::AcceleratedWidget widget) = 0;

  // Build an accessibility tree for an application with a name matching the
  // given pattern.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForSelector(const AXTreeSelector&) = 0;

  // Returns a filtered accesibility tree using the current property and node
  // filters.
  virtual std::unique_ptr<base::DictionaryValue> FilterAccessibilityTree(
      const base::DictionaryValue& dict) = 0;

  // Dumps a BrowserAccessibility tree into a string.
  virtual void FormatAccessibilityTree(const base::DictionaryValue& tree_node,
                                       std::string* contents) = 0;

  // Test version of FormatAccessibilityTree().
  // |root| must be non-null and must be in web content.
  virtual void FormatAccessibilityTreeForTesting(
      ui::AXPlatformNodeDelegate* root,
      std::string* contents) = 0;

  // Set regular expression filters that apply to each property of every node
  // before it's output.
  virtual void SetPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters) = 0;

  // Set regular expression filters that apply to every node before output.
  virtual void SetNodeFilters(
      const std::vector<AXNodeFilter>& node_filters) = 0;

  // If true, the internal accessibility id of each node will be included
  // in its output.
  virtual void set_show_ids(bool show_ids) = 0;

  // A string that indicates a given line in a file is an allow-empty,
  // allow or deny filter. Overridden by each platform subclass. Example:
  // Mac values:
  //   GetAllowEmptyString() -> "@MAC-ALLOW-EMPTY:"
  //   GetAllowString() -> "@MAC-ALLOW:"
  //   GetDenyString() -> "@MAC-DENY:"
  //   GetDenyNodeString() -> "@MAC-DENY-NODE:"
  // Example html:
  // <!--
  // @MAC-ALLOW-EMPTY:description*
  // @MAC-ALLOW:roleDescription*
  // @MAC-DENY:subrole*
  // @BLINK-DENY-NODE:internalRole=inlineTextBox
  // -->
  // <p>Text</p>
  virtual const std::string GetAllowEmptyString() = 0;
  virtual const std::string GetAllowString() = 0;
  virtual const std::string GetDenyString() = 0;
  virtual const std::string GetDenyNodeString() = 0;

  // A string that indicates event recording should continue at least until a
  // specific event has been received.
  // Overridden by each platform subclass.
  // Example win value:
  //   GetRunUntilEventString() -> "@WIN-RUN-UNTIL-EVENT"
  // Example html:
  // <!--
  // @WIN-RUN-UNTIL-EVENT:IA2_EVENT_TEXT_CARET_MOVED
  // -->
  virtual const std::string GetRunUntilEventString() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_
