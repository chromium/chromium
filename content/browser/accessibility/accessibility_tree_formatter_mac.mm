// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_browser.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

// This file uses the deprecated NSObject accessibility interface.
// TODO(crbug.com/948844): Migrate to the new NSAccessibility interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

using base::StringPrintf;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using std::string;

namespace content {

namespace {

const char kPositionDictAttr[] = "position";
const char kXCoordDictAttr[] = "x";
const char kYCoordDictAttr[] = "y";
const char kSizeDictAttr[] = "size";
const char kWidthDictAttr[] = "width";
const char kHeightDictAttr[] = "height";
const char kRangeLocDictAttr[] = "loc";
const char kRangeLenDictAttr[] = "len";

std::unique_ptr<base::DictionaryValue> PopulatePosition(
    const BrowserAccessibility& node) {
  DCHECK(node.instance_active());
  BrowserAccessibilityManager* root_manager = node.manager()->GetRootManager();
  DCHECK(root_manager);

  std::unique_ptr<base::DictionaryValue> position(new base::DictionaryValue);
  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and less
  // confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  BrowserAccessibility* root = root_manager->GetRoot();
  BrowserAccessibilityCocoa* cocoa_root = ToBrowserAccessibilityCocoa(root);
  NSPoint root_position = [[cocoa_root position] pointValue];
  NSSize root_size = [[cocoa_root size] sizeValue];
  int root_top = -static_cast<int>(root_position.y + root_size.height);
  int root_left = static_cast<int>(root_position.x);

  BrowserAccessibilityCocoa* cocoa_node =
      ToBrowserAccessibilityCocoa(const_cast<BrowserAccessibility*>(&node));
  NSPoint node_position = [[cocoa_node position] pointValue];
  NSSize node_size = [[cocoa_node size] sizeValue];

  position->SetInteger(kXCoordDictAttr,
                       static_cast<int>(node_position.x - root_left));
  position->SetInteger(
      kYCoordDictAttr,
      static_cast<int>(-node_position.y - node_size.height - root_top));
  return position;
}

std::unique_ptr<base::DictionaryValue> PopulateSize(
    const BrowserAccessibilityCocoa* cocoa_node) {
  std::unique_ptr<base::DictionaryValue> size(new base::DictionaryValue);
  NSSize node_size = [[cocoa_node size] sizeValue];
  size->SetInteger(kHeightDictAttr, static_cast<int>(node_size.height));
  size->SetInteger(kWidthDictAttr, static_cast<int>(node_size.width));
  return size;
}

std::unique_ptr<base::DictionaryValue> PopulateRange(NSRange range) {
  std::unique_ptr<base::DictionaryValue> rangeDict(new base::DictionaryValue);
  rangeDict->SetInteger(kRangeLocDictAttr, static_cast<int>(range.location));
  rangeDict->SetInteger(kRangeLenDictAttr, static_cast<int>(range.length));
  return rangeDict;
}

// Returns true if |value| is an NSValue containing a NSRange.
bool IsRangeValue(id value) {
  if (![value isKindOfClass:[NSValue class]])
    return false;
  return 0 == strcmp([value objCType], @encode(NSRange));
}

std::unique_ptr<base::Value> PopulateObject(id value);

std::unique_ptr<base::ListValue> PopulateArray(NSArray* array) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (NSUInteger i = 0; i < [array count]; i++)
    list->Append(PopulateObject([array objectAtIndex:i]));
  return list;
}

std::unique_ptr<base::Value> StringForBrowserAccessibility(
    BrowserAccessibilityCocoa* obj) {
  NSMutableArray* tokens = [[NSMutableArray alloc] init];

  // Always include the role
  id role = [obj role];
  [tokens addObject:role];

  // If the role is "group", include the role description as well.
  id roleDescription = [obj roleDescription];
  if ([role isEqualToString:NSAccessibilityGroupRole] &&
      roleDescription != nil && ![roleDescription isEqualToString:@""] &&
      ![roleDescription isEqualToString:@"group"]) {
    [tokens addObject:roleDescription];
  }

  // Include the description, title, or value - the first one not empty.
  id title = [obj title];
  id description = [obj descriptionForAccessibility];
  id value = [obj value];
  if (description && ![description isEqual:@""]) {
    [tokens addObject:description];
  } else if (title && ![title isEqual:@""]) {
    [tokens addObject:title];
  } else if (value && ![value isEqual:@""]) {
    [tokens addObject:value];
  }

  NSString* result = [tokens componentsJoinedByString:@" "];
  return std::unique_ptr<base::Value>(
      new base::Value(SysNSStringToUTF16(result)));
}

std::unique_ptr<base::Value> PopulateObject(id value) {
  if ([value isKindOfClass:[NSArray class]])
    return std::unique_ptr<base::Value>(PopulateArray((NSArray*)value));
  if (IsRangeValue(value))
    return std::unique_ptr<base::Value>(PopulateRange([value rangeValue]));
  if ([value isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    std::string str;
    StringForBrowserAccessibility(value)->GetAsString(&str);
    return std::unique_ptr<base::Value>(
        StringForBrowserAccessibility((BrowserAccessibilityCocoa*)value));
  }

  return std::unique_ptr<base::Value>(new base::Value(
      SysNSStringToUTF16([NSString stringWithFormat:@"%@", value])));
}

NSArray* AllAttributesArray() {
  static NSArray* all_attributes = [@[
    NSAccessibilityRoleDescriptionAttribute,
    NSAccessibilityTitleAttribute,
    NSAccessibilityValueAttribute,
    NSAccessibilityMinValueAttribute,
    NSAccessibilityMaxValueAttribute,
    NSAccessibilityValueDescriptionAttribute,
    NSAccessibilityDescriptionAttribute,
    NSAccessibilityHelpAttribute,
    @"AXInvalid",
    NSAccessibilityDisclosingAttribute,
    NSAccessibilityDisclosureLevelAttribute,
    @"AXAccessKey",
    @"AXARIAAtomic",
    @"AXARIABusy",
    @"AXARIAColumnCount",
    @"AXARIAColumnIndex",
    @"AXARIALive",
    @"AXARIARelevant",
    @"AXARIARowCount",
    @"AXARIARowIndex",
    @"AXARIASetSize",
    @"AXARIAPosInSet",
    @"AXAutocomplete",
    @"AXAutocompleteValue",
    @"AXBlockQuoteLevel",
    NSAccessibilityColumnHeaderUIElementsAttribute,
    NSAccessibilityColumnIndexRangeAttribute,
    @"AXDOMIdentifier",
    @"AXDropEffects",
    @"AXEditableAncestor",
    NSAccessibilityEnabledAttribute,
    NSAccessibilityExpandedAttribute,
    @"AXFocusableAncestor",
    NSAccessibilityFocusedAttribute,
    @"AXGrabbed",
    NSAccessibilityHeaderAttribute,
    @"AXHasPopup",
    @"AXHasPopupValue",
    @"AXHighestEditableAncestor",
    NSAccessibilityIndexAttribute,
    @"AXLanguage",
    @"AXLoaded",
    @"AXLoadingProcess",
    NSAccessibilityNumberOfCharactersAttribute,
    NSAccessibilitySortDirectionAttribute,
    NSAccessibilityOrientationAttribute,
    NSAccessibilityPlaceholderValueAttribute,
    @"AXRequired",
    NSAccessibilityRowHeaderUIElementsAttribute,
    NSAccessibilityRowIndexRangeAttribute,
    NSAccessibilitySelectedAttribute,
    NSAccessibilitySelectedChildrenAttribute,
    NSAccessibilityTitleUIElementAttribute,
    NSAccessibilityURLAttribute,
    NSAccessibilityVisibleCharacterRangeAttribute,
    NSAccessibilityVisibleChildrenAttribute,
    @"AXVisited",
    @"AXLinkedUIElements"
  ] retain];

  return all_attributes;
}

}  // namespace

class AccessibilityTreeFormatterMac : public AccessibilityTreeFormatterBrowser {
 public:
  explicit AccessibilityTreeFormatterMac();
  ~AccessibilityTreeFormatterMac() override;

  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;

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
};

// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatter::Create() {
  return std::make_unique<AccessibilityTreeFormatterMac>();
}

