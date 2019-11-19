// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_browser.h"

#include <atspi/atspi.h>
#include <dbus/dbus.h>

#include <iostream>
#include <utility>

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_auralinux.h"
#include "content/browser/accessibility/browser_accessibility_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11.h"

namespace content {

class AccessibilityTreeFormatterAuraLinux
    : public AccessibilityTreeFormatterBrowser {
 public:
  AccessibilityTreeFormatterAuraLinux();
  ~AccessibilityTreeFormatterAuraLinux() override;

 private:
  base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  const std::string GetDenyNodeString() override;
  void AddProperties(const BrowserAccessibility& node,
                     base::DictionaryValue* dict) override;

  base::string16 ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForProcess(
      base::ProcessId pid) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForWindow(
      gfx::AcceleratedWidget hwnd) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForPattern(
      const base::StringPiece& pattern) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeWithNode(
      AtspiAccessible* node);

  void AddTextProperties(AtkText* atk_text, base::DictionaryValue* dict);
  void AddValueProperties(AtkObject* atk_object, base::DictionaryValue* dict);
  void AddTableProperties(AtkObject* atk_object, base::DictionaryValue* dict);
  void AddTableCellProperties(const ui::AXPlatformNodeAuraLinux* node,
                              AtkObject* atk_object,
                              base::DictionaryValue* dict);

  void RecursiveBuildAccessibilityTree(AtspiAccessible* node,
                                       base::DictionaryValue* dict);
  virtual void AddProperties(AtspiAccessible* node,
                             base::DictionaryValue* dict);
};

// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatter::Create() {
  return std::make_unique<AccessibilityTreeFormatterAuraLinux>();
}

// static
std::vector<AccessibilityTreeFormatter::TestPass>
AccessibilityTreeFormatter::GetTestPasses() {
  return {
      {"blink", &AccessibilityTreeFormatterBlink::CreateBlink},
      {"linux", &AccessibilityTreeFormatter::Create},
  };
}

AccessibilityTreeFormatterAuraLinux::AccessibilityTreeFormatterAuraLinux() {}

