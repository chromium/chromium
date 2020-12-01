// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
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
#include "ui/accessibility/platform/inspect/property_node.h"

using ui::AXPropertyNode;

namespace content {

namespace {

const char kIndentSymbol = '+';
const int kIndentSymbolCount = 2;
const char kSkipString[] = "@NO_DUMP";
const char kSkipChildren[] = "@NO_CHILDREN_DUMP";

}  // namespace

AccessibilityTreeFormatterBase::AccessibilityTreeFormatterBase() = default;

AccessibilityTreeFormatterBase::~AccessibilityTreeFormatterBase() = default;

std::string AccessibilityTreeFormatterBase::Format(
    ui::AXPlatformNodeDelegate* root) const {
  auto* node_internal = BrowserAccessibility::FromAXPlatformNodeDelegate(root);
  DCHECK(node_internal);
  return FormatTree(BuildTree(node_internal));
}

std::string AccessibilityTreeFormatterBase::FormatTree(
    const base::Value& dict) const {
  std::string contents;
  RecursiveFormatTree(dict, &contents);
  return contents;
}

void AccessibilityTreeFormatterBase::RecursiveFormatTree(
    const base::Value& dict,
    std::string* contents,
    int depth) const {
  // Check dictionary against node filters, may require us to skip this node
  // and its children.
  if (MatchesNodeFilters(dict))
    return;

  std::string indent = std::string(depth * kIndentSymbolCount, kIndentSymbol);
  std::string line =
      indent + ProcessTreeForOutput(base::Value::AsDictionaryValue(dict));
  if (line.find(kSkipString) != std::string::npos)
    return;

  // Normalize any Windows-style line endings by removing \r.
  base::RemoveChars(line, "\r", &line);

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line, "\n", "<newline>", &line);

  *contents += line + "\n";
  if (line.find(kSkipChildren) != std::string::npos)
    return;

  const base::Value* children = dict.FindListPath(kChildrenDictAttr);
  if (children && !children->GetList().empty()) {
    for (const auto& child_dict : children->GetList()) {
      RecursiveFormatTree(child_dict, contents, depth + 1);
    }
  }
}

void AccessibilityTreeFormatterBase::SetPropertyFilters(
    const std::vector<AXPropertyFilter>& property_filters) {
  property_filters_ = property_filters;
}

void AccessibilityTreeFormatterBase::SetNodeFilters(
    const std::vector<AXNodeFilter>& node_filters) {
  node_filters_ = node_filters;
}

void AccessibilityTreeFormatterBase::set_show_ids(bool show_ids) {
  show_ids_ = show_ids;
}

std::vector<AXPropertyNode>
AccessibilityTreeFormatterBase::PropertyFilterNodesFor(
    const std::string& line_index) const {
  std::vector<AXPropertyNode> list;
  for (const auto& filter : property_filters_) {
    AXPropertyNode property_node = AXPropertyNode::From(filter);

    // Filter out if doesn't match line index (if specified).
    if (!property_node.line_indexes.empty() &&
        std::find(property_node.line_indexes.begin(),
                  property_node.line_indexes.end(),
                  line_index) == property_node.line_indexes.end()) {
      continue;
    }

    switch (filter.type) {
      case AXPropertyFilter::ALLOW_EMPTY:
      case AXPropertyFilter::ALLOW:
        list.push_back(std::move(property_node));
        break;
      case AXPropertyFilter::DENY:
        break;
      default:
        break;
    }
  }
  return list;
}

bool AccessibilityTreeFormatterBase::HasMatchAllPropertyFilter() const {
  for (const auto& filter : property_filters_) {
    if (filter.type == AXPropertyFilter::ALLOW && filter.match_str == "*") {
      return true;
    }
  }
  return false;
}

bool AccessibilityTreeFormatterBase::MatchesPropertyFilters(
    const std::string& text,
    bool default_result) const {
  return ui::AXTreeFormatter::MatchesPropertyFilters(property_filters_, text,
                                                     default_result);
}

bool AccessibilityTreeFormatterBase::MatchesNodeFilters(
    const base::Value& dict) const {
  return ui::AXTreeFormatter::MatchesNodeFilters(node_filters_, dict);
}

std::string AccessibilityTreeFormatterBase::FormatCoordinates(
    const base::Value& dict,
    const std::string& name,
    const std::string& x_name,
    const std::string& y_name) const {
  int x = dict.FindIntPath(x_name).value_or(0);
  int y = dict.FindIntPath(y_name).value_or(0);
  return base::StringPrintf("%s=(%d, %d)", name.c_str(), x, y);
}

std::string AccessibilityTreeFormatterBase::FormatRectangle(
    const base::Value& dict,
    const std::string& name,
    const std::string& left_name,
    const std::string& top_name,
    const std::string& width_name,
    const std::string& height_name) const {
  int left = dict.FindIntPath(left_name).value_or(0);
  int top = dict.FindIntPath(top_name).value_or(0);
  int width = dict.FindIntPath(width_name).value_or(0);
  int height = dict.FindIntPath(height_name).value_or(0);
  return base::StringPrintf("%s=(%d, %d, %d, %d)", name.c_str(), left, top,
                            width, height);
}

bool AccessibilityTreeFormatterBase::WriteAttribute(bool include_by_default,
                                                    const std::string& attr,
                                                    std::string* line) const {
  if (attr.empty())
    return false;
  if (!MatchesPropertyFilters(attr, include_by_default))
    return false;
  if (!line->empty())
    *line += " ";
  *line += attr;
  return true;
}

void AccessibilityTreeFormatterBase::AddPropertyFilter(
    std::vector<AXPropertyFilter>* property_filters,
    std::string filter,
    AXPropertyFilter::Type type) {
  property_filters->push_back(AXPropertyFilter(filter, type));
}

void AccessibilityTreeFormatterBase::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {}

}  // namespace content
