// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/accessibility/browser_accessibility_cocoa.h"

#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <utility>

#include "base/mac/availability.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/accessibility/browser_accessibility_position.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/mac/coordinate_conversion.h"

#import "ui/accessibility/platform/ax_platform_node_mac.h"

using BrowserAccessibilityPositionInstance =
    content::BrowserAccessibilityPosition::AXPositionInstance;
using SerializedPosition =
    content::BrowserAccessibilityPosition::SerializedPosition;
using AXPlatformRange =
    ui::AXRange<BrowserAccessibilityPositionInstance::element_type>;
using AXTextMarkerRangeRef = CFTypeRef;
using AXTextMarkerRef = CFTypeRef;
using StringAttribute = ax::mojom::StringAttribute;
using content::BrowserAccessibilityPosition;
using content::AccessibilityMatchPredicate;
using content::BrowserAccessibility;
using content::BrowserAccessibilityDelegate;
using content::BrowserAccessibilityManager;
using content::BrowserAccessibilityManagerMac;
using content::ContentClient;
using content::OneShotAccessibilityTreeSearch;
using ui::AXNodeData;
using ui::AXTreeIDRegistry;

static_assert(
    std::is_trivially_copyable<SerializedPosition>::value,
    "SerializedPosition must be POD because it's used to back an AXTextMarker");

namespace {

// Private WebKit accessibility attributes.
NSString* const NSAccessibilityARIAAtomicAttribute = @"AXARIAAtomic";
NSString* const NSAccessibilityARIABusyAttribute = @"AXARIABusy";
NSString* const NSAccessibilityARIAColumnCountAttribute = @"AXARIAColumnCount";
NSString* const NSAccessibilityARIAColumnIndexAttribute = @"AXARIAColumnIndex";
NSString* const NSAccessibilityARIALiveAttribute = @"AXARIALive";
NSString* const NSAccessibilityARIAPosInSetAttribute = @"AXARIAPosInSet";
NSString* const NSAccessibilityARIARelevantAttribute = @"AXARIARelevant";
NSString* const NSAccessibilityARIARowCountAttribute = @"AXARIARowCount";
NSString* const NSAccessibilityARIARowIndexAttribute = @"AXARIARowIndex";
NSString* const NSAccessibilityARIASetSizeAttribute = @"AXARIASetSize";
NSString* const NSAccessibilityAccessKeyAttribute = @"AXAccessKey";
NSString* const NSAccessibilityAutocompleteValueAttribute =
    @"AXAutocompleteValue";
NSString* const NSAccessibilityBlockQuoteLevelAttribute = @"AXBlockQuoteLevel";
NSString* const NSAccessibilityDOMClassList = @"AXDOMClassList";
NSString* const NSAccessibilityDOMIdentifierAttribute = @"AXDOMIdentifier";
NSString* const NSAccessibilityDropEffectsAttribute = @"AXDropEffects";
NSString* const NSAccessibilityEditableAncestorAttribute =
    @"AXEditableAncestor";
NSString* const NSAccessibilityElementBusyAttribute = @"AXElementBusy";
NSString* const NSAccessibilityFocusableAncestorAttribute =
    @"AXFocusableAncestor";
NSString* const NSAccessibilityGrabbedAttribute = @"AXGrabbed";
NSString* const NSAccessibilityHasPopupAttribute = @"AXHasPopup";
NSString* const NSAccessibilityHasPopupValueAttribute = @"AXHasPopupValue";
NSString* const NSAccessibilityHighestEditableAncestorAttribute =
    @"AXHighestEditableAncestor";
NSString* const NSAccessibilityInvalidAttribute = @"AXInvalid";
NSString* const NSAccessibilityIsMultiSelectableAttribute =
    @"AXIsMultiSelectable";
NSString* const NSAccessibilityLoadingProgressAttribute = @"AXLoadingProgress";
NSString* const NSAccessibilityOwnsAttribute = @"AXOwns";
NSString* const
    NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute =
        @"AXUIElementCountForSearchPredicate";
NSString* const
    NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute =
        @"AXUIElementsForSearchPredicate";
NSString* const NSAccessibilityVisitedAttribute = @"AXVisited";

// Private attributes for text markers.
NSString* const NSAccessibilityStartTextMarkerAttribute = @"AXStartTextMarker";
NSString* const NSAccessibilityEndTextMarkerAttribute = @"AXEndTextMarker";
NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
    @"AXSelectedTextMarkerRange";
NSString* const NSAccessibilityTextMarkerIsValidParameterizedAttribute =
    @"AXTextMarkerIsValid";
NSString* const NSAccessibilityIndexForTextMarkerParameterizedAttribute =
    @"AXIndexForTextMarker";
NSString* const NSAccessibilityTextMarkerForIndexParameterizedAttribute =
    @"AXTextMarkerForIndex";
NSString* const NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute =
    @"AXEndTextMarkerForBounds";
NSString* const NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute =
    @"AXStartTextMarkerForBounds";
NSString* const
    NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLineTextMarkerRangeForTextMarker";
// TODO(nektar): Implement programmatic text operations.
//
// NSString* const NSAccessibilityTextOperationMarkerRanges =
//    @"AXTextOperationMarkerRanges";
NSString* const NSAccessibilityUIElementForTextMarkerParameterizedAttribute =
    @"AXUIElementForTextMarker";
NSString* const
    NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute =
        @"AXTextMarkerRangeForUIElement";
NSString* const NSAccessibilityLineForTextMarkerParameterizedAttribute =
    @"AXLineForTextMarker";
NSString* const NSAccessibilityTextMarkerRangeForLineParameterizedAttribute =
    @"AXTextMarkerRangeForLine";
NSString* const NSAccessibilityStringForTextMarkerRangeParameterizedAttribute =
    @"AXStringForTextMarkerRange";
NSString* const NSAccessibilityTextMarkerForPositionParameterizedAttribute =
    @"AXTextMarkerForPosition";
NSString* const NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute =
    @"AXBoundsForTextMarkerRange";
NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRange";
NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRangeWithOptions";
NSString* const
    NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute =
        @"AXTextMarkerRangeForUnorderedTextMarkers";
NSString* const
    NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousTextMarkerForTextMarker";
NSString* const
    NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftWordTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightWordTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftLineTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightLineTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXSentenceTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXParagraphTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextWordEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousWordStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextLineEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousLineStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextSentenceEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousSentenceStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextParagraphEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousParagraphStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXStyleTextMarkerRangeForTextMarker";
NSString* const NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute =
    @"AXLengthForTextMarkerRange";

// Private attributes that can be used for testing text markers, e.g. in dump
// tree tests.
NSString* const
    NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerDebugDescription";
NSString* const
    NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerRangeDebugDescription";
NSString* const
    NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerNodeDebugDescription";

// Other private attributes.
NSString* const NSAccessibilitySelectTextWithCriteriaParameterizedAttribute =
    @"AXSelectTextWithCriteria";
NSString* const NSAccessibilityIndexForChildUIElementParameterizedAttribute =
    @"AXIndexForChildUIElement";
NSString* const NSAccessibilityValueAutofillAvailableAttribute =
    @"AXValueAutofillAvailable";
// Not currently supported by Chrome -- information not stored:
// NSString* const NSAccessibilityValueAutofilledAttribute =
// @"AXValueAutofilled"; Not currently supported by Chrome -- mismatch of types
// supported: NSString* const NSAccessibilityValueAutofillTypeAttribute =
// @"AXValueAutofillType";

// Actions.
NSString* const NSAccessibilityScrollToVisibleAction = @"AXScrollToVisible";

// A mapping from an accessibility attribute to its method name.
NSDictionary* attributeToMethodNameMap = nil;

// VoiceOver uses -1 to mean "no limit" for AXResultsLimit.
const int kAXResultsLimitNoLimit = -1;

extern "C" {

// The following are private accessibility APIs required for cursor navigation
// and text selection. VoiceOver started relying on them in Mac OS X 10.11.
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11

CFTypeID AXTextMarkerGetTypeID();

AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator,
                                   const UInt8* bytes,
                                   CFIndex length);

const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef text_marker);

size_t AXTextMarkerGetLength(AXTextMarkerRef text_marker);

CFTypeID AXTextMarkerRangeGetTypeID();

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator,
                                             AXTextMarkerRef start_marker,
                                             AXTextMarkerRef end_marker);

AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(
    AXTextMarkerRangeRef text_marker_range);

AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(
    AXTextMarkerRangeRef text_marker_range);

#endif  // MAC_OS_X_VERSION_10_11

}  // extern "C"

// AXTextMarkerCreate is a system function that makes a copy of the data buffer
// given to it.
id CreateTextMarker(BrowserAccessibilityPositionInstance position) {
  SerializedPosition serialized = position->Serialize();
  AXTextMarkerRef cf_text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized),
      sizeof(SerializedPosition));
  return [static_cast<id>(cf_text_marker) autorelease];
}

id CreateTextMarkerRange(const AXPlatformRange range) {
  SerializedPosition serialized_anchor = range.anchor()->Serialize();
  SerializedPosition serialized_focus = range.focus()->Serialize();
  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_anchor),
      sizeof(SerializedPosition)));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_focus),
      sizeof(SerializedPosition)));
  AXTextMarkerRangeRef cf_marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, start_marker, end_marker);
  return [static_cast<id>(cf_marker_range) autorelease];
}

BrowserAccessibilityPositionInstance CreatePositionFromTextMarker(
    id text_marker) {
  AXTextMarkerRef cf_text_marker = static_cast<AXTextMarkerRef>(text_marker);
  DCHECK(cf_text_marker);
  if (CFGetTypeID(cf_text_marker) != AXTextMarkerGetTypeID())
    return BrowserAccessibilityPosition::CreateNullPosition();

  if (AXTextMarkerGetLength(cf_text_marker) != sizeof(SerializedPosition))
    return BrowserAccessibilityPosition::CreateNullPosition();

  const UInt8* source_buffer = AXTextMarkerGetBytePtr(cf_text_marker);
  if (!source_buffer)
    return BrowserAccessibilityPosition::CreateNullPosition();

  return BrowserAccessibilityPosition::Unserialize(
      *reinterpret_cast<const SerializedPosition*>(source_buffer));
}

AXPlatformRange CreateRangeFromTextMarkerRange(id marker_range) {
  AXTextMarkerRangeRef cf_marker_range =
      static_cast<AXTextMarkerRangeRef>(marker_range);
  DCHECK(cf_marker_range);
  if (CFGetTypeID(cf_marker_range) != AXTextMarkerRangeGetTypeID())
    return AXPlatformRange();

  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(
      AXTextMarkerRangeCopyStartMarker(cf_marker_range));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(
      AXTextMarkerRangeCopyEndMarker(cf_marker_range));
  if (!start_marker.get() || !end_marker.get())
    return AXPlatformRange();

  BrowserAccessibilityPositionInstance anchor =
      CreatePositionFromTextMarker(static_cast<id>(start_marker.get()));
  BrowserAccessibilityPositionInstance focus =
      CreatePositionFromTextMarker(static_cast<id>(end_marker.get()));
  // |AXPlatformRange| takes ownership of its anchor and focus.
  return AXPlatformRange(std::move(anchor), std::move(focus));
}

BrowserAccessibilityPositionInstance CreateTreePosition(
    const BrowserAccessibility& object,
    int offset) {
  // A tree position is one for which the |offset| argument refers to a child
  // index instead of a character offset inside a text object.
  if (!object.instance_active())
    return BrowserAccessibilityPosition::CreateNullPosition();

  const BrowserAccessibilityManager* manager = object.manager();
  DCHECK(manager);
  return BrowserAccessibilityPosition::CreateTreePosition(
      manager->ax_tree_id(), object.GetId(), offset);
}

BrowserAccessibilityPositionInstance CreateTextPosition(
    const BrowserAccessibility& object,
    int offset,
    ax::mojom::TextAffinity affinity) {
  // A text position is one for which the |offset| argument refers to a
  // character offset inside a text object. As such, text positions are only
  // valid on platform leaf objects, e.g. static text nodes and text fields.
  if (!object.instance_active())
    return BrowserAccessibilityPosition::CreateNullPosition();

  const BrowserAccessibilityManager* manager = object.manager();
  DCHECK(manager);
  return BrowserAccessibilityPosition::CreateTextPosition(
      manager->ax_tree_id(), object.GetId(), offset, affinity);
}

AXPlatformRange CreateAXPlatformRange(const BrowserAccessibility& start_object,
                                      int start_offset,
                                      ax::mojom::TextAffinity start_affinity,
                                      const BrowserAccessibility& end_object,
                                      int end_offset,
                                      ax::mojom::TextAffinity end_affinity) {
  BrowserAccessibilityPositionInstance anchor =
      start_object.PlatformIsLeaf()
          ? CreateTextPosition(start_object, start_offset, start_affinity)
          : CreateTreePosition(start_object, start_offset);
  BrowserAccessibilityPositionInstance focus =
      end_object.PlatformIsLeaf()
          ? CreateTextPosition(end_object, end_offset, end_affinity)
          : CreateTreePosition(end_object, end_offset);
  // |AXPlatformRange| takes ownership of its anchor and focus.
  return AXPlatformRange(std::move(anchor), std::move(focus));
}

AXPlatformRange GetSelectedRange(BrowserAccessibility& owner) {
  const BrowserAccessibilityManager* manager = owner.manager();
  if (!manager)
    return {};

  const ui::AXTree::Selection unignored_selection =
      manager->ax_tree()->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  const BrowserAccessibility* anchor_object = manager->GetFromID(anchor_id);
  if (!anchor_object)
    return {};

  int32_t focus_id = unignored_selection.focus_object_id;
  const BrowserAccessibility* focus_object = manager->GetFromID(focus_id);
  if (!focus_object)
    return {};

  // |anchor_offset| and / or |focus_offset| refer to a character offset if
  // |anchor_object| / |focus_object| are text-only objects or native text
  // fields. Otherwise, they should be treated as child indices.
  int anchor_offset = unignored_selection.anchor_offset;
  int focus_offset = unignored_selection.focus_offset;
  DCHECK_GE(anchor_offset, 0);
  DCHECK_GE(focus_offset, 0);

  ax::mojom::TextAffinity anchor_affinity = unignored_selection.anchor_affinity;
  ax::mojom::TextAffinity focus_affinity = unignored_selection.focus_affinity;

  return CreateAXPlatformRange(*anchor_object, anchor_offset, anchor_affinity,
                               *focus_object, focus_offset, focus_affinity);
}

