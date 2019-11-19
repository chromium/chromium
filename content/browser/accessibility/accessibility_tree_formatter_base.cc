// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

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

AccessibilityTreeFormatter::TestPass AccessibilityTreeFormatter::GetTestPass(
    size_t index) {
  std::vector<content::AccessibilityTreeFormatter::TestPass> passes =
      content::AccessibilityTreeFormatter::GetTestPasses();
  CHECK_LT(index, passes.size());
  return passes[index];
}

// static
base::string16 AccessibilityTreeFormatterBase::DumpAccessibilityTreeFromManager(
    BrowserAccessibilityManager* ax_mgr,
    bool internal,
    std::vector<PropertyFilter> property_filters) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter;
  if (internal)
    formatter = std::make_unique<AccessibilityTreeFormatterBlink>();
  else
    formatter = Create();
  base::string16 accessibility_contents_utf16;
  formatter->SetPropertyFilters(property_filters);
  formatter->FormatAccessibilityTree(ax_mgr->GetRoot(),
                                     &accessibility_contents_utf16);
  return accessibility_contents_utf16;
}

bool AccessibilityTreeFormatter::MatchesPropertyFilters(
    const std::vector<PropertyFilter>& property_filters,
    const base::string16& text,
    bool default_result) {
  bool allow = default_result;
  for (const auto& filter : property_filters) {
    if (base::MatchPattern(text, filter.match_str)) {
      switch (filter.type) {
        case PropertyFilter::ALLOW_EMPTY:
          allow = true;
          break;
        case PropertyFilter::ALLOW:
          allow = (!base::MatchPattern(text, base::UTF8ToUTF16("*=''")));
          break;
        case PropertyFilter::DENY:
          allow = false;
          break;
      }
    }
  }
  return allow;
}

bool AccessibilityTreeFormatter::MatchesNodeFilters(
    const std::vector<NodeFilter>& node_filters,
    const base::DictionaryValue& dict) {
  for (const auto& filter : node_filters) {
    base::string16 value;
    if (!dict.GetString(filter.property, &value)) {
      continue;
    }
    if (base::MatchPattern(value, filter.pattern)) {
      return true;
    }
  }
  return false;
}

AccessibilityTreeFormatterBase::AccessibilityTreeFormatterBase()
    : show_ids_(false) {}

AccessibilityTreeFormatterBase::~AccessibilityTreeFormatterBase() {}

void AccessibilityTreeFormatterBase::FormatAccessibilityTree(
    BrowserAccessibility* root,
    base::string16* contents) {
  std::unique_ptr<base::DictionaryValue> dict = BuildAccessibilityTree(root);
  RecursiveFormatAccessibilityTree(*(dict.get()), contents);
}

void AccessibilityTreeFormatterBase::FormatAccessibilityTree(
    const base::DictionaryValue& dict,
    base::string16* contents) {
  RecursiveFormatAccessibilityTree(dict, contents);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterBase::FilterAccessibilityTree(
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

void AccessibilityTreeFormatterBase::RecursiveFormatAccessibilityTree(
    const base::DictionaryValue& dict,
    base::string16* contents,
    int depth) {
  // Check dictionary against node filters, may require us to skip this node
  // and its children.
  if (MatchesNodeFilters(dict))
    return;

  base::string16 indent =
      base::string16(depth * kIndentSymbolCount, kIndentSymbol);
  base::string16 line = indent + ProcessTreeForOutput(dict);
  if (line.find(base::ASCIIToUTF16(kSkipString)) != base::string16::npos)
    return;

  // Normalize any Windows-style line endings by removing \r.
  base::RemoveChars(line, base::ASCIIToUTF16("\r"), &line);

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line, base::ASCIIToUTF16("\n"),
                     base::ASCIIToUTF16("<newline>"), &line);

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

void AccessibilityTreeFormatterBase::SetPropertyFilters(
    const std::vector<PropertyFilter>& property_filters) {
  property_filters_ = property_filters;
}

void AccessibilityTreeFormatterBase::SetNodeFilters(
    const std::vector<NodeFilter>& node_filters) {
  node_filters_ = node_filters;
}

void AccessibilityTreeFormatterBase::set_show_ids(bool show_ids) {
  show_ids_ = show_ids;
}

base::FilePath::StringType
AccessibilityTreeFormatterBase::GetVersionSpecificExpectedFileSuffix() {
  return FILE_PATH_LITERAL("");
}

bool AccessibilityTreeFormatterBase::MatchesPropertyFilters(
    const base::string16& text,
    bool default_result) const {
  return AccessibilityTreeFormatter::MatchesPropertyFilters(
      property_filters_, text, default_result);
}

bool AccessibilityTreeFormatterBase::MatchesNodeFilters(
    const base::DictionaryValue& dict) const {
  return AccessibilityTreeFormatter::MatchesNodeFilters(node_filters_, dict);
}

base::string16 AccessibilityTreeFormatterBase::FormatCoordinates(
    const base::DictionaryValue& value,
    const std::string& name,
    const std::string& x_name,
    const std::string& y_name) {
  int x, y;
  value.GetInteger(x_name, &x);
  value.GetInteger(y_name, &y);
  std::string xy_str(base::StringPrintf("%s=(%d, %d)", name.c_str(), x, y));

  return base::UTF8ToUTF16(xy_str);
}

base::string16 AccessibilityTreeFormatterBase::FormatRectangle(
    const base::DictionaryValue& value,
    const std::string& name,
    const std::string& left_name,
    const std::string& top_name,
    const std::string& width_name,
    const std::string& height_name) {
  int left, top, width, height;
  value.GetInteger(left_name, &left);
  value.GetInteger(top_name, &top);
  value.GetInteger(width_name, &width);
  value.GetInteger(height_name, &height);
  std::string rect_str(base::StringPrintf("%s=(%d, %d, %d, %d)", name.c_str(),
                                          left, top, width, height));

  return base::UTF8ToUTF16(rect_str);
}

bool AccessibilityTreeFormatterBase::WriteAttribute(bool include_by_default,
                                                    const std::string& attr,
                                                    base::string16* line) {
  return WriteAttribute(include_by_default, base::UTF8ToUTF16(attr), line);
}

bool AccessibilityTreeFormatterBase::WriteAttribute(bool include_by_default,
                                                    const base::string16& attr,
                                                    base::string16* line) {
  if (attr.empty())
    return false;
  if (!MatchesPropertyFilters(attr, include_by_default))
    return false;
  if (!line->empty())
    *line += base::ASCIIToUTF16(" ");
  *line += attr;
  return true;
}

void AccessibilityTreeFormatterBase::AddPropertyFilter(
    std::vector<PropertyFilter>* property_filters,
    std::string filter,
    PropertyFilter::Type type) {
  property_filters->push_back(PropertyFilter(base::ASCIIToUTF16(filter), type));
}

void AccessibilityTreeFormatterBase::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {}
}  // namespace content
