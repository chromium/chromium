// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TOOLS_UTILS_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TOOLS_UTILS_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "content/public/browser/accessibility_tree_formatter.h"

namespace content {
namespace a11y {

/**
 * Return true if the given object is internal BrowserAccessibilityCocoa.
 */
bool IsBrowserAccessibilityCocoa(const id node);

/**
 * Returns true if the given object is AXUIElement.
 */
bool IsAXUIElement(const id node);

/**
 * Returns children of an accessible object, either AXUIElement or
 * BrowserAccessibilityCocoa.
 */
NSArray* ChildrenOf(const id node);

/**
 * Returns (parameterized) attributes of an accessible object, (either
 * AXUIElement or BrowserAccessibilityCocoa).
 */
NSArray* AttributeNamesOf(const id node);
NSArray* ParameterizedAttributeNamesOf(const id node);

/**
 * Returns (parameterized) attribute value on a given node (either AXUIElement
 * or BrowserAccessibilityCocoa)
 */
id AttributeValueOf(const id node, NSString* attribute);
id ParameterizedAttributeValueOf(const id node,
                                 NSString* attribute,
                                 id parameter);

/**
 * Return AXElement in a tree by a given criteria.
 */
using FindCriteria = base::RepeatingCallback<bool(const AXUIElementRef)>;
AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                               const FindCriteria& criteria);

/**
 * Returns AXUIElement and its application process id by a given tree selector.
 */
std::pair<AXUIElementRef, int> FindAXUIElement(
    const AccessibilityTreeFormatter::TreeSelector&);

}  // namespace a11y
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TOOLS_UTILS_MAC_H_