void AddMisspelledTextAttributes(const AXPlatformRange& ax_range,
                                 NSMutableAttributedString* attributed_string) {
  int anchor_start_offset = 0;
  [attributed_string beginEditing];
  for (const AXPlatformRange& leaf_text_range : ax_range) {
    DCHECK(!leaf_text_range.IsNull());
    DCHECK_EQ(leaf_text_range.anchor()->GetAnchor(),
              leaf_text_range.focus()->GetAnchor())
        << "An anchor range should only span a single object.";
    const BrowserAccessibility* anchor = leaf_text_range.focus()->GetAnchor();
    const std::vector<int32_t>& marker_types =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& marker_starts =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      if (!(marker_types[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kSpelling))) {
        continue;
      }

      int misspelling_start = anchor_start_offset + marker_starts[i];
      int misspelling_end = anchor_start_offset + marker_ends[i];
      int misspelling_length = misspelling_end - misspelling_start;
      DCHECK_LE(static_cast<unsigned long>(misspelling_end),
                [attributed_string length]);
      DCHECK_GT(misspelling_length, 0);
      [attributed_string
          addAttribute:NSAccessibilityMarkedMisspelledTextAttribute
                 value:@YES
                 range:NSMakeRange(misspelling_start, misspelling_length)];
    }

    anchor_start_offset += leaf_text_range.GetText().length();
  }
  [attributed_string endEditing];
}

NSString* GetTextForTextMarkerRange(id marker_range) {
  AXPlatformRange range = CreateRangeFromTextMarkerRange(marker_range);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

NSAttributedString* GetAttributedTextForTextMarkerRange(id marker_range) {
  AXPlatformRange ax_range = CreateRangeFromTextMarkerRange(marker_range);
  if (ax_range.IsNull())
    return nil;

  NSString* text = base::SysUTF16ToNSString(ax_range.GetText());
  if ([text length] == 0)
    return nil;

  NSMutableAttributedString* attributed_text =
      [[[NSMutableAttributedString alloc] initWithString:text] autorelease];
  // Currently, we only decorate the attributed string with misspelling
  // information.
  AddMisspelledTextAttributes(ax_range, attributed_text);
  return attributed_text;
}

// Returns an autoreleased copy of the AXNodeData's attribute.
NSString* NSStringForStringAttribute(BrowserAccessibility* browserAccessibility,
                                     StringAttribute attribute) {
  return base::SysUTF8ToNSString(
      browserAccessibility->GetStringAttribute(attribute));
}

// GetState checks the bitmask used in AXNodeData to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, ax::mojom::State state) {
  return accessibility->GetData().HasState(state);
}

// Given a search key provided to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate, return a predicate that can be added
// to OneShotAccessibilityTreeSearch.
AccessibilityMatchPredicate PredicateForSearchKey(NSString* searchKey) {
  if ([searchKey isEqualToString:@"AXAnyTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return true;
    };
  } else if ([searchKey isEqualToString:@"AXBlockquoteSameLevelSearchKey"]) {
    // TODO(dmazzoni): implement the "same level" part.
    return content::AccessibilityBlockquotePredicate;
  } else if ([searchKey isEqualToString:@"AXBlockquoteSearchKey"]) {
    return content::AccessibilityBlockquotePredicate;
  } else if ([searchKey isEqualToString:@"AXBoldFontSearchKey"]) {
    return content::AccessibilityTextStyleBoldPredicate;
  } else if ([searchKey isEqualToString:@"AXButtonSearchKey"]) {
    return content::AccessibilityButtonPredicate;
  } else if ([searchKey isEqualToString:@"AXCheckBoxSearchKey"]) {
    return content::AccessibilityCheckboxPredicate;
  } else if ([searchKey isEqualToString:@"AXControlSearchKey"]) {
    return content::AccessibilityControlPredicate;
  } else if ([searchKey isEqualToString:@"AXDifferentTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() != start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXFontChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFontColorChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFrameSearchKey"]) {
    return content::AccessibilityFramePredicate;
  } else if ([searchKey isEqualToString:@"AXGraphicSearchKey"]) {
    return content::AccessibilityGraphicPredicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel1SearchKey"]) {
    return content::AccessibilityH1Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel2SearchKey"]) {
    return content::AccessibilityH2Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel3SearchKey"]) {
    return content::AccessibilityH3Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel4SearchKey"]) {
    return content::AccessibilityH4Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel5SearchKey"]) {
    return content::AccessibilityH5Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel6SearchKey"]) {
    return content::AccessibilityH6Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingSameLevelSearchKey"]) {
    return content::AccessibilityHeadingSameLevelPredicate;
  } else if ([searchKey isEqualToString:@"AXHeadingSearchKey"]) {
    return content::AccessibilityHeadingPredicate;
  } else if ([searchKey isEqualToString:@"AXHighlightedSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXItalicFontSearchKey"]) {
    return content::AccessibilityTextStyleItalicPredicate;
  } else if ([searchKey isEqualToString:@"AXLandmarkSearchKey"]) {
    return content::AccessibilityLandmarkPredicate;
  } else if ([searchKey isEqualToString:@"AXLinkSearchKey"]) {
    return content::AccessibilityLinkPredicate;
  } else if ([searchKey isEqualToString:@"AXListSearchKey"]) {
    return content::AccessibilityListPredicate;
  } else if ([searchKey isEqualToString:@"AXLiveRegionSearchKey"]) {
    return content::AccessibilityLiveRegionPredicate;
  } else if ([searchKey isEqualToString:@"AXMisspelledWordSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXOutlineSearchKey"]) {
    return content::AccessibilityTreePredicate;
  } else if ([searchKey isEqualToString:@"AXPlainTextSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXRadioGroupSearchKey"]) {
    return content::AccessibilityRadioGroupPredicate;
  } else if ([searchKey isEqualToString:@"AXSameTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXStaticTextSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->IsTextOnlyObject();
    };
  } else if ([searchKey isEqualToString:@"AXStyleChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXTableSameLevelSearchKey"]) {
    // TODO(dmazzoni): implement the "same level" part.
    return content::AccessibilityTablePredicate;
  } else if ([searchKey isEqualToString:@"AXTableSearchKey"]) {
    return content::AccessibilityTablePredicate;
  } else if ([searchKey isEqualToString:@"AXTextFieldSearchKey"]) {
    return content::AccessibilityTextfieldPredicate;
  } else if ([searchKey isEqualToString:@"AXUnderlineSearchKey"]) {
    return content::AccessibilityTextStyleUnderlinePredicate;
  } else if ([searchKey isEqualToString:@"AXUnvisitedLinkSearchKey"]) {
    return content::AccessibilityUnvisitedLinkPredicate;
  } else if ([searchKey isEqualToString:@"AXVisitedLinkSearchKey"]) {
    return content::AccessibilityVisitedLinkPredicate;
  }

  return nullptr;
}

// Initialize a OneShotAccessibilityTreeSearch object given the parameters
// passed to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate. Return true on success.
bool InitializeAccessibilityTreeSearch(OneShotAccessibilityTreeSearch* search,
                                       id parameter) {
  if (![parameter isKindOfClass:[NSDictionary class]])
    return false;
  NSDictionary* dictionary = parameter;

  id startElementParameter = [dictionary objectForKey:@"AXStartElement"];
  if ([startElementParameter isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    BrowserAccessibilityCocoa* startNodeCocoa =
        (BrowserAccessibilityCocoa*)startElementParameter;
    search->SetStartNode([startNodeCocoa owner]);
  }

  bool immediateDescendantsOnly = false;
  NSNumber* immediateDescendantsOnlyParameter =
      [dictionary objectForKey:@"AXImmediateDescendantsOnly"];
  if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
    immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];

  bool onscreenOnly = false;
  // AXVisibleOnly actually means onscreen objects only -- nothing scrolled off.
  NSNumber* onscreenOnlyParameter = [dictionary objectForKey:@"AXVisibleOnly"];
  if ([onscreenOnlyParameter isKindOfClass:[NSNumber class]])
    onscreenOnly = [onscreenOnlyParameter boolValue];

  content::OneShotAccessibilityTreeSearch::Direction direction =
      content::OneShotAccessibilityTreeSearch::FORWARDS;
  NSString* directionParameter = [dictionary objectForKey:@"AXDirection"];
  if ([directionParameter isKindOfClass:[NSString class]]) {
    if ([directionParameter isEqualToString:@"AXDirectionNext"])
      direction = content::OneShotAccessibilityTreeSearch::FORWARDS;
    else if ([directionParameter isEqualToString:@"AXDirectionPrevious"])
      direction = content::OneShotAccessibilityTreeSearch::BACKWARDS;
  }

  int resultsLimit = kAXResultsLimitNoLimit;
  NSNumber* resultsLimitParameter = [dictionary objectForKey:@"AXResultsLimit"];
  if ([resultsLimitParameter isKindOfClass:[NSNumber class]])
    resultsLimit = [resultsLimitParameter intValue];

  std::string searchText;
  NSString* searchTextParameter = [dictionary objectForKey:@"AXSearchText"];
  if ([searchTextParameter isKindOfClass:[NSString class]])
    searchText = base::SysNSStringToUTF8(searchTextParameter);

  search->SetDirection(direction);
  search->SetImmediateDescendantsOnly(immediateDescendantsOnly);
  search->SetOnscreenOnly(onscreenOnly);
  search->SetSearchText(searchText);

  // Mac uses resultsLimit == -1 for unlimited, that that's
  // the default for OneShotAccessibilityTreeSearch already.
  // Only set the results limit if it's nonnegative.
  if (resultsLimit >= 0)
    search->SetResultLimit(resultsLimit);

  id searchKey = [dictionary objectForKey:@"AXSearchKey"];
  if ([searchKey isKindOfClass:[NSString class]]) {
    AccessibilityMatchPredicate predicate =
        PredicateForSearchKey((NSString*)searchKey);
    if (predicate)
      search->AddPredicate(predicate);
  } else if ([searchKey isKindOfClass:[NSArray class]]) {
    size_t searchKeyCount = static_cast<size_t>([searchKey count]);
    for (size_t i = 0; i < searchKeyCount; ++i) {
      id key = [searchKey objectAtIndex:i];
      if ([key isKindOfClass:[NSString class]]) {
        AccessibilityMatchPredicate predicate =
            PredicateForSearchKey((NSString*)key);
        if (predicate)
          search->AddPredicate(predicate);
      }
    }
  }

  return true;
}

void AppendTextToString(const std::string& extra_text, std::string* string) {
  if (extra_text.empty())
    return;

  if (string->empty()) {
    *string = extra_text;
    return;
  }

  *string += std::string(". ") + extra_text;
}

}  // namespace

#if defined(MAC_OS_X_VERSION_10_12) && \
    (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12)
#warning NSAccessibilityRequiredAttributeChrome \
  should be removed since the deployment target is >= 10.12
#endif

// The following private WebKit accessibility attribute became public in 10.12,
// but it can't be used on all OS because it has availability of 10.12. Instead,
// define a similarly named constant with the "Chrome" suffix, and the same
// string. This is used as the key to a dictionary, so string-comparison will
// work.
extern "C" {
NSString* const NSAccessibilityRequiredAttributeChrome = @"AXRequired";
}

// Not defined in current versions of library, but may be in the future:
#ifndef NSAccessibilityLanguageAttribute
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#endif

@implementation BrowserAccessibilityCocoa

