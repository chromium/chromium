// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// Used to store changes in edit fields, required by VoiceOver in order to
// support character echo and other announcements during editing.
struct AXTextEdit {
  AXTextEdit() = default;
  AXTextEdit(base::string16 inserted_text, base::string16 deleted_text)
      : inserted_text(inserted_text), deleted_text(deleted_text) {}

  bool IsEmpty() const { return inserted_text.empty() && deleted_text.empty(); }

  base::string16 inserted_text;
  base::string16 deleted_text;
};

}  // namespace content

// BrowserAccessibilityCocoa is a cocoa wrapper around the BrowserAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to the browser process over IPC.
// This class converts it into a format Cocoa can query.
@interface BrowserAccessibilityCocoa : NSAccessibilityElement {
 @private
  content::BrowserAccessibility* owner_;
  base::scoped_nsobject<NSMutableArray> children_;
  // Stores the previous value of an edit field.
  base::string16 oldValue_;
}

// This creates a cocoa browser accessibility object around
// the cross platform BrowserAccessibility object, which can't be nullptr.
- (instancetype)initWithObject:(content::BrowserAccessibility*)accessibility;

// Clear this object's pointer to the wrapped BrowserAccessibility object
// because the wrapped object has been deleted, but this object may
// persist if the system still has references to it.
- (void)detach;

// Invalidate children for a non-ignored ancestor (including self).
- (void)childrenChanged;

// Convenience method to get the internal, cross-platform role
// from browserAccessibility_.
- (ax::mojom::Role)internalRole;

// Convenience method to get the BrowserAccessibilityDelegate from
// the manager.
- (content::BrowserAccessibilityDelegate*)delegate;

// Get the BrowserAccessibility that this object wraps.
- (content::BrowserAccessibility*)owner;

// Computes the text that was added or deleted in a text field after an edit.
- (content::AXTextEdit)computeTextEdit;

// Determines if this object is alive, i.e. it hasn't been detached.
- (BOOL)instanceActive;

// Convert from the view's local coordinate system (with the origin in the upper
// left) to the primary NSScreen coordinate system (with the origin in the lower
// left).
- (NSRect)rectInScreen:(gfx::Rect)rect;

// Return the method name for the given attribute. For testing only.
- (NSString*)methodNameForAttribute:(NSString*)attribute;

// Swap the children array with the given scoped_nsobject.
- (void)swapChildren:(base::scoped_nsobject<NSMutableArray>*)other;

- (NSString*)valueForRange:(NSRange)range;
- (NSAttributedString*)attributedValueForRange:(NSRange)range;

// Internally-used property.
@property(nonatomic, readonly) NSPoint origin;

@property(nonatomic, readonly) NSString* accessKey;
@property(nonatomic, readonly) NSNumber* ariaAtomic;
@property(nonatomic, readonly) NSNumber* ariaBusy;
@property(nonatomic, readonly) NSString* ariaLive;
@property(nonatomic, readonly) NSNumber* ariaPosInSet;
@property(nonatomic, readonly) NSString* ariaRelevant;
@property(nonatomic, readonly) NSNumber* ariaSetSize;
@property(nonatomic, readonly) NSArray* children;
@property(nonatomic, readonly) NSArray* columns;
@property(nonatomic, readonly) NSArray* columnHeaders;
@property(nonatomic, readonly) NSValue* columnIndexRange;
@property(nonatomic, readonly) NSString* descriptionForAccessibility;
@property(nonatomic, readonly) NSNumber* disclosing;
@property(nonatomic, readonly) id disclosedByRow;
@property(nonatomic, readonly) NSNumber* disclosureLevel;
@property(nonatomic, readonly) id disclosedRows;
@property(nonatomic, readonly) NSString* dropEffects;
// Returns the object at the root of the current edit field, if any.
@property(nonatomic, readonly) id editableAncestor;
@property(nonatomic, readonly) NSNumber* enabled;
// Returns a text marker that points to the last character in the document that
// can be selected with Voiceover.
@property(nonatomic, readonly) id endTextMarker;
@property(nonatomic, readonly) NSNumber* expanded;
@property(nonatomic, readonly) NSNumber* focused;
@property(nonatomic, readonly) NSNumber* grabbed;
@property(nonatomic, readonly) id header;
@property(nonatomic, readonly) NSString* help;
// isIgnored returns whether or not the accessibility object
// should be ignored by the accessibility hierarchy.
@property(nonatomic, readonly, getter=isIgnored) BOOL ignored;
// Index of a row, column, or tree item.
@property(nonatomic, readonly) NSNumber* index;
@property(nonatomic, readonly) NSNumber* insertionPointLineNumber;
@property(nonatomic, readonly) NSString* invalid;
@property(nonatomic, readonly) NSNumber* isMultiSelectable;
@property(nonatomic, readonly) NSString* placeholderValue;
@property(nonatomic, readonly) NSNumber* loaded;
@property(nonatomic, readonly) NSNumber* loadingProgress;
@property(nonatomic, readonly) NSNumber* maxValue;
@property(nonatomic, readonly) NSNumber* minValue;
@property(nonatomic, readonly) NSNumber* numberOfCharacters;
@property(nonatomic, readonly) NSString* orientation;
@property(nonatomic, readonly) id parent;
@property(nonatomic, readonly) NSValue* position;
@property(nonatomic, readonly) NSNumber* required;
// A string indicating the role of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* role;
@property(nonatomic, readonly) NSString* roleDescription;
@property(nonatomic, readonly) NSArray* rowHeaders;
@property(nonatomic, readonly) NSValue* rowIndexRange;
@property(nonatomic, readonly) NSArray* rows;
// The object is selected as a whole.
@property(nonatomic, readonly) NSNumber* selected;
@property(nonatomic, readonly) NSArray* selectedChildren;
@property(nonatomic, readonly) NSString* selectedText;
@property(nonatomic, readonly) NSValue* selectedTextRange;
@property(nonatomic, readonly) id selectedTextMarkerRange;
@property(nonatomic, readonly) NSValue* size;
@property(nonatomic, readonly) NSString* sortDirection;
// Returns a text marker that points to the first character in the document that
// can be selected with Voiceover.
@property(nonatomic, readonly) id startTextMarker;
// A string indicating the subrole of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* subrole;
// The tabs owned by a tablist.
@property(nonatomic, readonly) NSArray* tabs;
@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) id titleUIElement;
@property(nonatomic, readonly) NSURL* url;
@property(nonatomic, readonly) NSString* value;
@property(nonatomic, readonly) NSString* valueDescription;
@property(nonatomic, readonly) NSValue* visibleCharacterRange;
@property(nonatomic, readonly) NSArray* visibleCells;
@property(nonatomic, readonly) NSArray* visibleChildren;
@property(nonatomic, readonly) NSArray* visibleColumns;
@property(nonatomic, readonly) NSArray* visibleRows;
@property(nonatomic, readonly) NSNumber* visited;
@property(nonatomic, readonly) id window;
@end

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
