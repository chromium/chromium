// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_browser.h"

namespace content {

class AccessibilityTreeFormatterStub
    : public AccessibilityTreeFormatterBrowser {
 public:
  explicit AccessibilityTreeFormatterStub();
  ~AccessibilityTreeFormatterStub() override;

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
};

#if !defined(PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL)
// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatter::Create() {
  return std::make_unique<AccessibilityTreeFormatterStub>();
}
#endif

AccessibilityTreeFormatterStub::AccessibilityTreeFormatterStub()
    : AccessibilityTreeFormatterBrowser() {}

AccessibilityTreeFormatterStub::~AccessibilityTreeFormatterStub() {
}

void AccessibilityTreeFormatterStub::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());
}

base::string16 AccessibilityTreeFormatterStub::ProcessTreeForOutput(
    const base::DictionaryValue& node,
    base::DictionaryValue* filtered_dict_result) {
  int id_value;
  node.GetInteger("id", &id_value);
  return base::IntToString16(id_value);
}

const base::FilePath::StringType
AccessibilityTreeFormatterStub::GetExpectedFileSuffix() {
  return base::FilePath::StringType();
}

const std::string AccessibilityTreeFormatterStub::GetAllowEmptyString() {
  return std::string();
}

const std::string AccessibilityTreeFormatterStub::GetAllowString() {
  return std::string();
}

const std::string AccessibilityTreeFormatterStub::GetDenyString() {
  return std::string();
}

}  // namespace content