// static
std::vector<AccessibilityTreeFormatter::TestPass>
AccessibilityTreeFormatter::GetTestPasses() {
  return {
      {"blink", &AccessibilityTreeFormatterBlink::CreateBlink},
      {"mac", &AccessibilityTreeFormatter::Create},
  };
}

AccessibilityTreeFormatterMac::AccessibilityTreeFormatterMac() {}

AccessibilityTreeFormatterMac::~AccessibilityTreeFormatterMac() {}

void AccessibilityTreeFormatterMac::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {
  AddPropertyFilter(property_filters, "AXValueAutofill*");
  AddPropertyFilter(property_filters, "AXAutocomplete*");
}
void AccessibilityTreeFormatterMac::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());
  BrowserAccessibilityCocoa* cocoa_node =
      ToBrowserAccessibilityCocoa(const_cast<BrowserAccessibility*>(&node));
  NSArray* supportedAttributes = [cocoa_node accessibilityAttributeNames];

  string role = SysNSStringToUTF8(
      [cocoa_node accessibilityAttributeValue:NSAccessibilityRoleAttribute]);
  dict->SetString(SysNSStringToUTF8(NSAccessibilityRoleAttribute), role);

  NSString* subrole =
      [cocoa_node accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
  if (subrole != nil) {
    dict->SetString(SysNSStringToUTF8(NSAccessibilitySubroleAttribute),
                    SysNSStringToUTF8(subrole));
  }

  for (NSString* requestedAttribute in AllAttributesArray()) {
    if (![supportedAttributes containsObject:requestedAttribute])
      continue;
    id value = [cocoa_node accessibilityAttributeValue:requestedAttribute];
    if (value != nil) {
      dict->Set(SysNSStringToUTF8(requestedAttribute), PopulateObject(value));
    }
  }
  dict->Set(kPositionDictAttr, PopulatePosition(node));
  dict->Set(kSizeDictAttr, PopulateSize(cocoa_node));
}

