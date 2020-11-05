// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_
#define CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/tree_formatter.h"

namespace base {
class CommandLine;
}

namespace content {

// A helper class used to instantiate platform-specific accessibility
// tree formatters.
class CONTENT_EXPORT AccessibilityTreeFormatter : public ui::AXTreeFormatter {
 public:
  // Create the appropriate native subclass of AccessibilityTreeFormatter.
  static std::unique_ptr<ui::AXTreeFormatter> Create();

  // Get a set of factory methods to create tree-formatters, one for each test
  // pass; see |DumpAccessibilityTestBase|.
  using FormatterFactory = std::unique_ptr<ui::AXTreeFormatter> (*)();
  using CommandLineHelper = void (*)(base::CommandLine* command_line);
  struct TestPass {
    const char* name;
    FormatterFactory create_formatter;
    CommandLineHelper set_up_command_line;
  };
  static std::vector<TestPass> GetTestPasses();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_
