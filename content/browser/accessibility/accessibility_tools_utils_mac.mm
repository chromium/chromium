// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tools_utils_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"

// error: 'accessibilityAttributeNames' is deprecated: first deprecated in
// macOS 10.10 - Use the NSAccessibility protocol methods instead (see
// NSAccessibilityProtocols.h
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace content {
namespace a11y {

using TreeSelector = AccessibilityTreeFormatter::TreeSelector;
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
  if (IsBrowserAccessibilityCocoa(node)) {
    return [node children];
  }

  if (IsAXUIElement(node)) {
    CFTypeRef children_ref;
    if ((AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(node),
                                       kAXChildrenAttribute, &children_ref)) ==
        kAXErrorSuccess) {
      return static_cast<NSArray*>(children_ref);
    }
    return nil;
  }

  NOTREACHED();
  return nil;
}

NSArray* AttributeNamesOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node)) {
    return [node accessibilityAttributeNames];
  }

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    if (AXUIElementCopyAttributeNames(static_cast<AXUIElementRef>(node),
                                      &attributes_ref) == kAXErrorSuccess) {
      return static_cast<NSArray*>(attributes_ref);
    }
    return nil;
  }

  NOTREACHED();
  return nil;
}

NSArray* ParameterizedAttributeNamesOf(const id node) {
  if (IsBrowserAccessibilityCocoa(node)) {
    return [node accessibilityParameterizedAttributeNames];
  }

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    if (AXUIElementCopyParameterizedAttributeNames(
            static_cast<AXUIElementRef>(node), &attributes_ref) ==
        kAXErrorSuccess) {
      return static_cast<NSArray*>(attributes_ref);
    }
    return nil;
  }

  NOTREACHED();
  return nil;
}

id AttributeValueOf(const id node, NSString* attribute) {
  if (IsBrowserAccessibilityCocoa(node)) {
    return [node accessibilityAttributeValue:attribute];
  }

  if (IsAXUIElement(node)) {
    CFTypeRef value_ref;
    if ((AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(node),
                                       static_cast<CFStringRef>(attribute),
                                       &value_ref)) == kAXErrorSuccess) {
      return static_cast<id>(value_ref);
    }
    return nil;
  }

  NOTREACHED();
  return nil;
}

id ParameterizedAttributeValueOf(const id node,
                                 NSString* attribute,
                                 id parameter) {
  if (IsBrowserAccessibilityCocoa(node)) {
    return [node accessibilityAttributeValue:attribute forParameter:parameter];
  }

  if (IsAXUIElement(node)) {
    CFTypeRef value_ref;
    if ((AXUIElementCopyParameterizedAttributeValue(
            static_cast<AXUIElementRef>(node),
            static_cast<CFStringRef>(attribute),
            static_cast<CFTypeRef>(parameter), &value_ref)) ==
        kAXErrorSuccess) {
      return static_cast<id>(value_ref);
    }
    return nil;
  }

  NOTREACHED();
  return nil;
}

AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                               const FindCriteria& criteria) {
  if (criteria.Run(node)) {
    return node;
  }

  NSArray* children = ChildrenOf(static_cast<id>(node));
  for (id child in children) {
    AXUIElementRef found =
        FindAXUIElement(static_cast<AXUIElementRef>(child), criteria);
    if (found != nil) {
      return found;
    }
  }

  return nil;
}

std::pair<AXUIElementRef, int> FindAXUIElement(
    const AccessibilityTreeFormatter::TreeSelector& selector) {
  NSArray* windows = static_cast<NSArray*>(CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID));

  std::string title;
  if (selector.types & TreeSelector::Chrome) {
    title = kChromeTitle;
  } else if (selector.types & TreeSelector::Chromium) {
    title = kChromiumTitle;
  } else if (selector.types & TreeSelector::Firefox) {
    title = kFirefoxTitle;
  } else if (selector.types & TreeSelector::Safari) {
    title = kSafariTitle;
  }

  for (NSDictionary* window_info in windows) {
    int pid =
        [static_cast<NSNumber*>([window_info objectForKey:@"kCGWindowOwnerPID"])
            intValue];
    std::string window_name = SysNSStringToUTF8(static_cast<NSString*>(
        [window_info objectForKey:@"kCGWindowOwnerName"]));

    if (window_name == selector.pattern) {
      return {AXUIElementCreateApplication(pid), pid};
    }

    if (window_name == title) {
      AXUIElementRef node = AXUIElementCreateApplication(pid);
      if (selector.types & TreeSelector::ActiveTab) {
        node = FindAXUIElement(
            node, base::BindRepeating([](const AXUIElementRef node) {
              // Only active tab in exposed in browsers, thus find first
              // AXWebArea role.
              NSString* role = AttributeValueOf(static_cast<id>(node),
                                                NSAccessibilityRoleAttribute);
              return SysNSStringToUTF8(role) == "AXWebArea";
            }));
      }

      if (node) {
        return {node, pid};
      }
    }
  }
  return {nil, 0};
}

}  // namespace a11y
}  // namespace content

#pragma clang diagnostic pop