+ (void)initialize {
  const struct {
    NSString* attribute;
    NSString* methodName;
  } attributeToMethodNameContainer[] = {
      {NSAccessibilityARIAAtomicAttribute, @"ariaAtomic"},
      {NSAccessibilityARIABusyAttribute, @"ariaBusy"},
      {NSAccessibilityARIAColumnCountAttribute, @"ariaColumnCount"},
      {NSAccessibilityARIAColumnIndexAttribute, @"ariaColumnIndex"},
      {NSAccessibilityARIALiveAttribute, @"ariaLive"},
      {NSAccessibilityARIAPosInSetAttribute, @"ariaPosInSet"},
      {NSAccessibilityARIARelevantAttribute, @"ariaRelevant"},
      {NSAccessibilityARIARowCountAttribute, @"ariaRowCount"},
      {NSAccessibilityARIARowIndexAttribute, @"ariaRowIndex"},
      {NSAccessibilityARIASetSizeAttribute, @"ariaSetSize"},
      {NSAccessibilityAccessKeyAttribute, @"accessKey"},
      {NSAccessibilityAutocompleteValueAttribute, @"autocompleteValue"},
      {NSAccessibilityBlockQuoteLevelAttribute, @"blockQuoteLevel"},
      {NSAccessibilityChildrenAttribute, @"children"},
      {NSAccessibilityColumnsAttribute, @"columns"},
      {NSAccessibilityColumnHeaderUIElementsAttribute, @"columnHeaders"},
      {NSAccessibilityColumnIndexRangeAttribute, @"columnIndexRange"},
      {NSAccessibilityContentsAttribute, @"contents"},
      {NSAccessibilityDescriptionAttribute, @"descriptionForAccessibility"},
      {NSAccessibilityDisclosingAttribute, @"disclosing"},
      {NSAccessibilityDisclosedByRowAttribute, @"disclosedByRow"},
      {NSAccessibilityDisclosureLevelAttribute, @"disclosureLevel"},
      {NSAccessibilityDisclosedRowsAttribute, @"disclosedRows"},
      {NSAccessibilityDropEffectsAttribute, @"dropEffects"},
      {NSAccessibilityDOMClassList, @"domClassList"},
      {NSAccessibilityDOMIdentifierAttribute, @"domIdentifier"},
      {NSAccessibilityEditableAncestorAttribute, @"editableAncestor"},
      {NSAccessibilityElementBusyAttribute, @"elementBusy"},
      {NSAccessibilityEnabledAttribute, @"enabled"},
      {NSAccessibilityEndTextMarkerAttribute, @"endTextMarker"},
      {NSAccessibilityExpandedAttribute, @"expanded"},
      {NSAccessibilityFocusableAncestorAttribute, @"focusableAncestor"},
      {NSAccessibilityFocusedAttribute, @"focused"},
      {NSAccessibilityGrabbedAttribute, @"grabbed"},
      {NSAccessibilityHeaderAttribute, @"header"},
      {NSAccessibilityHasPopupAttribute, @"hasPopup"},
      {NSAccessibilityHasPopupValueAttribute, @"hasPopupValue"},
      {NSAccessibilityHelpAttribute, @"help"},
      {NSAccessibilityHighestEditableAncestorAttribute,
       @"highestEditableAncestor"},
      {NSAccessibilityIndexAttribute, @"index"},
      {NSAccessibilityInsertionPointLineNumberAttribute,
       @"insertionPointLineNumber"},
      {NSAccessibilityInvalidAttribute, @"invalid"},
      {NSAccessibilityIsMultiSelectableAttribute, @"isMultiSelectable"},
      {NSAccessibilityLanguageAttribute, @"language"},
      {NSAccessibilityLinkedUIElementsAttribute, @"linkedUIElements"},
      {NSAccessibilityLoadingProgressAttribute, @"loadingProgress"},
      {NSAccessibilityMaxValueAttribute, @"maxValue"},
      {NSAccessibilityMinValueAttribute, @"minValue"},
      {NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters"},
      {NSAccessibilityOrientationAttribute, @"orientation"},
      {NSAccessibilityOwnsAttribute, @"owns"},
      {NSAccessibilityParentAttribute, @"parent"},
      {NSAccessibilityPlaceholderValueAttribute, @"placeholderValue"},
      {NSAccessibilityPositionAttribute, @"position"},
      {NSAccessibilityRequiredAttributeChrome, @"required"},
      {NSAccessibilityRoleAttribute, @"role"},
      {NSAccessibilityRoleDescriptionAttribute, @"roleDescription"},
      {NSAccessibilityRowHeaderUIElementsAttribute, @"rowHeaders"},
      {NSAccessibilityRowIndexRangeAttribute, @"rowIndexRange"},
      {NSAccessibilityRowsAttribute, @"rows"},
      // TODO(aboxhall): expose
      // NSAccessibilityServesAsTitleForUIElementsAttribute
      {NSAccessibilityStartTextMarkerAttribute, @"startTextMarker"},
      {NSAccessibilitySelectedAttribute, @"selected"},
      {NSAccessibilitySelectedChildrenAttribute, @"selectedChildren"},
      {NSAccessibilitySelectedTextAttribute, @"selectedText"},
      {NSAccessibilitySelectedTextRangeAttribute, @"selectedTextRange"},
      {NSAccessibilitySelectedTextMarkerRangeAttribute,
       @"selectedTextMarkerRange"},
      {NSAccessibilitySizeAttribute, @"size"},
      {NSAccessibilitySortDirectionAttribute, @"sortDirection"},
      {NSAccessibilitySubroleAttribute, @"subrole"},
      {NSAccessibilityTabsAttribute, @"tabs"},
      {NSAccessibilityTitleAttribute, @"title"},
      {NSAccessibilityTitleUIElementAttribute, @"titleUIElement"},
      {NSAccessibilityTopLevelUIElementAttribute, @"window"},
      {NSAccessibilityURLAttribute, @"url"},
      {NSAccessibilityValueAttribute, @"value"},
      {NSAccessibilityValueAutofillAvailableAttribute,
       @"valueAutofillAvailable"},
      // Not currently supported by Chrome -- information not stored:
      // {NSAccessibilityValueAutofilledAttribute, @"valueAutofilled"},
      // Not currently supported by Chrome -- mismatch of types supported:
      // {NSAccessibilityValueAutofillTypeAttribute, @"valueAutofillType"},
      {NSAccessibilityValueDescriptionAttribute, @"valueDescription"},
      {NSAccessibilityVisibleCharacterRangeAttribute, @"visibleCharacterRange"},
      {NSAccessibilityVisibleCellsAttribute, @"visibleCells"},
      {NSAccessibilityVisibleChildrenAttribute, @"visibleChildren"},
      {NSAccessibilityVisibleColumnsAttribute, @"visibleColumns"},
      {NSAccessibilityVisibleRowsAttribute, @"visibleRows"},
      {NSAccessibilityVisitedAttribute, @"visited"},
      {NSAccessibilityWindowAttribute, @"window"},
      {@"AXLoaded", @"loaded"},
  };

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  const size_t numAttributes = sizeof(attributeToMethodNameContainer) /
                               sizeof(attributeToMethodNameContainer[0]);
  for (size_t i = 0; i < numAttributes; ++i) {
    [dict setObject:attributeToMethodNameContainer[i].methodName
             forKey:attributeToMethodNameContainer[i].attribute];
  }
  attributeToMethodNameMap = dict;
  dict = nil;
}

- (instancetype)initWithObject:(BrowserAccessibility*)accessibility {
  if ((self = [super init]))
    owner_ = accessibility;
  return self;
}

- (void)detach {
  if (!owner_)
    return;
  NSAccessibilityPostNotification(
      self, NSAccessibilityUIElementDestroyedNotification);
  owner_ = nullptr;
}

- (NSString*)accessKey {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kAccessKey);
}

- (NSNumber*)ariaAtomic {
  if (![self instanceActive])
    return nil;
  bool boolValue =
      owner_->GetBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic);
  return [NSNumber numberWithBool:boolValue];
}

- (NSNumber*)ariaBusy {
  if (![self instanceActive])
    return nil;
  return [NSNumber
      numberWithBool:owner_->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy)];
}

- (NSNumber*)ariaColumnCount {
  if (![self instanceActive])
    return nil;
  base::Optional<int> aria_col_count = owner_->node()->GetTableAriaColCount();
  if (!aria_col_count)
    return nil;
  return [NSNumber numberWithInt:*aria_col_count];
}

- (NSNumber*)ariaColumnIndex {
  if (![self instanceActive])
    return nil;
  base::Optional<int> ariaColIndex = owner_->node()->GetTableCellAriaColIndex();
  if (!ariaColIndex)
    return nil;
  return [NSNumber numberWithInt:*ariaColIndex];
}

- (NSString*)ariaLive {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kLiveStatus);
}

- (NSNumber*)ariaPosInSet {
  if (![self instanceActive])
    return nil;
  base::Optional<int> posInSet = owner_->node()->GetPosInSet();
  if (!posInSet)
    return nil;
  return [NSNumber numberWithInt:*posInSet];
}

- (NSString*)ariaRelevant {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kLiveRelevant);
}

- (NSNumber*)ariaRowCount {
  if (![self instanceActive])
    return nil;
  base::Optional<int> ariaRowCount = owner_->node()->GetTableAriaRowCount();
  if (!ariaRowCount)
    return nil;
  return [NSNumber numberWithInt:*ariaRowCount];
}

- (NSNumber*)ariaRowIndex {
  if (![self instanceActive])
    return nil;
  base::Optional<int> ariaRowIndex = owner_->node()->GetTableCellAriaRowIndex();
  if (!ariaRowIndex)
    return nil;
  return [NSNumber numberWithInt:*ariaRowIndex];
}

- (NSNumber*)ariaSetSize {
  if (![self instanceActive])
    return nil;
  base::Optional<int> setSize = owner_->node()->GetSetSize();
  if (!setSize)
    return nil;
  return [NSNumber numberWithInt:*setSize];
}

- (NSString*)autocompleteValue {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kAutoComplete);
}

- (id)blockQuoteLevel {
  if (![self instanceActive])
    return nil;
  // TODO(accessibility) This is for the number of ancestors that are a
  // <blockquote>, including self, useful for tracking replies to replies etc.
  // in an email.
  int level = 0;
  BrowserAccessibility* ancestor = owner_;
  while (ancestor) {
    if (ancestor->GetRole() == ax::mojom::Role::kBlockquote)
      ++level;
    ancestor = ancestor->PlatformGetParent();
  }
  return [NSNumber numberWithInt:level];
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (![self instanceActive])
    return nil;
  if ([self internalRole] == ax::mojom::Role::kLayoutTableColumn)
    return nil;
  if (!children_) {
    uint32_t childCount = owner_->PlatformChildCount();
    children_.reset([[NSMutableArray alloc] initWithCapacity:childCount]);
    for (auto it = owner_->PlatformChildrenBegin();
         it != owner_->PlatformChildrenEnd(); ++it) {
      BrowserAccessibilityCocoa* child = ToBrowserAccessibilityCocoa(it.get());
      if ([child isIgnored])
        [children_ addObjectsFromArray:[child children]];
      else
        [children_ addObject:child];
    }

    // Also, add indirect children (if any).
    const std::vector<int32_t>& indirectChildIds = owner_->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int32_t child_id = indirectChildIds[i];
      BrowserAccessibility* child = owner_->manager()->GetFromID(child_id);

      // This only became necessary as a result of crbug.com/93095. It should be
      // a DCHECK in the future.
      if (child) {
        BrowserAccessibilityCocoa* child_cocoa =
            ToBrowserAccessibilityCocoa(child);
        [children_ addObject:child_cocoa];
      }
    }
  }
  return children_;
}

- (void)childrenChanged {
  if (![self instanceActive])
    return;
  if (![self isIgnored]) {
    children_.reset();
  } else {
    auto* parent = owner_->PlatformGetParent();
    if (parent)
      [ToBrowserAccessibilityCocoa(parent) childrenChanged];
  }
}

- (NSArray*)columnHeaders {
  if (![self instanceActive])
    return nil;

  bool is_cell_or_table_header = ui::IsCellOrTableHeader(owner_->GetRole());
  bool is_table_like = ui::IsTableLike(owner_->GetRole());
  if (!is_table_like && !is_cell_or_table_header)
    return nil;
  BrowserAccessibility* table = [self containingTable];
  if (!table)
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  if (is_table_like) {
    // If this is a table, return all column headers.
    std::set<int32_t> headerIds;
    for (int i = 0; i < *owner_->GetTableColCount(); i++) {
      std::vector<int32_t> colHeaderIds = table->GetColHeaderNodeIds(i);
      std::copy(colHeaderIds.begin(), colHeaderIds.end(),
                std::inserter(headerIds, headerIds.end()));
    }
    for (int32_t id : headerIds) {
      BrowserAccessibility* cell = owner_->manager()->GetFromID(id);
      if (cell)
        [ret addObject:ToBrowserAccessibilityCocoa(cell)];
    }
  } else {
    // Otherwise this is a cell, return the column headers for this cell.
    base::Optional<int> column = owner_->GetTableCellColIndex();
    if (!column)
      return nil;

    std::vector<int32_t> colHeaderIds = table->GetColHeaderNodeIds(*column);
    for (int32_t id : colHeaderIds) {
      BrowserAccessibility* cell = owner_->manager()->GetFromID(id);
      if (cell)
        [ret addObject:ToBrowserAccessibilityCocoa(cell)];
    }
  }

  return [ret count] ? ret : nil;
}

- (NSValue*)columnIndexRange {
  if (![self instanceActive])
    return nil;

  base::Optional<int> column = owner_->node()->GetTableCellColIndex();
  base::Optional<int> colspan = owner_->node()->GetTableCellColSpan();
  if (column && colspan)
    return [NSValue valueWithRange:NSMakeRange(*column, *colspan)];
  return nil;
}

- (NSArray*)columns {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (BrowserAccessibilityCocoa* child in [self children]) {
    if ([[child role] isEqualToString:NSAccessibilityColumnRole])
      [ret addObject:child];
  }
  return ret;
}

- (BrowserAccessibility*)containingTable {
  BrowserAccessibility* table = owner_;
  while (table && !ui::IsTableLike(table->GetRole())) {
    table = table->PlatformGetParent();
  }
  return table;
}

- (NSString*)descriptionForAccessibility {
  if (![self instanceActive])
    return nil;

  // Mac OS X wants static text exposed in AXValue.
  if (ui::IsNameExposedInAXValueForRole([self internalRole]))
    return @"";

  // If we're exposing the title in TitleUIElement, don't also redundantly
  // expose it in AXDescription.
  if ([self shouldExposeTitleUIElement])
    return @"";

  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      owner_->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  std::string name =
      owner_->GetStringAttribute(ax::mojom::StringAttribute::kName);

  auto status = owner_->GetData().GetImageAnnotationStatus();
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed: {
      base::string16 status_string =
          owner_->GetLocalizedStringForImageAnnotationStatus(status);
      AppendTextToString(base::UTF16ToUTF8(status_string), &name);
      break;
    }

    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      AppendTextToString(owner_->GetStringAttribute(
                             ax::mojom::StringAttribute::kImageAnnotation),
                         &name);
      break;

    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;
  }

  if (!name.empty()) {
    // On Mac OS X, the accessible name of an object is exposed as its
    // title if it comes from visible text, and as its description
    // otherwise, but never both.

    // Group, radiogroup etc.
    if ([self shouldExposeNameInDescription]) {
      return base::SysUTF8ToNSString(name);
    } else if (nameFrom == ax::mojom::NameFrom::kCaption ||
               nameFrom == ax::mojom::NameFrom::kContents ||
               nameFrom == ax::mojom::NameFrom::kRelatedElement ||
               nameFrom == ax::mojom::NameFrom::kValue) {
      return @"";
    } else {
      return base::SysUTF8ToNSString(name);
    }
  }

  // Given an image where there's no other title, return the base part
  // of the filename as the description.
  if ([[self role] isEqualToString:NSAccessibilityImageRole]) {
    if ([self titleUIElement])
      return @"";

    std::string url;
    if (owner_->GetStringAttribute(ax::mojom::StringAttribute::kUrl, &url)) {
      // Given a url like http://foo.com/bar/baz.png, just return the
      // base name, e.g., "baz.png".
      size_t leftIndex = url.rfind('/');
      std::string basename =
          leftIndex != std::string::npos ? url.substr(leftIndex) : url;
      return base::SysUTF8ToNSString(basename);
    }
  }

  // If it's focusable but didn't have any other name or value, compute a name
  // from its descendants. Note that this is a workaround because VoiceOver
  // does not always present focus changes if the new focus lacks a name.
  base::string16 value = owner_->GetValue();
  if (owner_->HasState(ax::mojom::State::kFocusable) &&
      !ui::IsControl(owner_->GetRole()) && value.empty() &&
      [self internalRole] != ax::mojom::Role::kDateTime &&
      [self internalRole] != ax::mojom::Role::kWebArea &&
      [self internalRole] != ax::mojom::Role::kRootWebArea) {
    return base::SysUTF8ToNSString(
        owner_->ComputeAccessibleNameFromDescendants());
  }

  return @"";
}

