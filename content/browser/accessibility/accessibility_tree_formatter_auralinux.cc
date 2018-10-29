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
  const base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
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

AccessibilityTreeFormatterAuraLinux::AccessibilityTreeFormatterAuraLinux() {
}

AccessibilityTreeFormatterAuraLinux::~AccessibilityTreeFormatterAuraLinux() {
}

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

void AccessibilityTreeFormatterAuraLinux::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());
  BrowserAccessibilityAuraLinux* acc_obj =
      ToBrowserAccessibilityAuraLinux(const_cast<BrowserAccessibility*>(&node));
  acc_obj->GetNode()->AddAccessibilityTreeProperties(dict);
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
    "container-atomic",
    "container-busy",
    "container-live",
    "container-relevant",
    "display",
    "explicit-name",
    "haspopup",
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
    "setsize",
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
  node.GetList("states", &states_value);
  for (auto it = states_value->begin(); it != states_value->end(); ++it) {
    std::string state_value;
    if (it->GetAsString(&state_value))
      WriteAttribute(false, state_value, &line);
  }

  int id_value;
  node.GetInteger("id", &id_value);
  WriteAttribute(false, base::StringPrintf("id=%d", id_value), &line);

  for (const char* attribute_name : ATK_OBJECT_ATTRIBUTES) {
    std::string attribute_value;
    if (node.GetString(attribute_name, &attribute_value)) {
      WriteAttribute(
          false,
          base::StringPrintf("%s:%s", attribute_name, attribute_value.c_str()),
          &line);
    }
  }

  return line;
}

const base::FilePath::StringType
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

}  // namespace content
