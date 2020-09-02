// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

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

namespace content {

namespace {

const char kIndentSymbol = '+';
const int kIndentSymbolCount = 2;
const char kSkipString[] = "@NO_DUMP";
const char kSkipChildren[] = "@NO_CHILDREN_DUMP";

}  // namespace

//
// PropertyNode
//

// static
PropertyNode PropertyNode::FromPropertyFilter(
    const AccessibilityTreeFormatter::PropertyFilter& filter) {
  // Property invocation: property_str expected format is
  // prop_name or prop_name(arg1, ... argN).
  PropertyNode root;
  const std::string& property_str = filter.property_str;
  Parse(&root, property_str.begin(), property_str.end());

  PropertyNode* node = &root.parameters[0];

  // Expel a trailing wildcard if any.
  node->original_property =
      property_str.substr(0, property_str.find_last_of('*'));

  // Line indexes filter: filter_str expected format is
  // :line_num_1, ... :line_num_N, a comma separated list of line indexes
  // the property should be queried for. For example, ":1,:5,:7" indicates that
  // the property should called for objects placed on 1, 5 and 7 lines only.
  const std::string& filter_str = filter.filter_str;
  if (!filter_str.empty()) {
    node->line_indexes =
        base::SplitString(filter_str, std::string(1, ','),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }

  return std::move(*node);
}

PropertyNode::PropertyNode() = default;
PropertyNode::PropertyNode(PropertyNode&& o)
    : key(std::move(o.key)),
      name_or_value(std::move(o.name_or_value)),
      parameters(std::move(o.parameters)),
      original_property(std::move(o.original_property)),
      line_indexes(std::move(o.line_indexes)) {}
PropertyNode::~PropertyNode() = default;

PropertyNode& PropertyNode::operator=(PropertyNode&& o) {
  key = std::move(o.key);
  name_or_value = std::move(o.name_or_value);
  parameters = std::move(o.parameters);
  original_property = std::move(o.original_property);
  line_indexes = std::move(o.line_indexes);
  return *this;
}

PropertyNode::operator bool() const {
  return !name_or_value.empty();
}

bool PropertyNode::IsMatching(const std::string& pattern) const {
  // Looking for exact property match. Expel a trailing whildcard from
  // the property filter to handle filters like AXRole*.
  return name_or_value.compare(0, name_or_value.find_last_of('*'), pattern) ==
         0;
}

bool PropertyNode::IsArray() const {
  return name_or_value == "[]";
}

bool PropertyNode::IsDict() const {
  return name_or_value == "{}";
}

base::Optional<int> PropertyNode::AsInt() const {
  int value = 0;
  if (!base::StringToInt(name_or_value, &value)) {
    return base::nullopt;
  }
  return value;
}

const PropertyNode* PropertyNode::FindKey(const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == refkey) {
      return &param;
    }
  }
  return nullptr;
}

base::Optional<std::string> PropertyNode::FindStringKey(
    const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == refkey) {
      return param.name_or_value;
    }
  }
  return base::nullopt;
}

base::Optional<int> PropertyNode::FindIntKey(const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == refkey) {
      return param.AsInt();
    }
  }
  return base::nullopt;
}

std::string PropertyNode::ToString() const {
  std::string out;
  for (const auto& index : line_indexes) {
    if (!out.empty()) {
      out += ',';
    }
    out += index;
  }
  if (!out.empty()) {
    out += ';';
  }

  if (!key.empty()) {
    out += key + ": ";
  }
  out += name_or_value;
  if (parameters.size()) {
    out += '(';
    for (size_t i = 0; i < parameters.size(); i++) {
      if (i != 0) {
        out += ", ";
      }
      out += parameters[i].ToString();
    }
    out += ')';
  }
  return out;
}

// private
PropertyNode::PropertyNode(PropertyNode::iterator key_begin,
                           PropertyNode::iterator key_end,
                           const std::string& name_or_value)
    : key(key_begin, key_end), name_or_value(name_or_value) {}
PropertyNode::PropertyNode(PropertyNode::iterator begin,
                           PropertyNode::iterator end)
    : name_or_value(begin, end) {}
PropertyNode::PropertyNode(PropertyNode::iterator key_begin,
                           PropertyNode::iterator key_end,
                           PropertyNode::iterator value_begin,
                           PropertyNode::iterator value_end)
    : key(key_begin, key_end), name_or_value(value_begin, value_end) {}

