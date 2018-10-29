// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

const char kIndentSymbol = '+';
const int kIndentSymbolCount = 2;
const char kSkipString[] = "@NO_DUMP";
const char kSkipChildren[] = "@NO_CHILDREN_DUMP";

}  // namespace

AccessibilityTreeFormatter::AccessibilityTreeFormatter()
    : show_ids_(false) {
}

AccessibilityTreeFormatter::~AccessibilityTreeFormatter() {
}

void AccessibilityTreeFormatter::FormatAccessibilityTree(
    BrowserAccessibility* root, base::string16* contents) {
  std::unique_ptr<base::DictionaryValue> dict = BuildAccessibilityTree(root);
  RecursiveFormatAccessibilityTree(*(dict.get()), contents);
}

void AccessibilityTreeFormatter::FormatAccessibilityTree(
    const base::DictionaryValue& dict,
    base::string16* contents) {
  RecursiveFormatAccessibilityTree(dict, contents);
}

base::string16 AccessibilityTreeFormatter::DumpAccessibilityTreeFromManager(
    BrowserAccessibilityManager* ax_mgr,
    bool internal) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter;
  if (internal)
    formatter = std::make_unique<AccessibilityTreeFormatterBlink>();
  else
    formatter = Create();
  base::string16 accessibility_contents_utf16;
  std::vector<Filter> filters;
  filters.push_back(Filter(base::ASCIIToUTF16("*"), Filter::ALLOW));
  formatter->SetFilters(filters);
  formatter->FormatAccessibilityTree(ax_mgr->GetRoot(),
                                     &accessibility_contents_utf16);
  return accessibility_contents_utf16;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatter::FilterAccessibilityTree(
    const base::DictionaryValue& dict) {
  auto filtered_dict = std::make_unique<base::DictionaryValue>();
  ProcessTreeForOutput(dict, filtered_dict.get());
  const base::ListValue* children;
  if (dict.GetList(kChildrenDictAttr, &children) && !children->empty()) {
    const base::DictionaryValue* child_dict;
    auto filtered_children = std::make_unique<base::ListValue>();
    for (size_t i = 0; i < children->GetSize(); i++) {
      children->GetDictionary(i, &child_dict);
      auto filtered_child = FilterAccessibilityTree(*child_dict);
      filtered_children->Append(std::move(filtered_child));
    }
    filtered_dict->Set(kChildrenDictAttr, std::move(filtered_children));
  }
  return filtered_dict;
}

void AccessibilityTreeFormatter::RecursiveFormatAccessibilityTree(
    const base::DictionaryValue& dict, base::string16* contents, int depth) {
  base::string16 indent = base::string16(depth * kIndentSymbolCount,
                                         kIndentSymbol);
  base::string16 line = indent + ProcessTreeForOutput(dict);
  if (line.find(base::ASCIIToUTF16(kSkipString)) != base::string16::npos)
    return;

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line,
                     base::ASCIIToUTF16("\n"),
                     base::ASCIIToUTF16("<newline>"),
                     &line);

  *contents += line + base::ASCIIToUTF16("\n");
  if (line.find(base::ASCIIToUTF16(kSkipChildren)) != base::string16::npos)
    return;

  const base::ListValue* children;
  if (!dict.GetList(kChildrenDictAttr, &children))
    return;
  const base::DictionaryValue* child_dict;
  for (size_t i = 0; i < children->GetSize(); i++) {
    children->GetDictionary(i, &child_dict);
    RecursiveFormatAccessibilityTree(*child_dict, contents, depth + 1);
  }
}

void AccessibilityTreeFormatter::SetFilters(
    const std::vector<Filter>& filters) {
  filters_ = filters;
}

// static
bool AccessibilityTreeFormatter::MatchesFilters(
    const std::vector<Filter>& filters,
    const base::string16& text,
    bool default_result) {
  bool allow = default_result;
  for (const auto& filter : filters) {
    if (base::MatchPattern(text, filter.match_str)) {
      switch (filter.type) {
        case Filter::ALLOW_EMPTY:
          allow = true;
          break;
        case Filter::ALLOW:
          allow = (!base::MatchPattern(text, base::UTF8ToUTF16("*=''")));
          break;
        case Filter::DENY:
          allow = false;
          break;
      }
    }
  }
  return allow;
}

bool AccessibilityTreeFormatter::MatchesFilters(
    const base::string16& text, bool default_result) const {
  return MatchesFilters(filters_, text, default_result);
}

base::string16 AccessibilityTreeFormatter::FormatCoordinates(
    const char* name, const char* x_name, const char* y_name,
    const base::DictionaryValue& value) {
  int x, y;
  value.GetInteger(x_name, &x);
  value.GetInteger(y_name, &y);
  std::string xy_str(base::StringPrintf("%s=(%d, %d)", name, x, y));

  return base::UTF8ToUTF16(xy_str);
}

bool AccessibilityTreeFormatter::WriteAttribute(bool include_by_default,
                                                const std::string& attr,
                                                base::string16* line) {
  return WriteAttribute(include_by_default, base::UTF8ToUTF16(attr), line);
}

bool AccessibilityTreeFormatter::WriteAttribute(bool include_by_default,
                                                const base::string16& attr,
                                                base::string16* line) {
  if (attr.empty())
    return false;
  if (!MatchesFilters(attr, include_by_default))
    return false;
  if (!line->empty())
    *line += base::ASCIIToUTF16(" ");
  *line += attr;
  return true;
}

}  // namespace content