- (NSNumber*)disclosing {
  if (![self instanceActive])
    return nil;
  if ([self internalRole] == ax::mojom::Role::kTreeItem) {
    return
        [NSNumber numberWithBool:GetState(owner_, ax::mojom::State::kExpanded)];
  } else {
    return nil;
  }
}

- (id)disclosedByRow {
  if (![self instanceActive])
    return nil;

  // The row that contains this row.
  // It should be the same as the first parent that is a treeitem.
  return nil;
}

- (NSNumber*)disclosureLevel {
  if (![self instanceActive])
    return nil;
  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kRow || role == ax::mojom::Role::kTreeItem) {
    int level =
        owner_->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    // Mac disclosureLevel is 0-based, but web levels are 1-based.
    if (level > 0)
      level--;
    return [NSNumber numberWithInt:level];
  } else {
    return nil;
  }
}

- (id)disclosedRows {
  if (![self instanceActive])
    return nil;

  // The rows that are considered inside this row.
  return nil;
}

- (NSString*)dropEffects {
  if (![self instanceActive])
    return nil;

  std::string dropEffects;
  if (owner_->GetHtmlAttribute("aria-dropeffect", &dropEffects))
    return base::SysUTF8ToNSString(dropEffects);

  return nil;
}

- (NSArray*)domClassList {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  std::string classes;
  if (owner_->GetHtmlAttribute("class", &classes)) {
    std::vector<std::string> split_classes = base::SplitString(
        classes, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& className : split_classes)
      [ret addObject:(base::SysUTF8ToNSString(className))];
  }
  return ret;
}

- (NSString*)domIdentifier {
  if (![self instanceActive])
    return nil;

  std::string id;
  if (owner_->GetHtmlAttribute("id", &id))
    return base::SysUTF8ToNSString(id);

  return @"";
}

- (id)editableAncestor {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityCocoa* editableRoot = self;
  while (![editableRoot owner]->GetBoolAttribute(
      ax::mojom::BoolAttribute::kEditableRoot)) {
    BrowserAccessibilityCocoa* parent = [editableRoot parent];
    if (!parent || ![parent isKindOfClass:[self class]] ||
        ![parent instanceActive]) {
      return nil;
    }
    editableRoot = parent;
  }
  return editableRoot;
}

- (NSNumber*)elementBusy {
  if (![self instanceActive])
    return nil;
  return [NSNumber numberWithBool:owner_->GetData().GetBoolAttribute(
                                      ax::mojom::BoolAttribute::kBusy)];
}

- (NSNumber*)enabled {
  if (![self instanceActive])
    return nil;
  return [NSNumber numberWithBool:owner_->GetData().GetRestriction() !=
                                  ax::mojom::Restriction::kDisabled];
}

// Returns a text marker that points to the last character in the document that
// can be selected with VoiceOver.
- (id)endTextMarker {
  const BrowserAccessibility* root = owner_->manager()->GetRoot();
  if (!root)
    return nil;

  BrowserAccessibilityPositionInstance position = root->CreatePositionAt(0);
  return CreateTextMarker(position->CreatePositionAtEndOfAnchor());
}

- (NSNumber*)expanded {
  if (![self instanceActive])
    return nil;
  return
      [NSNumber numberWithBool:GetState(owner_, ax::mojom::State::kExpanded)];
}

- (id)focusableAncestor {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityCocoa* focusableRoot = self;
  while (![focusableRoot owner]->HasState(ax::mojom::State::kFocusable)) {
    BrowserAccessibilityCocoa* parent = [focusableRoot parent];
    if (!parent || ![parent isKindOfClass:[self class]] ||
        ![parent instanceActive]) {
      return nil;
    }
    focusableRoot = parent;
  }
  return focusableRoot;
}

- (NSNumber*)focused {
  if (![self instanceActive])
    return nil;
  BrowserAccessibilityManager* manager = owner_->manager();
  NSNumber* ret = [NSNumber numberWithBool:manager->GetFocus() == owner_];
  return ret;
}

- (NSNumber*)grabbed {
  if (![self instanceActive])
    return nil;
  std::string grabbed;
  if (owner_->GetHtmlAttribute("aria-grabbed", &grabbed) && grabbed == "true")
    return [NSNumber numberWithBool:YES];

  return [NSNumber numberWithBool:NO];
}

- (NSNumber*)hasPopup {
  if (![self instanceActive])
    return nil;
  return @(owner_->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup));
}

- (NSString*)hasPopupValue {
  if (![self instanceActive])
    return nil;
  int hasPopup = owner_->GetIntAttribute(ax::mojom::IntAttribute::kHasPopup);
  switch (static_cast<ax::mojom::HasPopup>(hasPopup)) {
    case ax::mojom::HasPopup::kFalse:
      return @"false";
    case ax::mojom::HasPopup::kTrue:
      return @"true";
    case ax::mojom::HasPopup::kMenu:
      return @"menu";
    case ax::mojom::HasPopup::kListbox:
      return @"listbox";
    case ax::mojom::HasPopup::kTree:
      return @"tree";
    case ax::mojom::HasPopup::kGrid:
      return @"grid";
    case ax::mojom::HasPopup::kDialog:
      return @"dialog";
  }
}

- (id)header {
  if (![self instanceActive])
    return nil;
  int headerElementId = -1;
  if (ui::IsTableLike(owner_->GetRole())) {
    // The table header container is always the last child of the table,
    // if it exists. The table header container is a special node in the
    // accessibility tree only used on macOS. It has all of the table
    // headers as its children, even though those cells are also children
    // of rows in the table. Internally this is implemented using
    // AXTableInfo and indirect_child_ids.
    uint32_t childCount = owner_->PlatformChildCount();
    if (childCount > 0) {
      BrowserAccessibility* tableHeader = owner_->PlatformGetLastChild();
      if (tableHeader->GetRole() == ax::mojom::Role::kTableHeaderContainer)
        return ToBrowserAccessibilityCocoa(tableHeader);
    }
  } else if ([self internalRole] == ax::mojom::Role::kColumn) {
    owner_->GetIntAttribute(ax::mojom::IntAttribute::kTableColumnHeaderId,
                            &headerElementId);
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    owner_->GetIntAttribute(ax::mojom::IntAttribute::kTableRowHeaderId,
                            &headerElementId);
  }

  if (headerElementId > 0) {
    BrowserAccessibility* headerObject =
        owner_->manager()->GetFromID(headerElementId);
    if (headerObject)
      return ToBrowserAccessibilityCocoa(headerObject);
  }
  return nil;
}

- (NSString*)help {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kDescription);
}

- (id)highestEditableAncestor {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityCocoa* highestEditableAncestor = [self editableAncestor];
  while (highestEditableAncestor) {
    BrowserAccessibilityCocoa* ancestorParent =
        [highestEditableAncestor parent];
    if (!ancestorParent || ![ancestorParent isKindOfClass:[self class]]) {
      break;
    }
    BrowserAccessibilityCocoa* higherAncestor =
        [ancestorParent editableAncestor];
    if (!higherAncestor)
      break;
    highestEditableAncestor = higherAncestor;
  }
  return highestEditableAncestor;
}

- (NSNumber*)index {
  if (![self instanceActive])
    return nil;
  if ([self internalRole] == ax::mojom::Role::kColumn) {
    DCHECK(owner_->node());
    return @(*owner_->node()->GetTableColColIndex());
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    DCHECK(owner_->node());
    return @(*owner_->node()->GetTableRowRowIndex());
  }

  return nil;
}

- (NSNumber*)insertionPointLineNumber {
  if (![self instanceActive])
    return nil;
  if (!owner_->HasVisibleCaretOrSelection())
    return nil;

  const AXPlatformRange range = GetSelectedRange(*owner_);
  // If the selection is not collapsed, then there is no visible caret.
  if (!range.IsCollapsed())
    return nil;

  const BrowserAccessibilityPositionInstance caretPosition =
      range.focus()->LowestCommonAncestor(*owner_->CreatePositionAt(0));
  DCHECK(!caretPosition->IsNullPosition())
      << "Calling HasVisibleCaretOrSelection() should have ensured that there "
         "is a valid selection focus inside the current object.";
  const std::vector<int> lineBreaks = owner_->GetLineStartOffsets();
  auto iterator =
      std::upper_bound(lineBreaks.begin(), lineBreaks.end(),
                       caretPosition->AsTextPosition()->text_offset());
  return @(std::distance(lineBreaks.begin(), iterator));
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  if (![self instanceActive])
    return YES;
  return [[self role] isEqualToString:NSAccessibilityUnknownRole] ||
         owner_->HasState(ax::mojom::State::kInvisible);
}

- (NSString*)invalid {
  if (![self instanceActive])
    return nil;
  int invalidState;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kInvalidState,
                               &invalidState))
    return @"false";

  switch (static_cast<ax::mojom::InvalidState>(invalidState)) {
    case ax::mojom::InvalidState::kFalse:
      return @"false";
    case ax::mojom::InvalidState::kTrue:
      return @"true";
    case ax::mojom::InvalidState::kOther: {
      std::string ariaInvalidValue;
      if (owner_->GetStringAttribute(
              ax::mojom::StringAttribute::kAriaInvalidValue, &ariaInvalidValue))
        return base::SysUTF8ToNSString(ariaInvalidValue);
      // Return @"true" since we cannot be more specific about the value.
      return @"true";
    }
    default:
      NOTREACHED();
  }

  return @"false";
}

- (NSNumber*)isMultiSelectable {
  if (![self instanceActive])
    return nil;
  return [NSNumber
      numberWithBool:GetState(owner_, ax::mojom::State::kMultiselectable)];
}

- (NSString*)placeholderValue {
  if (![self instanceActive])
    return nil;
  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      owner_->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  if (nameFrom == ax::mojom::NameFrom::kPlaceholder) {
    return NSStringForStringAttribute(owner_,
                                      ax::mojom::StringAttribute::kName);
  }

  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kPlaceholder);
}

- (NSString*)language {
  if (![self instanceActive])
    return nil;
  ui::AXNode* node = owner_->node();
  DCHECK(node);
  return base::SysUTF8ToNSString(node->GetLanguage());
}

// private
- (void)addLinkedUIElementsFromAttribute:(ax::mojom::IntListAttribute)attribute
                                   addTo:(NSMutableArray*)outArray {
  const std::vector<int32_t>& attributeValues =
      owner_->GetIntListAttribute(attribute);
  for (size_t i = 0; i < attributeValues.size(); ++i) {
    BrowserAccessibility* element =
        owner_->manager()->GetFromID(attributeValues[i]);
    if (element)
      [outArray addObject:ToBrowserAccessibilityCocoa(element)];
  }
}

// private
- (NSArray*)linkedUIElements {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [self
      addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::kControlsIds
                                 addTo:ret];
  [self addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::kFlowtoIds
                                   addTo:ret];

  int target_id;
  if (owner_->GetIntAttribute(ax::mojom::IntAttribute::kInPageLinkTargetId,
                              &target_id)) {
    BrowserAccessibility* target =
        owner_->manager()->GetFromID(static_cast<int32_t>(target_id));
    if (target)
      [ret addObject:ToBrowserAccessibilityCocoa(target)];
  }

  [self addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::
                                             kRadioGroupIds
                                   addTo:ret];
  return ret;
}

- (NSNumber*)loaded {
  if (![self instanceActive])
    return nil;
  return [NSNumber numberWithBool:YES];
}

- (NSNumber*)loadingProgress {
  if (![self instanceActive])
    return nil;
  BrowserAccessibilityManager* manager = owner_->manager();
  float floatValue = manager->GetTreeData().loading_progress;
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)maxValue {
  if (![self instanceActive])
    return nil;
  float floatValue =
      owner_->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)minValue {
  if (![self instanceActive])
    return nil;
  float floatValue =
      owner_->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSString*)orientation {
  if (![self instanceActive])
    return nil;
  if (GetState(owner_, ax::mojom::State::kVertical))
    return NSAccessibilityVerticalOrientationValue;
  else if (GetState(owner_, ax::mojom::State::kHorizontal))
    return NSAccessibilityHorizontalOrientationValue;

  return @"";
}

- (id)owns {
  if (![self instanceActive])
    return nil;

  //
  // If the active descendant points to an element in a container with
  // selectable children, add the "owns" relationship to point to that
  // container. That's the only way activeDescendant is actually
  // supported with VoiceOver.
  //

  int activeDescendantId;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                               &activeDescendantId))
    return nil;

  BrowserAccessibilityManager* manager = owner_->manager();
  BrowserAccessibility* activeDescendant =
      manager->GetFromID(activeDescendantId);
  if (!activeDescendant)
    return nil;

  BrowserAccessibility* container = activeDescendant->PlatformGetParent();
  while (container &&
         !ui::IsContainerWithSelectableChildren(container->GetRole()))
    container = container->PlatformGetParent();
  if (!container)
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [ret addObject:ToBrowserAccessibilityCocoa(container)];
  return ret;
}

- (NSNumber*)numberOfCharacters {
  if (![self instanceActive])
    return nil;
  base::string16 value = owner_->GetValue();
  return [NSNumber numberWithUnsignedInt:value.size()];
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  if (![self instanceActive])
    return NSMakePoint(0, 0);
  gfx::Rect bounds = owner_->GetClippedRootFrameBoundsRect();
  return NSMakePoint(bounds.x(), bounds.y());
}

