// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TOOLS_UTILS_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TOOLS_UTILS_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

using ui::AXTreeSelector;

namespace content {
namespace a11y {

//
// Return true if the given object is internal BrowserAccessibilityCocoa.
//
CONTENT_EXPORT bool IsBrowserAccessibilityCocoa(const id node);

//
// Returns true if the given object is AXUIElement.
//
CONTENT_EXPORT bool IsAXUIElement(const id node);

//
// Returns children of an accessible object, either AXUIElement or
// BrowserAccessibilityCocoa.
//
CONTENT_EXPORT NSArray* ChildrenOf(const id node);

//
// Returns AXSize and AXPosition attributes for an accessible object.
//
CONTENT_EXPORT NSSize SizeOf(const id node);
CONTENT_EXPORT NSPoint PositionOf(const id node);

//
// Returns (parameterized) attributes of an accessible object, (either
// AXUIElement or BrowserAccessibilityCocoa).
//
CONTENT_EXPORT NSArray* AttributeNamesOf(const id node);
CONTENT_EXPORT NSArray* ParameterizedAttributeNamesOf(const id node);

//
// Returns (parameterized) attribute value on a given node (either AXUIElement
// or BrowserAccessibilityCocoa).
//
CONTENT_EXPORT id AttributeValueOf(const id node, NSString* attribute);
CONTENT_EXPORT id ParameterizedAttributeValueOf(const id node,
                                                NSString* attribute,
                                                id parameter);

//
// Performs the given selector on the given node and returns the result. If
// the node does not conform to the NSAccessibility protocol or the selector is
// not found, then returns nullopt.
CONTENT_EXPORT absl::optional<id> PerformSelector(const id node,
                                                  const std::string& selector);

//
// Returns true if an attribute value can be changed on a given node
// (either AXUIElement or BrowserAccessibilityCocoa).
//
CONTENT_EXPORT bool IsAttributeSettable(const id node, NSString* attribute);

//
// Sets attribute value on a given node (either AXUIElement or
// BrowserAccessibilityCocoa).
//
CONTENT_EXPORT void SetAttributeValueOf(const id node,
                                        NSString* attribute,
                                        id value);

// Returns a list of actions supported on a given accessible node (either
// AXUIElement or BrowserAccessibilityCocoa).
CONTENT_EXPORT NSArray* ActionNamesOf(const id node);

// Performs action on a given accessible node (either AXUIElement or
// BrowserAccessibilityCocoa).
CONTENT_EXPORT void PerformAction(const id node, NSString* action);

//
// Returns DOM id of a given node (either AXUIElement or
// BrowserAccessibilityCocoa).
//
CONTENT_EXPORT std::string GetDOMId(const id node);

//
// Return AXElement in a tree by a given criteria.
//
using FindCriteria = base::RepeatingCallback<bool(const AXUIElementRef)>;
CONTENT_EXPORT AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                                              const FindCriteria& criteria);

//
// Returns AXUIElement and its application process id by a given tree selector.
//
CONTENT_EXPORT std::pair<AXUIElementRef, int> FindAXUIElement(
    const AXTreeSelector&);

AXUIElementRef FindAXWindowChild(AXUIElementRef parent,
                                 const std::string& pattern);

}  // namespace a11y
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TOOLS_UTILS_MAC_H_