// private static
PropertyNode::iterator PropertyNode::Parse(PropertyNode* node,
                                           PropertyNode::iterator begin,
                                           PropertyNode::iterator end) {
  auto iter = begin;
  auto key_begin = end, key_end = end;
  while (iter != end) {
    // Subnode begins: create a new node, record its name and parse its
    // arguments.
    if (*iter == '(') {
      node->parameters.push_back(PropertyNode(key_begin, key_end, begin, iter));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for arrays, which have [arg1, ..., argN]
    // form.
    if (*iter == '[') {
      node->parameters.push_back(PropertyNode(key_begin, key_end, "[]"));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for dictionaries of {key1: value1, ...,
    // key2: value2} form.
    if (*iter == '{') {
      node->parameters.push_back(PropertyNode(key_begin, key_end, "{}"));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode ends.
    if (*iter == ')' || *iter == ']' || *iter == '}') {
      if (begin != iter) {
        node->parameters.push_back(
            PropertyNode(key_begin, key_end, begin, iter));
        key_begin = key_end = end;
      }
      return ++iter;
    }

    // Dictionary key
    auto maybe_key_end = end;
    if (*iter == ':') {
      maybe_key_end = iter++;
    }

    // Skip spaces, adjust new node start.
    if (*iter == ' ') {
      if (maybe_key_end != end) {
        key_begin = begin;
        key_end = maybe_key_end;
      }
      begin = ++iter;
      continue;
    }

    // Subsequent scalar param case.
    if (*iter == ',' && begin != iter) {
      node->parameters.push_back(PropertyNode(key_begin, key_end, begin, iter));
      iter++;
      key_begin = key_end = end;
      begin = iter;
      continue;
    }

    iter++;
  }

  // Single scalar param case.
  if (begin != iter) {
    node->parameters.push_back(PropertyNode(begin, iter));
  }
  return iter;
}

//
// AccessibilityTreeFormatter
//

AccessibilityTreeFormatter::PropertyFilter::PropertyFilter(
    const PropertyFilter&) = default;

AccessibilityTreeFormatter::PropertyFilter::PropertyFilter(
    const std::string& str,
    Type type)
    : match_str(str), type(type) {
  size_t index = str.find(';');
  if (index != std::string::npos) {
    filter_str = str.substr(0, index);
    if (index + 1 < str.length()) {
      match_str = str.substr(index + 1, std::string::npos);
    }
  }
  property_str = match_str.substr(0, match_str.find('='));
}

AccessibilityTreeFormatter::TestPass AccessibilityTreeFormatter::GetTestPass(
    size_t index) {
  std::vector<content::AccessibilityTreeFormatter::TestPass> passes =
      content::AccessibilityTreeFormatter::GetTestPasses();
  CHECK_LT(index, passes.size());
  return passes[index];
}

// static
std::string AccessibilityTreeFormatterBase::DumpAccessibilityTreeFromManager(
    BrowserAccessibilityManager* ax_mgr,
    bool internal,
    std::vector<PropertyFilter> property_filters) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter;
  if (internal)
    formatter = std::make_unique<AccessibilityTreeFormatterBlink>();
  else
    formatter = Create();
  std::string accessibility_contents;
  formatter->SetPropertyFilters(property_filters);
  std::unique_ptr<base::DictionaryValue> dict =
      static_cast<AccessibilityTreeFormatterBase*>(formatter.get())
          ->BuildAccessibilityTree(ax_mgr->GetRoot());
  formatter->FormatAccessibilityTree(*dict, &accessibility_contents);
  return accessibility_contents;
}

bool AccessibilityTreeFormatter::MatchesPropertyFilters(
    const std::vector<PropertyFilter>& property_filters,
    const std::string& text,
    bool default_result) {
  bool allow = default_result;
  for (const auto& filter : property_filters) {
    // Either
    //   1) the line matches a filter pattern, for example, AXSubrole=* filter
    //      will match AXSubrole=AXTerm line or
    //   2) a property on the line is exactly equal to the filter pattern, for
    //      example, AXSubrole filter will match AXSubrole=AXTerm line.
    if (base::MatchPattern(text, filter.match_str) ||
        (filter.match_str.length() > 0 &&
         filter.match_str.find('=') == std::string::npos &&
         filter.match_str[filter.match_str.length() - 1] != '*' &&
         base::MatchPattern(text, filter.match_str + "=*"))) {
      switch (filter.type) {
        case PropertyFilter::ALLOW_EMPTY:
          allow = true;
          break;
        case PropertyFilter::ALLOW:
          allow = (!base::MatchPattern(text, "*=''"));
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
    std::string value;
    if (!dict.GetString(filter.property, &value)) {
      continue;
    }
    if (base::MatchPattern(value, filter.pattern)) {
      return true;
    }
  }
  return false;
}

AccessibilityTreeFormatterBase::AccessibilityTreeFormatterBase() = default;

AccessibilityTreeFormatterBase::~AccessibilityTreeFormatterBase() = default;

void AccessibilityTreeFormatterBase::FormatAccessibilityTree(
    const base::DictionaryValue& dict,
    std::string* contents) {
  RecursiveFormatAccessibilityTree(dict, contents);
}

void AccessibilityTreeFormatterBase::FormatAccessibilityTreeForTesting(
    ui::AXPlatformNodeDelegate* root,
    std::string* contents) {
  auto* node_internal = BrowserAccessibility::FromAXPlatformNodeDelegate(root);
  DCHECK(node_internal);
  FormatAccessibilityTree(*BuildAccessibilityTree(node_internal), contents);
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
    std::string* contents,
    int depth) {
  // Check dictionary against node filters, may require us to skip this node
  // and its children.
  if (MatchesNodeFilters(dict))
    return;

  std::string indent = std::string(depth * kIndentSymbolCount, kIndentSymbol);
  std::string line = indent + ProcessTreeForOutput(dict);
  if (line.find(kSkipString) != std::string::npos)
    return;

  // Normalize any Windows-style line endings by removing \r.
  base::RemoveChars(line, "\r", &line);

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line, "\n", "<newline>", &line);

  *contents += line + "\n";
  if (line.find(kSkipChildren) != std::string::npos)
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

std::vector<PropertyNode>
AccessibilityTreeFormatterBase::PropertyFilterNodesFor(
    const std::string& line_index) const {
  std::vector<PropertyNode> list;
  for (const auto& filter : property_filters_) {
    PropertyNode property_node = PropertyNode::FromPropertyFilter(filter);

    // Filter out if doesn't match line index (if specified).
    if (!property_node.line_indexes.empty() &&
        std::find(property_node.line_indexes.begin(),
                  property_node.line_indexes.end(),
                  line_index) == property_node.line_indexes.end()) {
      continue;
    }

    switch (filter.type) {
      case PropertyFilter::ALLOW_EMPTY:
      case PropertyFilter::ALLOW:
        list.push_back(std::move(property_node));
        break;
      case PropertyFilter::DENY:
        break;
      default:
        break;
    }
  }
  return list;
}

bool AccessibilityTreeFormatterBase::HasMatchAllPropertyFilter() const {
  for (const auto& filter : property_filters_) {
    if (filter.type == PropertyFilter::ALLOW && filter.match_str == "*") {
      return true;
    }
  }
  return false;
}

bool AccessibilityTreeFormatterBase::MatchesPropertyFilters(
    const std::string& text,
    bool default_result) const {
  return AccessibilityTreeFormatter::MatchesPropertyFilters(
      property_filters_, text, default_result);
}

bool AccessibilityTreeFormatterBase::MatchesNodeFilters(
    const base::DictionaryValue& dict) const {
  return AccessibilityTreeFormatter::MatchesNodeFilters(node_filters_, dict);
}

std::string AccessibilityTreeFormatterBase::FormatCoordinates(
    const base::DictionaryValue& value,
    const std::string& name,
    const std::string& x_name,
    const std::string& y_name) {
  int x, y;
  value.GetInteger(x_name, &x);
  value.GetInteger(y_name, &y);
  return base::StringPrintf("%s=(%d, %d)", name.c_str(), x, y);
}

std::string AccessibilityTreeFormatterBase::FormatRectangle(
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
  return base::StringPrintf("%s=(%d, %d, %d, %d)", name.c_str(), left, top,
                            width, height);
}

bool AccessibilityTreeFormatterBase::WriteAttribute(bool include_by_default,
                                                    const std::string& attr,
                                                    std::string* line) {
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
    std::vector<PropertyFilter>* property_filters,
    std::string filter,
    PropertyFilter::Type type) {
  property_filters->push_back(PropertyFilter(filter, type));
}

void AccessibilityTreeFormatterBase::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {}

}  // namespace content