- (id)parent {
  if (![self instanceActive])
    return nil;
  // A nil parent means we're the root.
  if (owner_->PlatformGetParent()) {
    return NSAccessibilityUnignoredAncestor(
        ToBrowserAccessibilityCocoa(owner_->PlatformGetParent()));
  } else {
    // Hook back up to RenderWidgetHostViewCocoa.
    BrowserAccessibilityManagerMac* manager =
        owner_->manager()->GetRootManager()->ToBrowserAccessibilityManagerMac();
    if (manager)
      return manager->GetParentView();
    return nil;
  }
}

- (NSValue*)position {
  if (![self instanceActive])
    return nil;
  NSPoint origin = [self origin];
  NSSize size = [[self size] sizeValue];
  NSPoint pointInScreen =
      [self rectInScreen:gfx::Rect(gfx::Point(origin), gfx::Size(size))].origin;
  return [NSValue valueWithPoint:pointInScreen];
}

- (NSNumber*)required {
  if (![self instanceActive])
    return nil;
  return
      [NSNumber numberWithBool:GetState(owner_, ax::mojom::State::kRequired)];
}

// Returns an enum indicating the role from owner_.
// internal
- (ax::mojom::Role)internalRole {
  if ([self instanceActive])
    return static_cast<ax::mojom::Role>(owner_->GetRole());
  return ax::mojom::Role::kNone;
}

- (BOOL)shouldExposeNameInDescription {
  // Image annotations are not visible text, so they should be exposed
  // as a description and not a title.
  switch (owner_->GetData().GetImageAnnotationStatus()) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return true;

    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;
  }

  // VoiceOver will not read the label of a fieldset or radiogroup unless it is
  // exposed in the description instead of the title.
  switch (owner_->GetRole()) {
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kRadioGroup:
      return true;
    default:
      return false;
  }
}

// Returns true if this object should expose its accessible name using
// AXTitleUIElement rather than AXTitle or AXDescription. We only do
// this if it's a control, if there's a single label, and the label has
// nonempty text.
// internal
- (BOOL)shouldExposeTitleUIElement {
  // VoiceOver ignores TitleUIElement if the element isn't a control.
  if (!ui::IsControl(owner_->GetRole()))
    return false;

  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      owner_->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  if (nameFrom != ax::mojom::NameFrom::kCaption &&
      nameFrom != ax::mojom::NameFrom::kRelatedElement)
    return false;

  std::vector<int32_t> labelledby_ids =
      owner_->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds);
  if (labelledby_ids.size() != 1)
    return false;

  BrowserAccessibility* label = owner_->manager()->GetFromID(labelledby_ids[0]);
  if (!label)
    return false;

  std::string labelName =
      label->GetStringAttribute(ax::mojom::StringAttribute::kName);
  return !labelName.empty();
}

// internal
- (content::BrowserAccessibilityDelegate*)delegate {
  return [self instanceActive] ? owner_->manager()->delegate() : nil;
}

- (content::BrowserAccessibility*)owner {
  return owner_;
}

// Assumes that there is at most one insertion, deletion or replacement at once.
// TODO(nektar): Merge this method with
// |BrowserAccessibilityAndroid::CommonEndLengths|.
- (content::AXTextEdit)computeTextEdit {
  // Starting from macOS 10.11, if the user has edited some text we need to
  // dispatch the actual text that changed on the value changed notification.
  // We run this code on all macOS versions to get the highest test coverage.
  base::string16 oldValue = oldValue_;
  base::string16 newValue = owner_->GetValue();
  oldValue_ = newValue;
  if (oldValue.empty() && newValue.empty())
    return content::AXTextEdit();

  size_t i;
  size_t j;
  // Sometimes Blink doesn't use the same UTF16 characters to represent
  // whitespace.
  for (i = 0;
       i < oldValue.length() && i < newValue.length() &&
       (oldValue[i] == newValue[i] || (base::IsUnicodeWhitespace(oldValue[i]) &&
                                       base::IsUnicodeWhitespace(newValue[i])));
       ++i) {
  }
  for (j = 0;
       (i + j) < oldValue.length() && (i + j) < newValue.length() &&
       (oldValue[oldValue.length() - j - 1] ==
            newValue[newValue.length() - j - 1] ||
        (base::IsUnicodeWhitespace(oldValue[oldValue.length() - j - 1]) &&
         base::IsUnicodeWhitespace(newValue[newValue.length() - j - 1])));
       ++j) {
  }
  DCHECK_LE(i + j, oldValue.length());
  DCHECK_LE(i + j, newValue.length());

  base::string16 deletedText = oldValue.substr(i, oldValue.length() - i - j);
  base::string16 insertedText = newValue.substr(i, newValue.length() - i - j);
  return content::AXTextEdit(insertedText, deletedText);
}

- (BOOL)instanceActive {
  return owner_ && owner_->instance_active();
}

// internal
- (NSRect)rectInScreen:(gfx::Rect)rect {
  if (![self instanceActive])
    return NSZeroRect;

  // Get the delegate for the topmost BrowserAccessibilityManager, because
  // that's the only one that can convert points to their origin in the screen.
  BrowserAccessibilityDelegate* delegate =
      owner_->manager()->GetDelegateFromRootManager();
  if (delegate) {
    return gfx::ScreenRectToNSRect(
        rect + delegate->AccessibilityGetViewBounds().OffsetFromOrigin());
  } else {
    return NSZeroRect;
  }
}

// Returns a string indicating the NSAccessibility role of this object.
- (NSString*)role {
  if (![self instanceActive]) {
    TRACE_EVENT0("accessibility", "BrowserAccessibilityCocoa::role nil");
    return nil;
  }

  NSString* cocoa_role = nil;
  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kCanvas &&
      owner_->GetBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback)) {
    cocoa_role = NSAccessibilityGroupRole;
  } else if ((owner_->IsPlainTextField() &&
              owner_->HasState(ax::mojom::State::kMultiline)) ||
             owner_->IsRichTextField()) {
    cocoa_role = NSAccessibilityTextAreaRole;
  } else if (role == ax::mojom::Role::kImage &&
             owner_->HasExplicitlyEmptyName()) {
    cocoa_role = NSAccessibilityUnknownRole;
  } else if (owner_->IsWebAreaForPresentationalIframe()) {
    cocoa_role = NSAccessibilityGroupRole;
  } else {
    cocoa_role = [AXPlatformNodeCocoa nativeRoleFromAXRole:role];
  }

  TRACE_EVENT1("accessibility", "BrowserAccessibilityCocoa::role",
               "role=", base::SysNSStringToUTF8(cocoa_role));
  return cocoa_role;
}

// Returns a string indicating the role description of this object.
- (NSString*)roleDescription {
  if (![self instanceActive])
    return nil;

  if (owner_->GetData().GetImageAnnotationStatus() ==
          ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation ||
      owner_->GetData().GetImageAnnotationStatus() ==
          ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation) {
    return base::SysUTF16ToNSString(
        owner_->GetLocalizedRoleDescriptionForUnlabeledImage());
  }

  if (owner_->HasStringAttribute(
          ax::mojom::StringAttribute::kRoleDescription)) {
    return NSStringForStringAttribute(
        owner_, ax::mojom::StringAttribute::kRoleDescription);
  }

  NSString* role = [self role];
  ContentClient* content_client = content::GetContentClient();

  // The following descriptions are specific to webkit.
  if ([role isEqualToString:@"AXWebArea"]) {
    return base::SysUTF16ToNSString(
        content_client->GetLocalizedString(IDS_AX_ROLE_WEB_AREA));
  }

  if ([role isEqualToString:@"NSAccessibilityLinkRole"]) {
    return base::SysUTF16ToNSString(
        content_client->GetLocalizedString(IDS_AX_ROLE_LINK));
  }

  if ([role isEqualToString:@"AXHeading"]) {
    return base::SysUTF16ToNSString(
        content_client->GetLocalizedString(IDS_AX_ROLE_HEADING));
  }

  if (([role isEqualToString:NSAccessibilityGroupRole] ||
       [role isEqualToString:NSAccessibilityRadioButtonRole]) &&
      !owner_->IsWebAreaForPresentationalIframe()) {
    std::string role_attribute;
    if (owner_->GetHtmlAttribute("role", &role_attribute)) {
      ax::mojom::Role internalRole = [self internalRole];
      if ((internalRole != ax::mojom::Role::kBlockquote &&
           internalRole != ax::mojom::Role::kCaption &&
           internalRole != ax::mojom::Role::kGroup &&
           internalRole != ax::mojom::Role::kListItem &&
           internalRole != ax::mojom::Role::kParagraph) ||
          internalRole == ax::mojom::Role::kTab) {
        // TODO(dtseng): This is not localized; see crbug/84814.
        return base::SysUTF8ToNSString(role_attribute);
      }
    }
  }

  switch ([self internalRole]) {
    case ax::mojom::Role::kArticle:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_ARTICLE));
    case ax::mojom::Role::kBanner:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_BANNER));
    case ax::mojom::Role::kCheckBox:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_CHECK_BOX));
    case ax::mojom::Role::kComment:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_COMMENT));
    case ax::mojom::Role::kCommentSection:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_COMMENT_SECTION));
    case ax::mojom::Role::kComplementary:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_COMPLEMENTARY));
    case ax::mojom::Role::kContentInfo:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INFO));
    case ax::mojom::Role::kDescriptionList:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_DESCRIPTION_LIST));
    case ax::mojom::Role::kDescriptionListDetail:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_DEFINITION));
    case ax::mojom::Role::kDescriptionListTerm:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_DESCRIPTION_TERM));
    case ax::mojom::Role::kDisclosureTriangle:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_DISCLOSURE_TRIANGLE));
    case ax::mojom::Role::kFigure:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_FIGURE));
    case ax::mojom::Role::kFooter:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_FOOTER));
    case ax::mojom::Role::kForm:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_FORM));
    case ax::mojom::Role::kHeader:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_BANNER));
    case ax::mojom::Role::kMain:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_MAIN_CONTENT));
    case ax::mojom::Role::kMark:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_MARK));
    case ax::mojom::Role::kMath:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_MATH));
    case ax::mojom::Role::kNavigation:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_NAVIGATIONAL_LINK));
    case ax::mojom::Role::kRegion:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_REGION));
    case ax::mojom::Role::kRevision:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_REVISION));
    case ax::mojom::Role::kSection:
      // A <section> element uses the 'region' ARIA role mapping.
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_REGION));
    case ax::mojom::Role::kSpinButton:
      // This control is similar to what VoiceOver calls a "stepper".
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_STEPPER));
    case ax::mojom::Role::kStatus:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_STATUS));
    case ax::mojom::Role::kSearchBox:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_SEARCH_BOX));
    case ax::mojom::Role::kSuggestion:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_SUGGESTION));
    case ax::mojom::Role::kSwitch:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_SWITCH));
    case ax::mojom::Role::kTerm:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_DESCRIPTION_TERM));
    case ax::mojom::Role::kToggleButton:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_TOGGLE_BUTTON));
    default:
      break;
  }

  return NSAccessibilityRoleDescription(role, nil);
}

- (NSArray*)rowHeaders {
  if (![self instanceActive])
    return nil;

  bool is_cell_or_table_header = ui::IsCellOrTableHeader(owner_->GetRole());
  bool is_table_like = ui::IsTableLike(owner_->GetRole());
  if (!is_table_like && !is_cell_or_table_header)
    return nil;
  BrowserAccessibility* table = [self containingTable];
  if (!table)
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  if (is_table_like) {
    // If this is a table, return all row headers.
    std::set<int32_t> headerIds;
    for (int i = 0; i < *table->GetTableRowCount(); i++) {
      std::vector<int32_t> rowHeaderIds = table->GetRowHeaderNodeIds(i);
      for (int32_t id : rowHeaderIds)
        headerIds.insert(id);
    }
    for (int32_t id : headerIds) {
      BrowserAccessibility* cell = owner_->manager()->GetFromID(id);
      if (cell)
        [ret addObject:ToBrowserAccessibilityCocoa(cell)];
    }
  } else {
    // Otherwise this is a cell, return the row headers for this cell.
    std::vector<int32_t> rowHeaderIds;
    owner_->node()->GetTableCellRowHeaderNodeIds(&rowHeaderIds);
    for (int32_t id : rowHeaderIds) {
      BrowserAccessibility* cell = owner_->manager()->GetFromID(id);
      if (cell)
        [ret addObject:ToBrowserAccessibilityCocoa(cell)];
    }
  }

  return [ret count] ? ret : nil;
}

- (NSValue*)rowIndexRange {
  if (![self instanceActive])
    return nil;

  base::Optional<int> row = owner_->node()->GetTableCellRowIndex();
  base::Optional<int> rowspan = owner_->node()->GetTableCellRowSpan();
  if (row && rowspan)
    return [NSValue valueWithRange:NSMakeRange(*row, *rowspan)];
  return nil;
}

- (NSArray*)rows {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  if (ui::IsTableLike(owner_->GetRole())) {
    for (BrowserAccessibilityCocoa* child in [self children]) {
      if ([[child role] isEqualToString:NSAccessibilityRowRole])
        [ret addObject:child];
    }
  } else if ([self internalRole] == ax::mojom::Role::kColumn) {
    const std::vector<int32_t>& indirectChildIds = owner_->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int id = indirectChildIds[i];
      BrowserAccessibility* rowElement = owner_->manager()->GetFromID(id);
      if (rowElement)
        [ret addObject:ToBrowserAccessibilityCocoa(rowElement)];
    }
  }

  return ret;
}

- (NSNumber*)selected {
  if (![self instanceActive])
    return nil;
  return [NSNumber numberWithBool:owner_->GetBoolAttribute(
                                      ax::mojom::BoolAttribute::kSelected)];
}