AccessibilityTreeFormatterAuraLinux::~AccessibilityTreeFormatterAuraLinux() {}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterAuraLinux::BuildAccessibilityTreeForPattern(
    const base::StringPiece& pattern) {
  // AT-SPI2 always expects the first parameter to this call to be zero.
  AtspiAccessible* desktop = atspi_get_desktop(0);
  CHECK(desktop);

  GError* error = nullptr;
  int child_count = atspi_accessible_get_child_count(desktop, &error);
  if (error) {
    LOG(ERROR) << "Failed to get children of root accessible object"
               << error->message;
    g_clear_error(&error);
    return nullptr;
  }

  std::vector<std::pair<std::string, AtspiAccessible*>> matched_children;
  for (int i = 0; i < child_count; i++) {
    AtspiAccessible* child =
        atspi_accessible_get_child_at_index(desktop, i, &error);
    if (error) {
      g_clear_error(&error);
      continue;
    }

    char* name = atspi_accessible_get_name(child, &error);
    if (!error && name && base::MatchPattern(name, pattern)) {
      matched_children.push_back(std::make_pair(name, child));
    }

    free(name);
  }

  if (matched_children.size() == 1) {
    return BuildAccessibilityTreeWithNode(matched_children[0].second);
  }

  if (matched_children.size()) {
    LOG(ERROR) << "Matched more than one application. "
               << "Try to make a more specific pattern.";
    for (auto& match : matched_children) {
      LOG(ERROR) << "  * " << match.first;
    }
  }

  return nullptr;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterAuraLinux::BuildAccessibilityTreeForProcess(
    base::ProcessId pid) {
  LOG(ERROR) << "Aura Linux does not yet support building trees for processes";
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterAuraLinux::BuildAccessibilityTreeForWindow(
    gfx::AcceleratedWidget window) {
  LOG(ERROR) << "Aura Linux does not yet support building trees for window ids";
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterAuraLinux::BuildAccessibilityTreeWithNode(
    AtspiAccessible* node) {
  CHECK(node);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  RecursiveBuildAccessibilityTree(node, dict.get());

  return dict;
}

void AccessibilityTreeFormatterAuraLinux::RecursiveBuildAccessibilityTree(
    AtspiAccessible* node,
    base::DictionaryValue* dict) {
  AddProperties(node, dict);

  GError* error = nullptr;
  int child_count = atspi_accessible_get_child_count(node, &error);
  if (error) {
    g_clear_error(&error);
    return;
  }

  if (child_count <= 0)
    return;

  auto children = std::make_unique<base::ListValue>();
  for (int i = 0; i < child_count; i++) {
    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);

    AtspiAccessible* child =
        atspi_accessible_get_child_at_index(node, i, &error);
    if (error) {
      child_dict->SetString("error", "[Error retrieving child]");
      g_clear_error(&error);
      continue;
    }

    CHECK(child);
    RecursiveBuildAccessibilityTree(child, child_dict.get());
    children->Append(std::move(child_dict));
  }

  dict->Set(kChildrenDictAttr, std::move(children));
}

void AccessibilityTreeFormatterAuraLinux::AddTextProperties(
    AtkText* atk_text,
    base::DictionaryValue* dict) {
  auto text_values = std::make_unique<base::ListValue>();
  int character_count = atk_text_get_character_count(atk_text);
  text_values->AppendString(
      base::StringPrintf("character_count=%i", character_count));

  int caret_offset = atk_text_get_caret_offset(atk_text);
  if (caret_offset != -1)
    text_values->AppendString(
        base::StringPrintf("caret_offset=%i", caret_offset));

  int selection_start, selection_end;
  char* selection_text =
      atk_text_get_selection(atk_text, 0, &selection_start, &selection_end);
  if (selection_text) {
    g_free(selection_text);
    text_values->AppendString(
        base::StringPrintf("selection_start=%i", selection_start));
    text_values->AppendString(
        base::StringPrintf("selection_end=%i", selection_end));
  }

  auto add_attribute_set_values = [](gpointer value, gpointer list) {
    const AtkAttribute* attribute = static_cast<const AtkAttribute*>(value);
    static_cast<base::ListValue*>(list)->AppendString(
        base::StringPrintf("%s=%s", attribute->name, attribute->value));
  };

  int current_offset = 0, start_offset, end_offset;
  while (current_offset < character_count) {
    AtkAttributeSet* text_attributes = atk_text_get_run_attributes(
        atk_text, current_offset, &start_offset, &end_offset);
    text_values->AppendString(base::StringPrintf("offset=%i", start_offset));
    g_slist_foreach(text_attributes, add_attribute_set_values,
                    text_values.get());
    atk_attribute_set_free(text_attributes);

    current_offset = end_offset;
  }

  dict->Set("text", std::move(text_values));
}

void AccessibilityTreeFormatterAuraLinux::AddValueProperties(
    AtkObject* atk_object,
    base::DictionaryValue* dict) {
  if (!ATK_IS_VALUE(atk_object))
    return;

  auto value_properties = std::make_unique<base::ListValue>();
  AtkValue* value = ATK_VALUE(atk_object);
  GValue current = G_VALUE_INIT;
  g_value_init(&current, G_TYPE_FLOAT);
  atk_value_get_current_value(value, &current);
  value_properties->AppendString(
      base::StringPrintf("current=%f", g_value_get_float(&current)));

  GValue minimum = G_VALUE_INIT;
  g_value_init(&minimum, G_TYPE_FLOAT);
  atk_value_get_minimum_value(value, &minimum);
  value_properties->AppendString(
      base::StringPrintf("minimum=%f", g_value_get_float(&minimum)));

  GValue maximum = G_VALUE_INIT;
  g_value_init(&maximum, G_TYPE_FLOAT);
  atk_value_get_maximum_value(value, &maximum);
  value_properties->AppendString(
      base::StringPrintf("maximum=%f", g_value_get_float(&maximum)));
  dict->Set("value", std::move(value_properties));
}

void AccessibilityTreeFormatterAuraLinux::AddTableProperties(
    AtkObject* atk_object,
    base::DictionaryValue* dict) {
  if (!ATK_IS_TABLE(atk_object))
    return;

  // Column details.
  AtkTable* table = ATK_TABLE(atk_object);
  int n_cols = atk_table_get_n_columns(table);
  auto table_properties = std::make_unique<base::ListValue>();
  table_properties->AppendString(base::StringPrintf("cols=%i", n_cols));

  std::vector<std::string> col_headers;
  for (int i = 0; i < n_cols; i++) {
    std::string header = atk_table_get_column_description(table, i);
    if (!header.empty())
      col_headers.push_back(base::StringPrintf("'%s'", header.c_str()));
  }

  if (!col_headers.size())
    col_headers.push_back("NONE");

  table_properties->AppendString(base::StringPrintf(
      "headers=(%s);", base::JoinString(col_headers, ", ").c_str()));

  // Row details.
  int n_rows = atk_table_get_n_rows(table);
  table_properties->AppendString(base::StringPrintf("rows=%i", n_rows));

  std::vector<std::string> row_headers;
  for (int i = 0; i < n_rows; i++) {
    std::string header = atk_table_get_row_description(table, i);
    if (!header.empty())
      row_headers.push_back(base::StringPrintf("'%s'", header.c_str()));
  }

  if (!row_headers.size())
    row_headers.push_back("NONE");

  table_properties->AppendString(base::StringPrintf(
      "headers=(%s);", base::JoinString(row_headers, ", ").c_str()));

  // Caption details.
  AtkObject* caption = atk_table_get_caption(table);
  table_properties->AppendString(
      base::StringPrintf("caption=%s;", caption ? "true" : "false"));

  // Summarize information about the cells from the table's perspective here.
  std::vector<std::string> span_info;
  for (int r = 0; r < n_rows; r++) {
    for (int c = 0; c < n_cols; c++) {
      int row_span = atk_table_get_row_extent_at(table, r, c);
      int col_span = atk_table_get_column_extent_at(table, r, c);
      if (row_span != 1 || col_span != 1) {
        span_info.push_back(base::StringPrintf("cell at %i,%i: %ix%i", r, c,
                                               row_span, col_span));
      }
    }
  }
  if (!span_info.size())
    span_info.push_back("all: 1x1");

  table_properties->AppendString(base::StringPrintf(
      "spans=(%s)", base::JoinString(span_info, ", ").c_str()));
  dict->Set("table", std::move(table_properties));
}

void AccessibilityTreeFormatterAuraLinux::AddTableCellProperties(
    const ui::AXPlatformNodeAuraLinux* node,
    AtkObject* atk_object,
    base::DictionaryValue* dict) {
  AtkRole role = atk_object_get_role(atk_object);
  if (role != ATK_ROLE_TABLE_CELL && role != ATK_ROLE_COLUMN_HEADER &&
      role != ATK_ROLE_ROW_HEADER) {
    return;
  }

  int row = 0, col = 0, row_span = 0, col_span = 0;
  int n_row_headers = 0, n_column_headers = 0;

  // Properties obtained via AtkTableCell, if possible. If we do not have at
  // least ATK 2.12, use the same logic in our AtkTableCell implementation so
  // that tests can still be run.
  if (ui::AtkTableCellInterface::Exists()) {
    AtkTableCell* cell = G_TYPE_CHECK_INSTANCE_CAST(
        (atk_object), ui::AtkTableCellInterface::GetType(), AtkTableCell);

    ui::AtkTableCellInterface::GetRowColumnSpan(cell, &row, &col, &row_span,
                                                &col_span);

    GPtrArray* column_headers =
        ui::AtkTableCellInterface::GetColumnHeaderCells(cell);
    n_column_headers = column_headers->len;
    g_ptr_array_unref(column_headers);

    GPtrArray* row_headers = ui::AtkTableCellInterface::GetRowHeaderCells(cell);
    n_row_headers = row_headers->len;
    g_ptr_array_unref(row_headers);
  } else {
    row = node->GetTableRow().value_or(-1);
    col = node->GetTableColumn().value_or(-1);
    row_span = node->GetTableRowSpan().value_or(0);
    col_span = node->GetTableColumnSpan().value_or(0);
    if (role == ATK_ROLE_TABLE_CELL) {
      n_column_headers = node->GetDelegate()->GetColHeaderNodeIds(col).size();
      n_row_headers = node->GetDelegate()->GetRowHeaderNodeIds(row).size();
    }
  }

  std::vector<std::string> cell_info;
  cell_info.push_back(base::StringPrintf("row=%i", row));
  cell_info.push_back(base::StringPrintf("col=%i", col));
  cell_info.push_back(base::StringPrintf("row_span=%i", row_span));
  cell_info.push_back(base::StringPrintf("col_span=%i", col_span));
  cell_info.push_back(base::StringPrintf("n_row_headers=%i", n_row_headers));
  cell_info.push_back(base::StringPrintf("n_col_headers=%i", n_column_headers));

  auto cell_properties = std::make_unique<base::ListValue>();
  cell_properties->AppendString(
      base::StringPrintf("(%s)", base::JoinString(cell_info, ", ").c_str()));
  dict->Set("cell", std::move(cell_properties));
}

void AccessibilityTreeFormatterAuraLinux::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());
  BrowserAccessibilityAuraLinux* acc_obj =
      ToBrowserAccessibilityAuraLinux(const_cast<BrowserAccessibility*>(&node));
  DCHECK(acc_obj);

  ui::AXPlatformNodeAuraLinux* ax_platform_node = acc_obj->GetNode();
  DCHECK(ax_platform_node);

  AtkObject* atk_object = ax_platform_node->GetNativeViewAccessible();
  DCHECK(atk_object);

  AtkRole role = atk_object_get_role(atk_object);
  if (role != ATK_ROLE_UNKNOWN) {
    dict->SetString("role", AtkRoleToString(role));
  }

  const gchar* name = atk_object_get_name(atk_object);
  if (name)
    dict->SetString("name", std::string(name));
  const gchar* description = atk_object_get_description(atk_object);
  if (description)
    dict->SetString("description", std::string(description));

  AtkStateSet* state_set = atk_object_ref_state_set(atk_object);
  auto states = std::make_unique<base::ListValue>();
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type))
      states->AppendString(atk_state_type_get_name(state_type));
  }
  dict->Set("states", std::move(states));
  g_object_unref(state_set);

  AtkRelationSet* relation_set = atk_object_ref_relation_set(atk_object);
  auto relations = std::make_unique<base::ListValue>();
  for (int i = ATK_RELATION_NULL; i < ATK_RELATION_LAST_DEFINED; i++) {
    AtkRelationType relation_type = static_cast<AtkRelationType>(i);
    if (atk_relation_set_contains(relation_set, relation_type))
      relations->AppendString(atk_relation_type_get_name(relation_type));
  }
  dict->Set("relations", std::move(relations));
  g_object_unref(relation_set);

  AtkAttributeSet* attributes = atk_object_get_attributes(atk_object);
  for (AtkAttributeSet* attr = attributes; attr; attr = attr->next) {
    AtkAttribute* attribute = static_cast<AtkAttribute*>(attr->data);
    dict->SetString(attribute->name, attribute->value);
  }
  atk_attribute_set_free(attributes);

  if (ATK_IS_TEXT(atk_object))
    AddTextProperties(ATK_TEXT(atk_object), dict);
  AddValueProperties(atk_object, dict);
  AddTableProperties(atk_object, dict);
  AddTableCellProperties(ax_platform_node, atk_object, dict);
}