base::string16 AccessibilityTreeFormatterMac::ProcessTreeForOutput(
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_dict_result) {
  base::string16 error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  base::string16 line;
  if (show_ids()) {
    int id_value;
    dict.GetInteger("id", &id_value);
    WriteAttribute(true, base::NumberToString16(id_value), &line);
  }

  NSArray* defaultAttributes =
      [NSArray arrayWithObjects:NSAccessibilityTitleAttribute,
                                NSAccessibilityTitleUIElementAttribute,
                                NSAccessibilityDescriptionAttribute,
                                NSAccessibilityHelpAttribute,
                                NSAccessibilityValueAttribute, nil];
  string s_value;
  dict.GetString(SysNSStringToUTF8(NSAccessibilityRoleAttribute), &s_value);
  WriteAttribute(true, s_value, &line);

  string subroleAttribute = SysNSStringToUTF8(NSAccessibilitySubroleAttribute);
  if (dict.GetString(subroleAttribute, &s_value)) {
    WriteAttribute(
        false, StringPrintf("%s=%s", subroleAttribute.c_str(), s_value.c_str()),
        &line);
  }

  for (NSString* requestedAttribute in AllAttributesArray()) {
    string requestedAttributeUTF8 = SysNSStringToUTF8(requestedAttribute);
    if (dict.GetString(requestedAttributeUTF8, &s_value)) {
      WriteAttribute([defaultAttributes containsObject:requestedAttribute],
                     StringPrintf("%s='%s'", requestedAttributeUTF8.c_str(),
                                  s_value.c_str()),
                     &line);
      continue;
    }
    const base::Value* value;
    if (dict.Get(requestedAttributeUTF8, &value)) {
      std::string json_value;
      base::JSONWriter::Write(*value, &json_value);
      WriteAttribute([defaultAttributes containsObject:requestedAttribute],
                     StringPrintf("%s=%s", requestedAttributeUTF8.c_str(),
                                  json_value.c_str()),
                     &line);
    }
  }
  const base::DictionaryValue* d_value = NULL;
  if (dict.GetDictionary(kPositionDictAttr, &d_value)) {
    WriteAttribute(false,
                   FormatCoordinates(*d_value, kPositionDictAttr,
                                     kXCoordDictAttr, kYCoordDictAttr),
                   &line);
  }
  if (dict.GetDictionary(kSizeDictAttr, &d_value)) {
    WriteAttribute(false,
                   FormatCoordinates(*d_value, kSizeDictAttr, kWidthDictAttr,
                                     kHeightDictAttr),
                   &line);
  }

  return line;
}

base::FilePath::StringType
AccessibilityTreeFormatterMac::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

const string AccessibilityTreeFormatterMac::GetAllowEmptyString() {
  return "@MAC-ALLOW-EMPTY:";
}

const string AccessibilityTreeFormatterMac::GetAllowString() {
  return "@MAC-ALLOW:";
}

const string AccessibilityTreeFormatterMac::GetDenyString() {
  return "@MAC-DENY:";
}

const string AccessibilityTreeFormatterMac::GetDenyNodeString() {
  return "@MAC-DENY-NODE:";
}

}  // namespace content

#pragma clang diagnostic pop