- (NSArray*)selectedChildren {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  BrowserAccessibilityManager* manager = owner_->manager();
  BrowserAccessibility* focusedChild = manager->GetFocus();
  if (focusedChild && !focusedChild->IsDescendantOf(owner_))
    focusedChild = nullptr;

  // If it's not multiselectable, try to skip iterating over the
  // children.
  if (!GetState(owner_, ax::mojom::State::kMultiselectable)) {
    // First try the focused child.
    if (focusedChild && focusedChild != owner_) {
      [ret addObject:ToBrowserAccessibilityCocoa(focusedChild)];
      return ret;
    }

    // Next try the active descendant.
    int activeDescendantId;
    if (owner_->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                &activeDescendantId)) {
      BrowserAccessibility* activeDescendant =
          manager->GetFromID(activeDescendantId);
      if (activeDescendant) {
        [ret addObject:ToBrowserAccessibilityCocoa(activeDescendant)];
        return ret;
      }
    }
  }

  // If it's multiselectable or if the previous attempts failed,
  // return any children with the "selected" state, which may
  // come from aria-selected.
  for (auto it = owner_->PlatformChildrenBegin();
       it != owner_->PlatformChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    if (child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
      [ret addObject:ToBrowserAccessibilityCocoa(child)];
  }

  // And if nothing's selected but one has focus, use the focused one.
  if ([ret count] == 0 && focusedChild && focusedChild != owner_) {
    [ret addObject:ToBrowserAccessibilityCocoa(focusedChild)];
  }

  return ret;
}

- (NSString*)selectedText {
  if (![self instanceActive])
    return nil;
  if (!owner_->HasVisibleCaretOrSelection())
    return nil;

  const AXPlatformRange range = GetSelectedRange(*owner_);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

// Returns range of text under the current object that is selected.
//
// Example, caret at offset 5:
// NSRange:  “pos=5 len=0”
- (NSValue*)selectedTextRange {
  if (![self instanceActive])
    return nil;
  if (!owner_->HasVisibleCaretOrSelection())
    return nil;

  const AXPlatformRange range = GetSelectedRange(*owner_).AsForwardRange();
  if (range.IsNull())
    return nil;

  const BrowserAccessibilityPositionInstance startPosition =
      range.anchor()->LowestCommonAncestor(*owner_->CreatePositionAt(0));
  DCHECK(!startPosition->IsNullPosition())
      << "Calling HasVisibleCaretOrSelection() should have ensured that there "
         "is a valid selection anchor inside the current object.";
  int selStart = startPosition->AsTextPosition()->text_offset();
  DCHECK_GE(selStart, 0);
  int selLength = range.GetText().length();
  return [NSValue valueWithRange:NSMakeRange(selStart, selLength)];
}

- (id)selectedTextMarkerRange {
  if (![self instanceActive])
    return nil;
  return CreateTextMarkerRange(GetSelectedRange(*owner_));
}

- (NSValue*)size {
  if (![self instanceActive])
    return nil;
  gfx::Rect bounds = owner_->GetClippedRootFrameBoundsRect();
  return [NSValue valueWithSize:NSMakeSize(bounds.width(), bounds.height())];
}

- (NSString*)sortDirection {
  if (![self instanceActive])
    return nil;
  int sortDirection;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                               &sortDirection))
    return nil;

  switch (static_cast<ax::mojom::SortDirection>(sortDirection)) {
    case ax::mojom::SortDirection::kUnsorted:
      return nil;
    case ax::mojom::SortDirection::kAscending:
      return NSAccessibilityAscendingSortDirectionValue;
    case ax::mojom::SortDirection::kDescending:
      return NSAccessibilityDescendingSortDirectionValue;
    case ax::mojom::SortDirection::kOther:
      return NSAccessibilityUnknownSortDirectionValue;
    default:
      NOTREACHED();
  }

  return nil;
}

// Returns a text marker that points to the first character in the document that
// can be selected with VoiceOver.
- (id)startTextMarker {
  const BrowserAccessibility* root = owner_->manager()->GetRoot();
  if (!root)
    return nil;

  BrowserAccessibilityPositionInstance position = root->CreatePositionAt(0);
  return CreateTextMarker(position->CreatePositionAtStartOfAnchor());
}

// Returns a subrole based upon the role.
- (NSString*)subrole {
  if (![self instanceActive])
    return nil;

  if (owner_->IsPlainTextField() &&
      GetState(owner_, ax::mojom::State::kProtected)) {
    return NSAccessibilitySecureTextFieldSubrole;
  }

  if ([self internalRole] == ax::mojom::Role::kDescriptionList)
    return NSAccessibilityDefinitionListSubrole;

  if ([self internalRole] == ax::mojom::Role::kList)
    return NSAccessibilityContentListSubrole;

  return [AXPlatformNodeCocoa nativeSubroleFromAXRole:[self internalRole]];
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  if (![self instanceActive])
    return nil;
  NSMutableArray* tabSubtree = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == ax::mojom::Role::kTab)
    [tabSubtree addObject:self];

  for (uint i = 0; i < [[self children] count]; ++i) {
    NSArray* tabChildren = [[[self children] objectAtIndex:i] tabs];
    if ([tabChildren count] > 0)
      [tabSubtree addObjectsFromArray:tabChildren];
  }

  return tabSubtree;
}

- (NSString*)title {
  if (![self instanceActive])
    return nil;
  // Mac OS X wants static text exposed in AXValue.
  if (ui::IsNameExposedInAXValueForRole([self internalRole]))
    return @"";

  if ([self shouldExposeNameInDescription])
    return @"";

  // If we're exposing the title in TitleUIElement, don't also redundantly
  // expose it in AXDescription.
  if ([self shouldExposeTitleUIElement])
    return @"";

  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      owner_->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));

  // On Mac OS X, cell titles are "" if it it came from content.
  NSString* role = [self role];
  if ([role isEqualToString:NSAccessibilityCellRole] &&
      nameFrom == ax::mojom::NameFrom::kContents)
    return @"";

  // On Mac OS X, the accessible name of an object is exposed as its
  // title if it comes from visible text, and as its description
  // otherwise, but never both.
  if (nameFrom == ax::mojom::NameFrom::kCaption ||
      nameFrom == ax::mojom::NameFrom::kContents ||
      nameFrom == ax::mojom::NameFrom::kRelatedElement ||
      nameFrom == ax::mojom::NameFrom::kValue) {
    return NSStringForStringAttribute(owner_,
                                      ax::mojom::StringAttribute::kName);
  }

  return @"";
}

- (id)titleUIElement {
  if (![self instanceActive])
    return nil;
  if (![self shouldExposeTitleUIElement])
    return nil;

  std::vector<int32_t> labelledby_ids =
      owner_->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds);
  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      owner_->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  if ((nameFrom == ax::mojom::NameFrom::kCaption ||
       nameFrom == ax::mojom::NameFrom::kRelatedElement) &&
      labelledby_ids.size() == 1) {
    BrowserAccessibility* titleElement =
        owner_->manager()->GetFromID(labelledby_ids[0]);
    if (titleElement)
      return ToBrowserAccessibilityCocoa(titleElement);
  }

  return nil;
}

- (NSURL*)url {
  if (![self instanceActive])
    return nil;
  std::string url;
  if ([[self role] isEqualToString:@"AXWebArea"])
    url = owner_->manager()->GetTreeData().url;
  else
    url = owner_->GetStringAttribute(ax::mojom::StringAttribute::kUrl);

  if (url.empty())
    return nil;

  return [NSURL URLWithString:(base::SysUTF8ToNSString(url))];
}

- (id)value {
  if (![self instanceActive])
    return nil;

  if (ui::IsNameExposedInAXValueForRole([self internalRole]))
    return NSStringForStringAttribute(owner_,
                                      ax::mojom::StringAttribute::kName);

  NSString* role = [self role];
  if ([role isEqualToString:@"AXHeading"]) {
    int level = 0;
    if (owner_->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                &level)) {
      return [NSNumber numberWithInt:level];
    }
  } else if ([role isEqualToString:NSAccessibilityButtonRole]) {
    // AXValue does not make sense for pure buttons.
    return @"";
  } else if (owner_->HasIntAttribute(ax::mojom::IntAttribute::kCheckedState) ||
             [role isEqualToString:NSAccessibilityRadioButtonRole]) {
    // On Mac, tabs are exposed as radio buttons, and are treated as checkable.
    int value;
    const auto checkedState = static_cast<ax::mojom::CheckedState>(
        owner_->GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
    switch (checkedState) {
      case ax::mojom::CheckedState::kTrue:
        value = 1;
        break;
      case ax::mojom::CheckedState::kMixed:
        value = 2;
        break;
      default:
        value = owner_->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)
                    ? 1
                    : 0;
        break;
    }
    return [NSNumber numberWithInt:value];
  } else if (IsRangeValueSupported(owner_->GetData())) {
    float floatValue;
    if (owner_->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                  &floatValue)) {
      return [NSNumber numberWithFloat:floatValue];
    }
  } else if ([role isEqualToString:NSAccessibilityColorWellRole]) {
    unsigned int color = static_cast<unsigned int>(
        owner_->GetIntAttribute(ax::mojom::IntAttribute::kColorValue));
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // This string matches the one returned by a native Mac color well.
    return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1", red / 255.,
                                      green / 255., blue / 255.];
  }

  return base::SysUTF16ToNSString(owner_->GetValue());
}

- (NSNumber*)valueAutofillAvailable {
  if (![self instanceActive])
    return nil;
  return owner_->HasState(ax::mojom::State::kAutofillAvailable) ? @YES : @NO;
}

// Not currently supported, as Chrome does not store whether an autofill
// occurred. We could have autofill fire an event, however, and set an
// "is_autofilled" flag until the next edit. - (NSNumber*)valueAutofilled {
//  return @NO;
// }

// Not currently supported, as Chrome's autofill types aren't like Safari's.
// - (NSString*)valueAutofillType {
//  return @"none";
//}

- (NSString*)valueDescription {
  if (![self instanceActive])
    return nil;
  if (owner_)
    return base::SysUTF16ToNSString(owner_->GetValue());
  return nil;
}

- (NSValue*)visibleCharacterRange {
  if (![self instanceActive])
    return nil;
  base::string16 value = owner_->GetValue();
  return [NSValue valueWithRange:NSMakeRange(0, value.size())];
}

- (NSArray*)visibleCells {
  if (![self instanceActive])
    return nil;

  std::vector<int32_t> unique_cell_ids;
  owner_->node()->GetTableUniqueCellIds(&unique_cell_ids);
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (size_t i = 0; i < unique_cell_ids.size(); ++i) {
    int id = unique_cell_ids[i];
    BrowserAccessibility* cell = owner_->manager()->GetFromID(id);
    if (cell)
      [ret addObject:ToBrowserAccessibilityCocoa(cell)];
  }
  return ret;
}

- (NSArray*)visibleChildren {
  if (![self instanceActive])
    return nil;
  return [self children];
}

- (NSArray*)visibleColumns {
  if (![self instanceActive])
    return nil;
  return [self columns];
}

- (NSArray*)visibleRows {
  if (![self instanceActive])
    return nil;
  return [self rows];
}

- (NSNumber*)visited {
  if (![self instanceActive])
    return nil;
  return [NSNumber numberWithBool:GetState(owner_, ax::mojom::State::kVisited)];
}

- (id)window {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityManagerMac* manager =
      owner_->manager()->GetRootManager()->ToBrowserAccessibilityManagerMac();
  if (!manager || !manager->GetParentView())
    return nil;

  return manager->GetWindow();
}

- (NSString*)methodNameForAttribute:(NSString*)attribute {
  return [attributeToMethodNameMap objectForKey:attribute];
}

- (void)swapChildren:(base::scoped_nsobject<NSMutableArray>*)other {
  children_.swap(*other);
}

- (NSString*)valueForRange:(NSRange)range {
  if (![self instanceActive])
    return nil;

  base::string16 value = owner_->GetValue();
  if (NSMaxRange(range) > value.length())
    return nil;

  return base::SysUTF16ToNSString(value.substr(range.location, range.length));
}

// Retrieves the text inside this object and decorates it with attributes
// indicating specific ranges of interest within the text, e.g. the location of
// misspellings.
- (NSAttributedString*)attributedValueForRange:(NSRange)range {
  if (![self instanceActive])
    return nil;

  base::string16 text = owner_->GetValue();
  if (owner_->IsTextOnlyObject() && text.empty())
    text = owner_->GetText();

  // We need to get the whole text because a spelling mistake might start or end
  // outside our range.
  NSString* value = base::SysUTF16ToNSString(text);
  NSMutableAttributedString* attributedValue =
      [[[NSMutableAttributedString alloc] initWithString:value] autorelease];

  if (!owner_->IsTextOnlyObject()) {
    AXPlatformRange ax_range(owner_->CreatePositionAt(0),
                             owner_->CreatePositionAt(int{text.length()}));
    AddMisspelledTextAttributes(ax_range, attributedValue);
  }

  return [attributedValue attributedSubstringFromRange:range];
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityAttributeValue",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return nil;

  SEL selector = NSSelectorFromString([self methodNameForAttribute:attribute]);
  if (selector)
    return [self performSelector:selector];

  return nil;
}