void AccessibilityTreeFormatterAuraLinux::AddProperties(
    AtspiAccessible* node,
    base::DictionaryValue* dict) {
  GError* error = nullptr;
  char* role_name = atspi_accessible_get_role_name(node, &error);
  if (!error)
    dict->SetString("role", role_name);
  g_clear_error(&error);
  free(role_name);

  char* name = atspi_accessible_get_name(node, &error);
  if (!error)
    dict->SetString("name", name);
  g_clear_error(&error);
  free(name);

  error = nullptr;
  char* description = atspi_accessible_get_description(node, &error);
  if (!error)
    dict->SetString("description", description);
  g_clear_error(&error);
  free(description);

  error = nullptr;
  GHashTable* attributes = atspi_accessible_get_attributes(node, &error);
  if (!error) {
    GHashTableIter i;
    void* key = nullptr;
    void* value = nullptr;

    g_hash_table_iter_init(&i, attributes);
    while (g_hash_table_iter_next(&i, &key, &value)) {
      dict->SetString(static_cast<char*>(key), static_cast<char*>(value));
    }
  }
  g_clear_error(&error);
  g_hash_table_unref(attributes);

  AtspiStateSet* atspi_states = atspi_accessible_get_state_set(node);
  GArray* state_array = atspi_state_set_get_states(atspi_states);
  auto states = std::make_unique<base::ListValue>();
  for (unsigned i = 0; i < state_array->len; i++) {
    AtspiStateType state_type = g_array_index(state_array, AtspiStateType, i);
    states->AppendString(ATSPIStateToString(state_type));
  }
  dict->Set("states", std::move(states));
  g_array_free(state_array, TRUE);
  g_object_unref(atspi_states);
}

