// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tools_utils_mac.h"

#include "base/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"

// error: 'accessibilityAttributeNames' is deprecated: first deprecated in
// macOS 10.10 - Use the NSAccessibility protocol methods instead (see
// NSAccessibilityProtocols.h
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace content {
namespace a11y {

using base::SysNSStringToUTF8;

const char kChromeTitle[] = "Google Chrome";
const char kChromiumTitle[] = "Chromium";
const char kFirefoxTitle[] = "Firefox";
const char kSafariTitle[] = "Safari";

bool IsBrowserAccessibilityCocoa(const id node) {
  return [node isKindOfClass:[BrowserAccessibilityCocoa class]];
}

bool IsAXUIElement(const id node) {
  return CFGetTypeID(node) == AXUIElementGetTypeID();
}

NSArray* ChildrenOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node))
    return [node children];

  if (IsAXUIElement(node)) {
    CFTypeRef children_ref;
    if ((AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(node),
                                       kAXChildrenAttribute, &children_ref)) ==
        kAXErrorSuccess)
      return static_cast<NSArray*>(children_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSSize SizeOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node)) {
    return [[static_cast<BrowserAccessibilityCocoa*>(node) size] sizeValue];
  }

  if (!IsAXUIElement(node)) {
    NOTREACHED()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakeSize(0, 0);
  }

  id value = AttributeValueOf(node, NSAccessibilitySizeAttribute);
  if (value && CFGetTypeID(value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType(static_cast<AXValueRef>(value));
    if (type == kAXValueCGSizeType) {
      NSSize size;
      if (AXValueGetValue(static_cast<AXValueRef>(value), type, &size)) {
        return size;
      }
    }
  }
  return NSMakeSize(0, 0);
}

NSPoint PositionOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node)) {
    return
        [[static_cast<BrowserAccessibilityCocoa*>(node) position] pointValue];
  }

  if (!IsAXUIElement(node)) {
    NOTREACHED()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakePoint(0, 0);
  }

  id value = AttributeValueOf(node, NSAccessibilityPositionAttribute);
  if (value && CFGetTypeID(value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType(static_cast<AXValueRef>(value));
    if (type == kAXValueCGPointType) {
      NSPoint point;
      if (AXValueGetValue(static_cast<AXValueRef>(value), type, &point)) {
        return point;
      }
    }
  }
  return NSMakePoint(0, 0);
}

NSArray* AttributeNamesOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node))
    return [node accessibilityAttributeNames];

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    if (AXUIElementCopyAttributeNames(static_cast<AXUIElementRef>(node),
                                      &attributes_ref) == kAXErrorSuccess)
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSArray* ParameterizedAttributeNamesOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node))
    return [node accessibilityParameterizedAttributeNames];

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    if (AXUIElementCopyParameterizedAttributeNames(
            static_cast<AXUIElementRef>(node), &attributes_ref) ==
        kAXErrorSuccess)
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

id AttributeValueOf(const id node, NSString* attribute) {
  if (IsBrowserAccessibilityCocoa(node))
    return [node accessibilityAttributeValue:attribute];

  if (IsAXUIElement(node)) {
    CFTypeRef value_ref;
    if ((AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(node),
                                       static_cast<CFStringRef>(attribute),
                                       &value_ref)) == kAXErrorSuccess)
      return static_cast<id>(value_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

id ParameterizedAttributeValueOf(const id node,
                                 NSString* attribute,
                                 id parameter) {
  if (IsBrowserAccessibilityCocoa(node))
    return [node accessibilityAttributeValue:attribute forParameter:parameter];

  if (IsAXUIElement(node)) {
    CFTypeRef value_ref;
    if ((AXUIElementCopyParameterizedAttributeValue(
            static_cast<AXUIElementRef>(node),
            static_cast<CFStringRef>(attribute),
            static_cast<CFTypeRef>(parameter), &value_ref)) == kAXErrorSuccess)
      return static_cast<id>(value_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

bool IsAttributeSettable(const id node, NSString* attribute) {
  if (IsBrowserAccessibilityCocoa(node))
    return [node accessibilityIsAttributeSettable:attribute];

  if (IsAXUIElement(node)) {
    Boolean settable;
    if (AXUIElementIsAttributeSettable(static_cast<AXUIElementRef>(node),
                                       static_cast<CFStringRef>(attribute),
                                       &settable) == kAXErrorSuccess)
      return static_cast<bool>(settable);
    return false;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return false;
}

void SetAttributeValueOf(const id node, NSString* attribute, id value) {
  if (IsBrowserAccessibilityCocoa(node)) {
    [node accessibilitySetValue:value forAttribute:attribute];
    return;
  }

  if (IsAXUIElement(node)) {
    AXUIElementSetAttributeValue(static_cast<AXUIElementRef>(node),
                                 static_cast<CFStringRef>(attribute),
                                 static_cast<CFTypeRef>(value));
    return;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                               const FindCriteria& criteria) {
  if (criteria.Run(node))
    return node;

  NSArray* children = ChildrenOf(static_cast<id>(node));
  for (id child in children) {
    AXUIElementRef found =
        FindAXUIElement(static_cast<AXUIElementRef>(child), criteria);
    if (found != nil)
      return found;
  }

  return nil;
}

std::pair<AXUIElementRef, int> FindAXUIElement(const AXTreeSelector& selector) {
  NSArray* windows = static_cast<NSArray*>(CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID));

  std::string title;
  if (selector.types & AXTreeSelector::Chrome) {
    title = kChromeTitle;
  } else if (selector.types & AXTreeSelector::Chromium) {
    title = kChromiumTitle;
  } else if (selector.types & AXTreeSelector::Firefox) {
    title = kFirefoxTitle;
  } else if (selector.types & AXTreeSelector::Safari) {
    title = kSafariTitle;
  } else {
    LOG(ERROR) << selector.AppName()
               << " application is not supported on the system";
    return {nil, 0};
  }

  for (NSDictionary* window_info in windows) {
    int pid =
        [static_cast<NSNumber*>([window_info objectForKey:@"kCGWindowOwnerPID"])
            intValue];
    std::string window_name = SysNSStringToUTF8(static_cast<NSString*>(
        [window_info objectForKey:@"kCGWindowOwnerName"]));

    if (window_name == selector.pattern)
      return {AXUIElementCreateApplication(pid), pid};

    if (window_name == title) {
      AXUIElementRef node = AXUIElementCreateApplication(pid);
      if (selector.types & AXTreeSelector::ActiveTab)
        node = FindAXUIElement(
            node, base::BindRepeating([](const AXUIElementRef node) {
              // Only active tab in exposed in browsers, thus find first
              // AXWebArea role.
              NSString* role = AttributeValueOf(static_cast<id>(node),
                                                NSAccessibilityRoleAttribute);
              return SysNSStringToUTF8(role) == "AXWebArea";
            }));

      if (node)
        return {node, pid};
    }
  }
  return {nil, 0};
}

}  // namespace a11y
}  // namespace content

#pragma clang diagnostic pop