// Returns the accessibility value for the given attribute and parameter. If the
// value isn't supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (parameter && [parameter isKindOfClass:[NSNumber self]]) {
    TRACE_EVENT2(
        "accessibility",
        "BrowserAccessibilityCocoa::accessibilityAttributeValue:forParameter",
        "role=", ui::ToString([self internalRole]), "attribute=",
        base::SysNSStringToUTF8(attribute) +
            " parameter=" + base::SysNSStringToUTF8([parameter stringValue]));
  } else {
    TRACE_EVENT2(
        "accessibility",
        "BrowserAccessibilityCocoa::accessibilityAttributeValue:forParameter",
        "role=", ui::ToString([self internalRole]),
        "attribute=", base::SysNSStringToUTF8(attribute));
  }

  if (![self instanceActive])
    return nil;

  const std::vector<int> lineBreaks = owner_->GetLineStartOffsets();
  base::string16 value = owner_->GetValue();
  if (owner_->IsTextOnlyObject() && value.empty())
    value = owner_->GetText();
  int valueLength = static_cast<int>(value.size());

  if ([attribute isEqualToString:
                     NSAccessibilityStringForRangeParameterizedAttribute]) {
    return [self valueForRange:[(NSValue*)parameter rangeValue]];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityAttributedStringForRangeParameterizedAttribute]) {
    return [self attributedValueForRange:[(NSValue*)parameter rangeValue]];
  }

  if ([attribute
          isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute]) {
    int lineIndex = [(NSNumber*)parameter intValue];
    auto iterator =
        std::upper_bound(lineBreaks.begin(), lineBreaks.end(), lineIndex);
    return @(std::distance(lineBreaks.begin(), iterator));
  }

  if ([attribute
          isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute]) {
    int lineIndex = [(NSNumber*)parameter intValue];
    int lineCount = static_cast<int>(lineBreaks.size()) + 1;
    if (lineIndex < 0 || lineIndex >= lineCount)
      return nil;
    int start = (lineIndex > 0) ? lineBreaks[lineIndex - 1] : 0;
    int end =
        (lineIndex < (lineCount - 1)) ? lineBreaks[lineIndex] : valueLength;
    return [NSValue valueWithRange:NSMakeRange(start, end - start)];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
    if (!ui::IsTableLike([self internalRole]))
      return nil;
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;
    if (2 != [parameter count])
      return nil;
    NSArray* array = parameter;
    int column = [[array objectAtIndex:0] intValue];
    int row = [[array objectAtIndex:1] intValue];

    ui::AXNode* cellNode = owner_->node()->GetTableCellFromCoords(row, column);
    if (!cellNode)
      return nil;

    BrowserAccessibility* cell = owner_->manager()->GetFromID(cellNode->id());
    if (cell)
      return ToBrowserAccessibilityCocoa(cell);
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityUIElementForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (!position->IsNullPosition())
      return ToBrowserAccessibilityCocoa(position->GetAnchor());

    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance startPosition =
        owner_->CreatePositionAt(0);
    BrowserAccessibilityPositionInstance endPosition =
        startPosition->CreatePositionAtEndOfAnchor();
    AXPlatformRange range =
        AXPlatformRange(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityStringForTextMarkerRangeParameterizedAttribute])
    return GetTextForTextMarkerRange(parameter);

  if ([attribute
          isEqualToString:
              NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute])
    return GetAttributedTextForTextMarkerRange(parameter);

  if ([attribute
          isEqualToString:
              NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextCharacterPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousCharacterPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance endPosition =
        CreatePositionFromTextMarker(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startWordPosition =
        endPosition->CreatePreviousWordStartPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibilityPositionInstance endWordPosition =
        endPosition->CreatePreviousWordEndPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibilityPositionInstance startPosition =
        *startWordPosition <= *endWordPosition ? std::move(endWordPosition)
                                               : std::move(startWordPosition);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance startPosition =
        CreatePositionFromTextMarker(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance endWordPosition =
        startPosition->CreateNextWordEndPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibilityPositionInstance startWordPosition =
        startPosition->CreateNextWordStartPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibilityPositionInstance endPosition =
        *startWordPosition <= *endWordPosition ? std::move(startWordPosition)
                                               : std::move(endWordPosition);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextWordEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousWordStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute isEqualToString:
                     NSAccessibilityLineForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    int textOffset = position->AsTextPosition()->text_offset();
    const auto iterator =
        std::upper_bound(lineBreaks.begin(), lineBreaks.end(), textOffset);
    return @(std::distance(lineBreaks.begin(), iterator));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForLineParameterizedAttribute]) {
    int lineIndex = [(NSNumber*)parameter intValue];
    int lineCount = static_cast<int>(lineBreaks.size()) + 1;
    if (lineIndex < 0 || lineIndex >= lineCount)
      return nil;

    int lineStartOffset = (lineIndex > 0) ? lineBreaks[lineIndex - 1] : 0;
    BrowserAccessibilityPositionInstance lineStartPosition = CreateTextPosition(
        *owner_, lineStartOffset, ax::mojom::TextAffinity::kDownstream);
    if (lineStartPosition->IsNullPosition())
      return nil;

    // Make sure that the line start position is really at the start of the
    // current line.
    lineStartPosition = lineStartPosition->CreatePreviousLineStartPosition(
        ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibilityPositionInstance lineEndPosition =
        lineStartPosition->CreateNextLineEndPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    AXPlatformRange range(std::move(lineStartPosition),
                          std::move(lineEndPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance endPosition =
        CreatePositionFromTextMarker(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startLinePosition =
        endPosition->CreatePreviousLineStartPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibilityPositionInstance endLinePosition =
        endPosition->CreatePreviousLineEndPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibilityPositionInstance startPosition =
        *startLinePosition <= *endLinePosition ? std::move(endLinePosition)
                                               : std::move(startLinePosition);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance startPosition =
        CreatePositionFromTextMarker(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startLinePosition =
        startPosition->CreateNextLineStartPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibilityPositionInstance endLinePosition =
        startPosition->CreateNextLineEndPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibilityPositionInstance endPosition =
        *startLinePosition <= *endLinePosition ? std::move(startLinePosition)
                                               : std::move(endLinePosition);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextLineEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousLineStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startPosition =
        position->CreatePreviousParagraphStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibilityPositionInstance endPosition =
        position->CreateNextParagraphEndPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextParagraphEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousParagraphStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startPosition =
        position->CreatePreviousFormatStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibilityPositionInstance endPosition =
        position->CreateNextFormatEndPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute]) {
    NSString* text = GetTextForTextMarkerRange(parameter);
    return @([text length]);
  }

  if ([attribute isEqualToString:
                     NSAccessibilityTextMarkerIsValidParameterizedAttribute]) {
    return @(CreatePositionFromTextMarker(parameter)->IsNullPosition());
  }

  if ([attribute isEqualToString:
                     NSAccessibilityIndexForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return @(position->AsTextPosition()->text_offset());
  }

  if ([attribute isEqualToString:
                     NSAccessibilityTextMarkerForIndexParameterizedAttribute]) {
    int index = [static_cast<NSNumber*>(parameter) intValue];
    if (index < 0)
      return nil;

    const BrowserAccessibility* root = owner_->manager()->GetRoot();
    if (!root)
      return nil;

    return CreateTextMarker(root->CreatePositionAt(index));
  }

  if ([attribute isEqualToString:
                     NSAccessibilityBoundsForRangeParameterizedAttribute]) {
    if ([self internalRole] != ax::mojom::Role::kStaticText)
      return nil;
    NSRange range = [(NSValue*)parameter rangeValue];
    gfx::Rect rect = owner_->GetUnclippedScreenInnerTextRangeBoundsRect(
        range.location, range.location + range.length);
    NSRect nsrect = [self rectInScreen:rect];
    return [NSValue valueWithRect:nsrect];
  }

  if ([attribute isEqualToString:@"AXUIElementCountForSearchPredicate"]) {
    OneShotAccessibilityTreeSearch search(owner_);
    if (InitializeAccessibilityTreeSearch(&search, parameter))
      return [NSNumber numberWithInt:search.CountMatches()];
    return nil;
  }

  if ([attribute isEqualToString:@"AXUIElementsForSearchPredicate"]) {
    OneShotAccessibilityTreeSearch search(owner_);
    if (InitializeAccessibilityTreeSearch(&search, parameter)) {
      size_t count = search.CountMatches();
      NSMutableArray* result = [NSMutableArray arrayWithCapacity:count];
      for (size_t i = 0; i < count; ++i) {
        BrowserAccessibility* match = search.GetMatchAtIndex(i);
        [result addObject:ToBrowserAccessibilityCocoa(match)];
      }
      return result;
    }
    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    AXPlatformRange range(position->CreatePreviousLineStartPosition(
                              ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary),
                          position->CreateNextLineEndPosition(
                              ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute]) {
    BrowserAccessibility* startObject;
    BrowserAccessibility* endObject;
    int startOffset, endOffset;
    AXPlatformRange range = CreateRangeFromTextMarkerRange(parameter);
    if (range.IsNull())
      return nil;

    startObject = range.anchor()->GetAnchor();
    endObject = range.focus()->GetAnchor();
    startOffset = range.anchor()->text_offset();
    endOffset = range.focus()->text_offset();
    DCHECK(startObject && endObject);
    DCHECK_GE(startOffset, 0);
    DCHECK_GE(endOffset, 0);

    gfx::Rect rect =
        BrowserAccessibilityManager::GetRootFrameInnerTextRangeBoundsRect(
            *startObject, startOffset, *endObject, endOffset);
    NSRect nsrect = [self rectInScreen:rect];
    return [NSValue valueWithRect:nsrect];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute]) {
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;

    NSArray* textMarkerArray = parameter;
    if ([textMarkerArray count] != 2)
      return nil;

    BrowserAccessibilityPositionInstance startPosition =
        CreatePositionFromTextMarker([textMarkerArray objectAtIndex:0]);
    BrowserAccessibilityPositionInstance endPosition =
        CreatePositionFromTextMarker([textMarkerArray objectAtIndex:1]);
    if (*startPosition <= *endPosition) {
      return CreateTextMarkerRange(
          AXPlatformRange(std::move(startPosition), std::move(endPosition)));
    } else {
      return CreateTextMarkerRange(
          AXPlatformRange(std::move(endPosition), std::move(startPosition)));
    }
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    return base::SysUTF8ToNSString(position->ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute]) {
    AXPlatformRange range = CreateRangeFromTextMarkerRange(parameter);
    return base::SysUTF8ToNSString(range.ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return @"nil";
    DCHECK(position->GetAnchor());
    return base::SysUTF8ToNSString(position->GetAnchor()->ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityIndexForChildUIElementParameterizedAttribute]) {
    if (![parameter isKindOfClass:[BrowserAccessibilityCocoa class]])
      return nil;

    BrowserAccessibilityCocoa* childCocoaObj =
        (BrowserAccessibilityCocoa*)parameter;
    BrowserAccessibility* child = [childCocoaObj owner];
    if (!child)
      return nil;

    if (child->PlatformGetParent() != owner_)
      return nil;

    return @(child->GetIndexInParent());
  }

  return nil;
}

// Returns an array of parameterized attributes names that this object will
// respond to.
- (NSArray*)accessibilityParameterizedAttributeNames {
  TRACE_EVENT1(
      "accessibility",
      "BrowserAccessibilityCocoa::accessibilityParameterizedAttributeNames",
      "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:
          NSAccessibilityUIElementForTextMarkerParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute,
          NSAccessibilityLineForTextMarkerParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForLineParameterizedAttribute,
          NSAccessibilityStringForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityTextMarkerForPositionParameterizedAttribute,
          NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
          NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
          NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
          NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityIndexForChildUIElementParameterizedAttribute,
          NSAccessibilityBoundsForRangeParameterizedAttribute,
          NSAccessibilityStringForRangeParameterizedAttribute,
          NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute,
          NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
          NSAccessibilitySelectTextWithCriteriaParameterizedAttribute, nil];

  if ([[self role] isEqualToString:NSAccessibilityTableRole] ||
      [[self role] isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityCellForColumnAndRowParameterizedAttribute
    ]];
  }

  if (owner_->HasState(ax::mojom::State::kEditable)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityLineForIndexParameterizedAttribute,
      NSAccessibilityRangeForLineParameterizedAttribute,
      NSAccessibilityStringForRangeParameterizedAttribute,
      NSAccessibilityRangeForPositionParameterizedAttribute,
      NSAccessibilityRangeForIndexParameterizedAttribute,
      NSAccessibilityBoundsForRangeParameterizedAttribute,
      NSAccessibilityRTFForRangeParameterizedAttribute,
      NSAccessibilityAttributedStringForRangeParameterizedAttribute,
      NSAccessibilityStyleRangeForIndexParameterizedAttribute
    ]];
  }

  if ([self internalRole] == ax::mojom::Role::kStaticText) {
    [ret addObjectsFromArray:@[
      NSAccessibilityBoundsForRangeParameterizedAttribute
    ]];
  }

  if ([self internalRole] == ax::mojom::Role::kRootWebArea ||
      [self internalRole] == ax::mojom::Role::kWebArea) {
    [ret addObjectsFromArray:@[
      NSAccessibilityTextMarkerIsValidParameterizedAttribute,
      NSAccessibilityIndexForTextMarkerParameterizedAttribute,
      NSAccessibilityTextMarkerForIndexParameterizedAttribute
    ]];
  }

  return ret;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityActionNames",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  NSMutableArray* actions = [NSMutableArray
      arrayWithObjects:NSAccessibilityShowMenuAction,
                       NSAccessibilityScrollToVisibleAction, nil];

  // VoiceOver expects the "press" action to be first.
  if (owner_->IsClickable())
    [actions insertObject:NSAccessibilityPressAction atIndex:0];

  if (ui::IsMenuRelated(owner_->GetRole()))
    [actions addObject:NSAccessibilityCancelAction];

  if ([self internalRole] == ax::mojom::Role::kSlider ||
      [self internalRole] == ax::mojom::Role::kSpinButton) {
    [actions addObjectsFromArray:@[
      NSAccessibilityIncrementAction, NSAccessibilityDecrementAction
    ]];
  }

  return actions;
}

// Returns the list of accessibility attributes that this object supports.
- (NSArray*)accessibilityAttributeNames {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityAttributeNames",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:NSAccessibilityBlockQuoteLevelAttribute,
                       NSAccessibilityChildrenAttribute,
                       NSAccessibilityDescriptionAttribute,
                       NSAccessibilityDOMClassList,
                       NSAccessibilityDOMIdentifierAttribute,
                       NSAccessibilityElementBusyAttribute,
                       NSAccessibilityEnabledAttribute,
                       NSAccessibilityEndTextMarkerAttribute,
                       NSAccessibilityFocusedAttribute,
                       NSAccessibilityHelpAttribute,
                       NSAccessibilityLinkedUIElementsAttribute,
                       NSAccessibilityParentAttribute,
                       NSAccessibilityPositionAttribute,
                       NSAccessibilityRoleAttribute,
                       NSAccessibilityRoleDescriptionAttribute,
                       NSAccessibilitySelectedAttribute,
                       NSAccessibilitySelectedTextMarkerRangeAttribute,
                       NSAccessibilitySizeAttribute,
                       NSAccessibilityStartTextMarkerAttribute,
                       NSAccessibilitySubroleAttribute,
                       NSAccessibilityTitleAttribute,
                       NSAccessibilityTitleUIElementAttribute,
                       NSAccessibilityTopLevelUIElementAttribute,
                       NSAccessibilityValueAttribute,
                       NSAccessibilityVisitedAttribute,
                       NSAccessibilityWindowAttribute, nil];

  // Specific role attributes.
  NSString* role = [self role];
  NSString* subrole = [self subrole];
  if ([role isEqualToString:NSAccessibilityTableRole] ||
      [role isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityColumnsAttribute,
      NSAccessibilityVisibleColumnsAttribute,
      NSAccessibilityRowsAttribute,
      NSAccessibilityVisibleRowsAttribute,
      NSAccessibilityVisibleCellsAttribute,
      NSAccessibilityHeaderAttribute,
      NSAccessibilityColumnHeaderUIElementsAttribute,
      NSAccessibilityRowHeaderUIElementsAttribute,
      NSAccessibilityARIAColumnCountAttribute,
      NSAccessibilityARIARowCountAttribute,
    ]];
  } else if ([role isEqualToString:NSAccessibilityColumnRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityIndexAttribute, NSAccessibilityHeaderAttribute,
      NSAccessibilityRowsAttribute, NSAccessibilityVisibleRowsAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityCellRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityColumnIndexRangeAttribute,
      NSAccessibilityRowIndexRangeAttribute,
      NSAccessibilityARIAColumnIndexAttribute,
      NSAccessibilityARIARowIndexAttribute,
      @"AXSortDirection",
    ]];
    if ([self internalRole] != ax::mojom::Role::kColumnHeader) {
      [ret addObjectsFromArray:@[
        NSAccessibilityColumnHeaderUIElementsAttribute,
      ]];
    }
    if ([self internalRole] != ax::mojom::Role::kRowHeader) {
      [ret addObjectsFromArray:@[
        NSAccessibilityRowHeaderUIElementsAttribute,
      ]];
    }
  } else if ([role isEqualToString:@"AXWebArea"]) {
    [ret addObjectsFromArray:@[
      @"AXLoaded", NSAccessibilityLoadingProgressAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityTabGroupRole]) {
    [ret addObject:NSAccessibilityTabsAttribute];
  } else if (IsRangeValueSupported(owner_->GetData())) {
    [ret addObjectsFromArray:@[
      NSAccessibilityMaxValueAttribute, NSAccessibilityMinValueAttribute,
      NSAccessibilityValueDescriptionAttribute
    ]];
  } else if ([subrole isEqualToString:NSAccessibilityOutlineRowSubrole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityDisclosingAttribute,
      NSAccessibilityDisclosedByRowAttribute,
      NSAccessibilityDisclosureLevelAttribute,
      NSAccessibilityDisclosedRowsAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityRowRole]) {
    if (owner_->PlatformGetParent()) {
      base::string16 parentRole;
      owner_->PlatformGetParent()->GetHtmlAttribute("role", &parentRole);
      const base::string16 treegridRole(base::ASCIIToUTF16("treegrid"));
      if (parentRole == treegridRole) {
        [ret addObjectsFromArray:@[
          NSAccessibilityDisclosingAttribute,
          NSAccessibilityDisclosedByRowAttribute,
          NSAccessibilityDisclosureLevelAttribute,
          NSAccessibilityDisclosedRowsAttribute
        ]];
      } else {
        [ret addObjectsFromArray:@[ NSAccessibilityIndexAttribute ]];
      }
    }
  } else if ([role isEqualToString:NSAccessibilityListRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilitySelectedChildrenAttribute,
      NSAccessibilityVisibleChildrenAttribute
    ]];
  }

  // Caret navigation and text selection attributes.
  if (owner_->HasState(ax::mojom::State::kEditable)) {
    // Add ancestor attributes if not a web area.
    if (![role isEqualToString:@"AXWebArea"]) {
      [ret addObjectsFromArray:@[
        NSAccessibilityEditableAncestorAttribute,
        NSAccessibilityFocusableAncestorAttribute,
        NSAccessibilityHighestEditableAncestorAttribute
      ]];
    }
  }

  if (owner_->GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityInsertionPointLineNumberAttribute,
      NSAccessibilityNumberOfCharactersAttribute,
      NSAccessibilityPlaceholderValueAttribute,
      NSAccessibilitySelectedTextAttribute,
      NSAccessibilitySelectedTextRangeAttribute,
      NSAccessibilityVisibleCharacterRangeAttribute,
      NSAccessibilityValueAutofillAvailableAttribute,
      // Not currently supported by Chrome:
      // NSAccessibilityValueAutofilledAttribute,
      // Not currently supported by Chrome:
      // NSAccessibilityValueAutofillTypeAttribute
    ]];
  }

  // Add the url attribute only if it has a valid url.
  if ([self url] != nil) {
    [ret addObjectsFromArray:@[ NSAccessibilityURLAttribute ]];
  }

  // Position in set and Set size.
  // Only add these attributes for roles that use posinset and setsize.
  if (ui::IsItemLike(owner_->node()->data().role))
    [ret addObjectsFromArray:@[ NSAccessibilityARIAPosInSetAttribute ]];
  if (ui::IsSetLike(owner_->node()->data().role) ||
      ui::IsItemLike(owner_->node()->data().role))
    [ret addObjectsFromArray:@[ NSAccessibilityARIASetSizeAttribute ]];

  // Live regions.
  if (owner_->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus)) {
    [ret addObjectsFromArray:@[ NSAccessibilityARIALiveAttribute ]];
  }
  if (owner_->HasStringAttribute(ax::mojom::StringAttribute::kLiveRelevant)) {
    [ret addObjectsFromArray:@[ NSAccessibilityARIARelevantAttribute ]];
  }
  if (owner_->HasBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic)) {
    [ret addObjectsFromArray:@[ NSAccessibilityARIAAtomicAttribute ]];
  }
  if (owner_->HasBoolAttribute(ax::mojom::BoolAttribute::kBusy)) {
    [ret addObjectsFromArray:@[ NSAccessibilityARIABusyAttribute ]];
  }

  std::string dropEffect;
  if (owner_->GetHtmlAttribute("aria-dropeffect", &dropEffect)) {
    [ret addObjectsFromArray:@[ NSAccessibilityDropEffectsAttribute ]];
  }

  std::string grabbed;
  if (owner_->GetHtmlAttribute("aria-grabbed", &grabbed)) {
    [ret addObjectsFromArray:@[ NSAccessibilityGrabbedAttribute ]];
  }

  if (owner_->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityHasPopupAttribute, NSAccessibilityHasPopupValueAttribute
    ]];
  }

  if (owner_->HasBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    [ret addObjectsFromArray:@[ NSAccessibilitySelectedAttribute ]];
  }

  // Add expanded attribute only if it has expanded or collapsed state.
  if (GetState(owner_, ax::mojom::State::kExpanded) ||
      GetState(owner_, ax::mojom::State::kCollapsed)) {
    [ret addObjectsFromArray:@[ NSAccessibilityExpandedAttribute ]];
  }

  if (GetState(owner_, ax::mojom::State::kVertical) ||
      GetState(owner_, ax::mojom::State::kHorizontal)) {
    [ret addObjectsFromArray:@[ NSAccessibilityOrientationAttribute ]];
  }

  // Anything focusable or any control:
  if (owner_->HasIntAttribute(ax::mojom::IntAttribute::kRestriction) ||
      owner_->HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) ||
      owner_->HasState(ax::mojom::State::kFocusable)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityAccessKeyAttribute,
      NSAccessibilityInvalidAttribute,
      @"AXRequired",
    ]];
  }

  // TODO(accessibility) What nodes should language be exposed on given new
  // auto detection features?
  //
  // Once lang attribute inheritance becomes stable most nodes will have a
  // language, so it may make more sense to always expose this attribute.
  //
  // For now we expose the language attribute if we have any language set.
  if (owner_->node() && !owner_->node()->GetLanguage().empty()) {
    [ret addObjectsFromArray:@[ NSAccessibilityLanguageAttribute ]];
  }

  if ([self internalRole] == ax::mojom::Role::kTextFieldWithComboBox) {
    [ret addObjectsFromArray:@[
      NSAccessibilityOwnsAttribute,
    ]];
  }

  // Title UI Element.
  if (owner_->HasIntListAttribute(
          ax::mojom::IntListAttribute::kLabelledbyIds) &&
      owner_->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds)
              .size() > 0) {
    [ret addObjectsFromArray:@[ NSAccessibilityTitleUIElementAttribute ]];
  }

  if (owner_->HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete))
    [ret addObject:NSAccessibilityAutocompleteValueAttribute];

  if ([self shouldExposeTitleUIElement])
    [ret addObject:NSAccessibilityTitleUIElementAttribute];

  // TODO(aboxhall): expose NSAccessibilityServesAsTitleForUIElementsAttribute
  // for elements which are referred to by labelledby or are labels

  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityGetIndexOf",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return 0;

  NSUInteger index = 0;
  for (BrowserAccessibilityCocoa* childToCheck in [self children]) {
    if ([child isEqual:childToCheck])
      return index;
    ++index;
  }
  return NSNotFound;
}