const char* const ATK_OBJECT_ATTRIBUTES[] = {
    "atomic",
    "autocomplete",
    "busy",
    "checkable",
    "class",
    "colcount",
    "colindex",
    "colspan",
    "coltext",
    "container-atomic",
    "container-busy",
    "container-live",
    "container-relevant",
    "current",
    "display",
    "dropeffect",
    "explicit-name",
    "grabbed",
    "haspopup",
    "hidden",
    "id",
    "keyshortcuts",
    "level",
    "live",
    "placeholder",
    "posinset",
    "relevant",
    "roledescription",
    "rowcount",
    "rowindex",
    "rowspan",
    "rowtext",
    "setsize",
    "sort",
    "src",
    "table-cell-index",
    "tag",
    "text-input-type",
    "valuemin",
    "valuemax",
    "valuenow",
    "valuetext",
    "xml-roles",
};

base::string16 AccessibilityTreeFormatterAuraLinux::ProcessTreeForOutput(
    const base::DictionaryValue& node,
    base::DictionaryValue* filtered_dict_result) {
  base::string16 error_value;
  if (node.GetString("error", &error_value))
    return error_value;

  base::string16 line;
  std::string role_value;
  node.GetString("role", &role_value);
  if (!role_value.empty()) {
    WriteAttribute(true, base::StringPrintf("[%s]", role_value.c_str()), &line);
  }

  std::string name_value;
  if (node.GetString("name", &name_value))
    WriteAttribute(true, base::StringPrintf("name='%s'", name_value.c_str()),
                   &line);

  std::string description_value;
  node.GetString("description", &description_value);
  WriteAttribute(
      false, base::StringPrintf("description='%s'", description_value.c_str()),
      &line);

  const base::ListValue* states_value;
  if (node.GetList("states", &states_value)) {
    for (auto it = states_value->begin(); it != states_value->end(); ++it) {
      std::string state_value;
      if (it->GetAsString(&state_value))
        WriteAttribute(false, state_value, &line);
    }
  }

  const base::ListValue* relations_value;
  if (node.GetList("relations", &relations_value)) {
    for (auto it = relations_value->begin(); it != relations_value->end();
         ++it) {
      std::string relation_value;
      if (it->GetAsString(&relation_value)) {
        // By default, exclude embedded-by because that should appear on every
        // top-level document object. The other relation types are less common
        // and thus almost always of interest when testing.
        WriteAttribute(relation_value != "embedded-by", relation_value, &line);
      }
    }
  }

  for (const char* attribute_name : ATK_OBJECT_ATTRIBUTES) {
    std::string attribute_value;
    if (node.GetString(attribute_name, &attribute_value)) {
      WriteAttribute(
          false,
          base::StringPrintf("%s:%s", attribute_name, attribute_value.c_str()),
          &line);
    }
  }

  const base::ListValue* value_info;
  if (node.GetList("value", &value_info)) {
    for (auto it = value_info->begin(); it != value_info->end(); ++it) {
      std::string value_property;
      if (it->GetAsString(&value_property))
        WriteAttribute(true, value_property, &line);
    }
  }

  const base::ListValue* table_info;
  if (node.GetList("table", &table_info)) {
    for (auto it = table_info->begin(); it != table_info->end(); ++it) {
      std::string table_property;
      if (it->GetAsString(&table_property))
        WriteAttribute(true, table_property, &line);
    }
  }

  const base::ListValue* cell_info;
  if (node.GetList("cell", &cell_info)) {
    for (auto it = cell_info->begin(); it != cell_info->end(); ++it) {
      std::string cell_property;
      if (it->GetAsString(&cell_property))
        WriteAttribute(true, cell_property, &line);
    }
  }

  const base::ListValue* text_info;
  if (node.GetList("text", &text_info)) {
    for (auto it = text_info->begin(); it != text_info->end(); ++it) {
      std::string cell_property;
      if (it->GetAsString(&cell_property))
        WriteAttribute(false, cell_property, &line);
    }
  }

  return line;
}

base::FilePath::StringType
AccessibilityTreeFormatterAuraLinux::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-auralinux.txt");
}

const std::string AccessibilityTreeFormatterAuraLinux::GetAllowEmptyString() {
  return "@AURALINUX-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterAuraLinux::GetAllowString() {
  return "@AURALINUX-ALLOW:";
}

const std::string AccessibilityTreeFormatterAuraLinux::GetDenyString() {
  return "@AURALINUX-DENY:";
}

const std::string AccessibilityTreeFormatterAuraLinux::GetDenyNodeString() {
  return "@AURALINUX-DENY-NODE:";
}

}  // namespace content
