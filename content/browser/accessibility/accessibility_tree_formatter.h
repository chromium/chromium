// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace {
const char kChildrenDictAttr[] = "children";
}

namespace content {

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class CONTENT_EXPORT AccessibilityTreeFormatter {
 public:
  explicit AccessibilityTreeFormatter();
  virtual ~AccessibilityTreeFormatter();

  // A single filter specification. See GetAllowString() and GetDenyString()
  // for more information.
  struct Filter {
    enum Type {
      ALLOW,
      ALLOW_EMPTY,
      DENY
    };
    base::string16 match_str;
    Type type;

    Filter(base::string16 match_str, Type type)
        : match_str(match_str), type(type) {}
  };

  // Create the appropriate native subclass of AccessibilityTreeFormatter.
  static std::unique_ptr<AccessibilityTreeFormatter> Create();

  static bool MatchesFilters(
      const std::vector<Filter>& filters,
      const base::string16& text,
      bool default_result);

  // Populates the given DictionaryValue with the accessibility tree.
  // The dictionary contains a key/value pair for each attribute of the node,
  // plus a "children" attribute containing a list of all child nodes.
  // {
  //   "AXName": "node",  /* actual attributes will vary by platform */
  //   "position": {  /* some attributes may be dictionaries */
  //     "x": 0,
  //     "y": 0
  //   },
  //   /* ... more attributes of |node| */
  //   "children": [ {  /* list of children created recursively */
  //     "AXName": "child node 1",
  //     /* ... more attributes */
  //     "children": [ ]
  //   }, {
  //     "AXName": "child name 2",
  //     /* ... more attributes */
  //     "children": [ ]
  //   } ]
  // }
  // Build an accessibility tree for the current Chrome app.
  virtual std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* root) = 0;

  // Build an accessibility tree for any process with a window.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForProcess(base::ProcessId pid) = 0;

  // Build an accessibility tree for any window.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForWindow(gfx::AcceleratedWidget widget) = 0;

  // Build an accessibility tree for an application with a name matching the
  // given pattern.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForPattern(const base::StringPiece& pattern) = 0;

  // Returns a filtered accesibility tree using the current filters.
  std::unique_ptr<base::DictionaryValue> FilterAccessibilityTree(
      const base::DictionaryValue& dict);

  // Dumps a BrowserAccessibility tree into a string.
  void FormatAccessibilityTree(
      BrowserAccessibility* root, base::string16* contents);
  void FormatAccessibilityTree(const base::DictionaryValue& tree_node,
                               base::string16* contents);

  static base::string16 DumpAccessibilityTreeFromManager(
      BrowserAccessibilityManager* ax_mgr,
      bool internal);

  // Set regular expression filters that apply to each component of every
  // line before it's output.
  void SetFilters(const std::vector<Filter>& filters);

  // If true, the internal accessibility id of each node will be included
  // in its output.
  void set_show_ids(bool show_ids) { show_ids_ = show_ids; }

  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  virtual const base::FilePath::StringType GetExpectedFileSuffix() = 0;

  // A string that indicates a given line in a file is an allow-empty,
  // allow or deny filter. Overridden by each platform subclass. Example:
  // Mac values:
  //   GetAllowEmptyString() -> "@MAC-ALLOW-EMPTY:"
  //   GetAllowString() -> "@MAC-ALLOW:"
  //   GetDenyString() -> "@MAC-DENY:"
  // Example html:
  // <!--
  // @MAC-ALLOW-EMPTY:description*
  // @MAC-ALLOW:roleDescription*
  // @MAC-DENY:subrole*
  // -->
  // <p>Text</p>
  virtual const std::string GetAllowEmptyString() = 0;
  virtual const std::string GetAllowString() = 0;
  virtual const std::string GetDenyString() = 0;

 protected:
  //
  // Overridden by platform subclasses.
  //

  // Process accessibility tree with filters for output.
  // Given a dictionary that contains a platform-specific dictionary
  // representing an accessibility tree, and utilizing filters_:
  // - Returns a filtered text view as one large string.
  // - Provides a filtered version of the dictionary in an out param,
  //   (only if the out param is provided).
  virtual base::string16 ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) = 0;

  //
  // Utility functions to be used by each platform.
  //

  base::string16 FormatCoordinates(const char* name,
                                   const char* x_name,
                                   const char* y_name,
                                   const base::DictionaryValue& value);

  // Writes the given attribute string out to |line| if it matches the filters.
  // Returns false if the attribute was filtered out.
  bool WriteAttribute(bool include_by_default,
                      const base::string16& attr,
                      base::string16* line);
  bool WriteAttribute(bool include_by_default,
                      const std::string& attr,
                      base::string16* line);

  bool show_ids() { return show_ids_; }

 private:
  void RecursiveFormatAccessibilityTree(const BrowserAccessibility& node,
                                        base::string16* contents,
                                        int indent);
  void RecursiveFormatAccessibilityTree(const base::DictionaryValue& tree_node,
                                        base::string16* contents,
                                        int depth = 0);

  bool MatchesFilters(const base::string16& text, bool default_result) const;

  // Filters used when formatting the accessibility tree as text.
  std::vector<Filter> filters_;

  // Whether or not node ids should be included in the dump.
  bool show_ids_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityTreeFormatter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_