// Returns whether or not the specified attribute can be set by the
// accessibility API via |accessibilitySetValue:forAttribute:|.
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityIsAttributeSettable",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return NO;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    if ([self internalRole] == ax::mojom::Role::kDateTime)
      return NO;

    return GetState(owner_, ax::mojom::State::kFocusable);
  }

  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return owner_->HasAction(ax::mojom::Action::kSetValue);

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] &&
      owner_->HasState(ax::mojom::State::kEditable)) {
    return YES;
  }

  return NO;
}

// Returns whether or not this object should be ignored in the accessibility
// tree.
- (BOOL)accessibilityIsIgnored {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityIsIgnored",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return YES;

  return [self isIgnored];
}

// Performs the given accessibility action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityPerformAction",
               "role=", ui::ToString([self internalRole]),
               "action=", base::SysNSStringToUTF8(action));
  if (![self instanceActive])
    return;

  // TODO(dmazzoni): Support more actions.
  BrowserAccessibilityManager* manager = owner_->manager();
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    manager->DoDefaultAction(*owner_);
    if (owner_->GetData().GetRestriction() != ax::mojom::Restriction::kNone ||
        !owner_->HasIntAttribute(ax::mojom::IntAttribute::kCheckedState))
      return;
    // Hack: preemptively set the checked state to what it should become,
    // otherwise VoiceOver will very likely report the old, incorrect state to
    // the user as it requests the value too quickly.
    ui::AXNode* node = owner_->node();
    if (!node)
      return;
    AXNodeData data(node->TakeData());  // Temporarily take data.
    if (data.role == ax::mojom::Role::kRadioButton) {
      data.SetCheckedState(ax::mojom::CheckedState::kTrue);
    } else if (data.role == ax::mojom::Role::kCheckBox ||
               data.role == ax::mojom::Role::kSwitch ||
               data.role == ax::mojom::Role::kToggleButton) {
      ax::mojom::CheckedState checkedState = data.GetCheckedState();
      ax::mojom::CheckedState newCheckedState =
          checkedState == ax::mojom::CheckedState::kFalse
              ? ax::mojom::CheckedState::kTrue
              : ax::mojom::CheckedState::kFalse;
      data.SetCheckedState(newCheckedState);
    }
    node->SetData(data);  // Set the data back in the node.
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    manager->ShowContextMenu(*owner_);
  } else if ([action isEqualToString:NSAccessibilityScrollToVisibleAction]) {
    manager->ScrollToMakeVisible(*owner_, gfx::Rect());
  } else if ([action isEqualToString:NSAccessibilityIncrementAction]) {
    manager->Increment(*owner_);
  } else if ([action isEqualToString:NSAccessibilityDecrementAction]) {
    manager->Decrement(*owner_);
  }
}

// Returns the description of the given action.
- (NSString*)accessibilityActionDescription:(NSString*)action {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityActionDescription",
               "role=", ui::ToString([self internalRole]),
               "action=", base::SysNSStringToUTF8(action));
  if (![self instanceActive])
    return nil;

  return NSAccessibilityActionDescription(action);
}

// Sets an override value for a specific accessibility attribute.
// This class does not support this.
- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString*)attribute {
  TRACE_EVENT2(
      "accessibility",
      "BrowserAccessibilityCocoa::accessibilitySetOverrideValue:forAttribute",
      "role=", ui::ToString([self internalRole]),
      "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return NO;
  return NO;
}

// Sets the value for an accessibility attribute via the accessibility API.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilitySetValue:forAttribute",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    BrowserAccessibilityManager* manager = owner_->manager();
    NSNumber* focusedNumber = value;
    BOOL focused = [focusedNumber intValue];
    if (focused)
      manager->SetFocus(*owner_);
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    NSRange range = [(NSValue*)value rangeValue];
    BrowserAccessibilityManager* manager = owner_->manager();
    manager->SetSelection(
        AXPlatformRange(owner_->CreatePositionAt(range.location),
                        owner_->CreatePositionAt(NSMaxRange(range))));
  }
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil unless this
// object is invalid.
- (id)accessibilityHitTest:(NSPoint)point {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityHitTest",
               "role=", ui::ToString([self internalRole]),
               "point=", base::SysNSStringToUTF8(NSStringFromPoint(point)));
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityManager* manager = owner_->manager();
  gfx::Point screen_point(point.x, point.y);
  screen_point += manager->GetViewBounds().OffsetFromOrigin();

  BrowserAccessibility* hit = manager->CachingAsyncHitTest(screen_point);
  if (!hit)
    return nil;

  return NSAccessibilityUnignoredAncestor(ToBrowserAccessibilityCocoa(hit));
}

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[BrowserAccessibilityCocoa class]])
    return NO;
  return ([self hash] == [object hash]);
}

- (NSUInteger)hash {
  // Potentially called during dealloc.
  if (![self instanceActive])
    return [super hash];
  return owner_->GetId();
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityNotifiesWhenDestroyed",
               "role=", ui::ToString([self internalRole]));
  // Indicate that BrowserAccessibilityCocoa will post a notification when it's
  // destroyed (see -detach). This allows VoiceOver to do some internal things
  // more efficiently.
  return YES;
}

@end
