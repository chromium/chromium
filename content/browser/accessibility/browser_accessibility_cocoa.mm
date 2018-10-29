// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/accessibility/browser_accessibility_cocoa.h"

#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <map>
#include <memory>
#include <utility>

#include "base/mac/availability.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/accessibility/browser_accessibility_position.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/public/common/content_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node.h"

#import "ui/accessibility/platform/ax_platform_node_mac.h"

using BrowserAccessibilityPositionInstance =
    content::BrowserAccessibilityPosition::AXPositionInstance;
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
NSString* const NSAccessibilityDOMIdentifierAttribute = @"AXDOMIdentifier";
NSString* const NSAccessibilityDropEffectsAttribute = @"AXDropEffects";
NSString* const NSAccessibilityEditableAncestorAttribute =
    @"AXEditableAncestor";
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
NSString* const NSAccessibilitySelectTextWithCriteriaParameterizedAttribute =
    @"AXSelectTextWithCriteria";
NSString* const NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute =
    @"AXBoundsForTextMarkerRange";
NSString* const NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute =
    @"AXTextMarkerRangeForUnorderedTextMarkers";
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

AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator,
                                   const UInt8* bytes,
                                   CFIndex length);

const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef text_marker);

size_t AXTextMarkerGetLength(AXTextMarkerRef text_marker);

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator,
                                             AXTextMarkerRef start_marker,
                                             AXTextMarkerRef end_marker);

AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(
    AXTextMarkerRangeRef text_marker_range);

AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(
    AXTextMarkerRangeRef text_marker_range);

#endif  // MAC_OS_X_VERSION_10_11

}  // extern "C"

// AXTextMarkerCreate copies from data buffer given to it.
id CreateTextMarker(BrowserAccessibilityPositionInstance position) {
  AXTextMarkerRef text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(position.get()),
      sizeof(BrowserAccessibilityPosition));
  return static_cast<id>(
      base::mac::CFTypeRefToNSObjectAutorelease(text_marker));
}

// |range| is destructed at the end of this method. |anchor| and |focus| are
// copied into the individual text markers.
id CreateTextMarkerRange(const AXPlatformRange range) {
  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(range.anchor()),
      sizeof(BrowserAccessibilityPosition)));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(range.focus()),
      sizeof(BrowserAccessibilityPosition)));
  AXTextMarkerRangeRef marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, start_marker, end_marker);
  return static_cast<id>(
      base::mac::CFTypeRefToNSObjectAutorelease(marker_range));
}

BrowserAccessibilityPositionInstance CreatePositionFromTextMarker(
    AXTextMarkerRef text_marker) {
  DCHECK(text_marker);
  if (AXTextMarkerGetLength(text_marker) !=
      sizeof(BrowserAccessibilityPosition))
    return BrowserAccessibilityPosition::CreateNullPosition();
  const UInt8* source_buffer = AXTextMarkerGetBytePtr(text_marker);
  if (!source_buffer)
    return BrowserAccessibilityPosition::CreateNullPosition();
  UInt8* destination_buffer = new UInt8[sizeof(BrowserAccessibilityPosition)];
  std::memcpy(destination_buffer, source_buffer,
              sizeof(BrowserAccessibilityPosition));
  BrowserAccessibilityPosition::AXPositionInstance position(
      reinterpret_cast<
          BrowserAccessibilityPosition::AXPositionInstance::pointer>(
          destination_buffer));
  if (!position)
    return BrowserAccessibilityPosition::CreateNullPosition();
  return position;
}

AXPlatformRange CreateRangeFromTextMarkerRange(
    AXTextMarkerRangeRef marker_range) {
  DCHECK(marker_range);
  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(
      AXTextMarkerRangeCopyStartMarker(marker_range));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(
      AXTextMarkerRangeCopyEndMarker(marker_range));
  if (!start_marker.get() || !end_marker.get())
    return AXPlatformRange();

  BrowserAccessibilityPositionInstance anchor =
      CreatePositionFromTextMarker(start_marker.get());
  BrowserAccessibilityPositionInstance focus =
      CreatePositionFromTextMarker(end_marker.get());
  // |AXPlatformRange| takes ownership of its anchor and focus.
  return AXPlatformRange(std::move(anchor), std::move(focus));
}

BrowserAccessibilityPositionInstance CreateTextPosition(
    const BrowserAccessibility& object,
    int offset,
    ax::mojom::TextAffinity affinity) {
  if (!object.instance_active())
    return BrowserAccessibilityPosition::CreateNullPosition();

  const BrowserAccessibilityManager* manager = object.manager();
  DCHECK(manager);
  return BrowserAccessibilityPosition::CreateTextPosition(
      manager->ax_tree_id(), object.GetId(), offset, affinity);
}

AXPlatformRange CreateTextRange(const BrowserAccessibility& start_object,
                                int start_offset,
                                ax::mojom::TextAffinity start_affinity,
                                const BrowserAccessibility& end_object,
                                int end_offset,
                                ax::mojom::TextAffinity end_affinity) {
  BrowserAccessibilityPositionInstance anchor =
      CreateTextPosition(start_object, start_offset, start_affinity);
  BrowserAccessibilityPositionInstance focus =
      CreateTextPosition(end_object, end_offset, end_affinity);
  // |AXPlatformRange| takes ownership of its anchor and focus.
  return AXPlatformRange(std::move(anchor), std::move(focus));
}

void AddMisspelledTextAttributes(
    const std::vector<const BrowserAccessibility*>& text_only_objects,
    NSMutableAttributedString* attributed_string) {
  [attributed_string beginEditing];
  for (const BrowserAccessibility* text_object : text_only_objects) {
    const std::vector<int32_t>& marker_types = text_object->GetIntListAttribute(
        ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& marker_starts = text_object->GetIntListAttribute(
        ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends = text_object->GetIntListAttribute(
        ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      if (!(marker_types[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kSpelling))) {
        continue;
      }

      int misspelling_start = marker_starts[i];
      int misspelling_end = marker_ends[i];
      int misspelling_length = misspelling_end - misspelling_start;
      DCHECK_GT(misspelling_length, 0);
      [attributed_string
          addAttribute:NSAccessibilityMarkedMisspelledTextAttribute
                 value:@YES
                 range:NSMakeRange(misspelling_start, misspelling_length)];
    }
  }
  [attributed_string endEditing];
}

NSString* GetTextForTextMarkerRange(AXTextMarkerRangeRef marker_range) {
  AXPlatformRange range = CreateRangeFromTextMarkerRange(marker_range);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

NSAttributedString* GetAttributedTextForTextMarkerRange(
    AXTextMarkerRangeRef marker_range) {
  BrowserAccessibility* start_object;
  BrowserAccessibility* end_object;
  int start_offset, end_offset;
  ax::mojom::TextAffinity start_affinity, end_affinity;
  AXPlatformRange ax_range = CreateRangeFromTextMarkerRange(marker_range);
  if (ax_range.IsNull())
    return nil;
  start_object = ax_range.anchor()->GetAnchor();
  end_object = ax_range.focus()->GetAnchor();
  start_offset = ax_range.anchor()->text_offset();
  end_offset = ax_range.focus()->text_offset();
  start_affinity = ax_range.anchor()->affinity();
  end_affinity = ax_range.focus()->affinity();

  NSString* text = base::SysUTF16ToNSString(
      BrowserAccessibilityManager::GetTextForRange(*start_object, *end_object));
  if ([text length] == 0)
    return nil;

  // Be permissive with the start and end offsets.
  if (start_object == end_object && end_offset < start_offset)
    std::swap(start_offset, end_offset);

  int trim_length = 0;
  if ((end_object->IsPlainTextField() || end_object->IsTextOnlyObject()) &&
      end_offset < static_cast<int>(end_object->GetText().length())) {
    trim_length = static_cast<int>(end_object->GetText().length()) - end_offset;
  }
  int range_length = [text length] - start_offset - trim_length;

  // http://crbug.com/651145
  // This shouldn't happen, so this is a temporary workaround to prevent
  // hard crashes.
  if (range_length < 0)
    return nil;

  DCHECK_GE(range_length, 0);
  NSRange range = NSMakeRange(start_offset, range_length);
  DCHECK_LE(NSMaxRange(range), [text length]);

  NSMutableAttributedString* attributed_text =
      [[[NSMutableAttributedString alloc] initWithString:text] autorelease];
  std::vector<const BrowserAccessibility*> text_only_objects =
      BrowserAccessibilityManager::FindTextOnlyObjectsInRange(*start_object,
                                                              *end_object);
  AddMisspelledTextAttributes(text_only_objects, attributed_text);
  return [attributed_text attributedSubstringFromRange:range];
}

// Returns an autoreleased copy of the AXNodeData's attribute.
NSString* NSStringForStringAttribute(
    BrowserAccessibility* browserAccessibility,
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
bool InitializeAccessibilityTreeSearch(
    OneShotAccessibilityTreeSearch* search,
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
  NSNumber *immediateDescendantsOnlyParameter =
      [dictionary objectForKey:@"AXImmediateDescendantsOnly"];
  if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
    immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];

  bool visibleOnly = false;
  NSNumber *visibleOnlyParameter = [dictionary objectForKey:@"AXVisibleOnly"];
  if ([visibleOnlyParameter isKindOfClass:[NSNumber class]])
    visibleOnly = [visibleOnlyParameter boolValue];

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
  search->SetVisibleOnly(visibleOnly);
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
      {NSAccessibilityChildrenAttribute, @"children"},
      {NSAccessibilityColumnsAttribute, @"columns"},
      {NSAccessibilityColumnHeaderUIElementsAttribute, @"columnHeaders"},
      {NSAccessibilityColumnIndexRangeAttribute, @"columnIndexRange"},
      {NSAccessibilityContentsAttribute, @"contents"},
      {NSAccessibilityDescriptionAttribute, @"description"},
      {NSAccessibilityDisclosingAttribute, @"disclosing"},
      {NSAccessibilityDisclosedByRowAttribute, @"disclosedByRow"},
      {NSAccessibilityDisclosureLevelAttribute, @"disclosureLevel"},
      {NSAccessibilityDisclosedRowsAttribute, @"disclosedRows"},
      {NSAccessibilityDropEffectsAttribute, @"dropEffects"},
      {NSAccessibilityDOMIdentifierAttribute, @"domIdentifier"},
      {NSAccessibilityEditableAncestorAttribute, @"editableAncestor"},
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
  if (!ui::IsTableLike(owner_->GetRole()))
    return nil;
  int count = -1;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount,
                               &count)) {
    return nil;
  }
  return [NSNumber numberWithInt:count];
}

- (NSNumber*)ariaColumnIndex {
  if (!ui::IsCellOrTableHeader(owner_->GetRole()))
    return nil;
  int index = -1;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                               &index)) {
    return nil;
  }
  return [NSNumber numberWithInt:index];
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
  return [NSNumber numberWithInt:owner_->GetIntAttribute(
                                     ax::mojom::IntAttribute::kPosInSet)];
}

- (NSString*)ariaRelevant {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kLiveRelevant);
}

- (NSNumber*)ariaRowCount {
  if (!ui::IsTableLike(owner_->GetRole()))
    return nil;
  int count = -1;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kAriaRowCount,
                               &count)) {
    return nil;
  }
  return [NSNumber numberWithInt:count];
}

- (NSNumber*)ariaRowIndex {
  if (!ui::IsCellOrTableHeader(owner_->GetRole()))
    return nil;
  int index = -1;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex,
                               &index)) {
    return nil;
  }
  return [NSNumber numberWithInt:index];
}

- (NSNumber*)ariaSetSize {
  if (![self instanceActive])
    return nil;
  return [NSNumber
      numberWithInt:owner_->GetIntAttribute(ax::mojom::IntAttribute::kSetSize)];
}

- (NSString*)autocompleteValue {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kAutoComplete);
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
    for (uint32_t index = 0; index < childCount; ++index) {
      BrowserAccessibilityCocoa* child =
          ToBrowserAccessibilityCocoa(owner_->PlatformGetChild(index));
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
    for (int i = 0; i < table->GetTableColCount(); i++) {
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
    int column = owner_->node()->GetTableCellColIndex();

    std::vector<int32_t> colHeaderIds = table->GetColHeaderNodeIds(column);
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
  if (!ui::IsCellOrTableHeader(owner_->GetRole()))
    return nil;

  int column = owner_->node()->GetTableCellColIndex();
  int colspan = owner_->node()->GetTableCellColSpan();
  if (column >= 0 && colspan >= 1)
    return [NSValue valueWithRange:NSMakeRange(column, colspan)];
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

- (NSString*)description {
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
  // from its descendants.
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

- (NSString*)domIdentifier {
  if (![self instanceActive])
    return nil;

  std::string id;
  if (owner_->GetHtmlAttribute("id", &id))
    return base::SysUTF8ToNSString(id);

  return nil;
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
      BrowserAccessibility* tableHeader =
          owner_->PlatformGetChild(childCount - 1);
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
    int columnIndex =
        owner_->GetIntAttribute(ax::mojom::IntAttribute::kTableColumnIndex);
    return [NSNumber numberWithInt:columnIndex];
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    int rowIndex =
        owner_->GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
    return [NSNumber numberWithInt:rowIndex];
  }

  return nil;
}

- (NSNumber*)insertionPointLineNumber {
  if (![self instanceActive])
    return nil;

  // TODO(nektar): Deprecate sel_start and sel_end attributes.
  int selStart, selEnd;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                               &selStart) ||
      !owner_->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &selEnd)) {
    return nil;
  }

  if (selStart > selEnd)
    std::swap(selStart, selEnd);

  const std::vector<int> line_breaks = owner_->GetLineStartOffsets();
  for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
    if (line_breaks[i] > selStart)
      return [NSNumber numberWithInt:i];
  }

  return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  if (![self instanceActive])
    return YES;
  return [[self role] isEqualToString:NSAccessibilityUnknownRole];
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
    case ax::mojom::InvalidState::kSpelling:
      return @"spelling";
    case ax::mojom::InvalidState::kGrammar:
      return @"grammar";
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

  ax::mojom::DescriptionFrom descriptionFrom =
      static_cast<ax::mojom::DescriptionFrom>(
          owner_->GetIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom));
  if (descriptionFrom == ax::mojom::DescriptionFrom::kPlaceholder) {
    return NSStringForStringAttribute(owner_,
                                      ax::mojom::StringAttribute::kDescription);
  }

  return NSStringForStringAttribute(owner_,
                                    ax::mojom::StringAttribute::kPlaceholder);
}

- (NSString*)language {
  if (![self instanceActive])
    return nil;
  return base::SysUTF8ToNSString(owner_->GetInheritedStringAttribute(
      ax::mojom::StringAttribute::kLanguage));
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
  if ([ret count] == 0)
    return nil;
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
  gfx::Rect bounds = owner_->GetPageBoundsRect();
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
  NSPoint pointInScreen = [self pointInScreen:origin size:size];
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
  return static_cast<ax::mojom::Role>(owner_->GetRole());
}

- (BOOL)shouldExposeNameInDescription {
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
- (NSPoint)pointInScreen:(NSPoint)origin
                    size:(NSSize)size {
  if (![self instanceActive])
    return NSZeroPoint;

  // Get the delegate for the topmost BrowserAccessibilityManager, because
  // that's the only one that can convert points to their origin in the screen.
  BrowserAccessibilityDelegate* delegate =
      owner_->manager()->GetDelegateFromRootManager();
  if (delegate) {
    gfx::Rect bounds(origin.x, origin.y, size.width, size.height);
    gfx::Point point = delegate->AccessibilityOriginInScreen(bounds);
    return NSMakePoint(point.x(), point.y());
  } else {
    return NSZeroPoint;
  }
}

// Returns a string indicating the NSAccessibility role of this object.
- (NSString*)role {
  if (![self instanceActive])
    return nil;

  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kCanvas &&
      owner_->GetBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback)) {
    return NSAccessibilityGroupRole;
  }

  if ((owner_->IsPlainTextField() &&
       owner_->HasState(ax::mojom::State::kMultiline)) ||
      owner_->IsRichTextField()) {
    return NSAccessibilityTextAreaRole;
  }

  if (role == ax::mojom::Role::kImage && owner_->HasExplicitlyEmptyName())
    return NSAccessibilityUnknownRole;

  // If this is a web area for a presentational iframe, give it a role of
  // something other than WebArea so that the fact that it's a separate doc
  // is not exposed to AT.
  if (owner_->IsWebAreaForPresentationalIframe())
    return NSAccessibilityGroupRole;

  return [AXPlatformNodeCocoa nativeRoleFromAXRole:role];
}

// Returns a string indicating the role description of this object.
- (NSString*)roleDescription {
  if (![self instanceActive])
    return nil;

  if (owner_->HasStringAttribute(
          ax::mojom::StringAttribute::kRoleDescription)) {
    return NSStringForStringAttribute(
        owner_, ax::mojom::StringAttribute::kRoleDescription);
  }

  NSString* role = [self role];

  ContentClient* content_client = content::GetContentClient();

  // The following descriptions are specific to webkit.
  if ([role isEqualToString:@"AXWebArea"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_WEB_AREA));
  }

  if ([role isEqualToString:@"NSAccessibilityLinkRole"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_LINK));
  }

  if ([role isEqualToString:@"AXHeading"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_HEADING));
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

  switch([self internalRole]) {
    case ax::mojom::Role::kArticle:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_ARTICLE));
    case ax::mojom::Role::kBanner:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_BANNER));
    case ax::mojom::Role::kCheckBox:
      return base::SysUTF16ToNSString(
          content_client->GetLocalizedString(IDS_AX_ROLE_CHECK_BOX));
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
    for (int i = 0; i < table->GetTableRowCount(); i++) {
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
  if (!ui::IsCellOrTableHeader(owner_->GetRole()))
    return nil;

  int row = owner_->node()->GetTableCellRowIndex();
  int rowspan = owner_->node()->GetTableCellRowSpan();
  if (row >= 0 && rowspan >= 1)
    return [NSValue valueWithRange:NSMakeRange(row, rowspan)];
  return nil;
}

- (NSArray*)rows {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == ax::mojom::Role::kTable ||
      [self internalRole] == ax::mojom::Role::kGrid) {
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
  if (!focusedChild->IsDescendantOf(owner_))
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
  uint32_t childCount = owner_->PlatformChildCount();
  for (uint32_t index = 0; index < childCount; ++index) {
    BrowserAccessibility* child = owner_->PlatformGetChild(index);
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

  // TODO(nektar): Deprecate sel_start and sel_end attributes.
  int selStart, selEnd;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                               &selStart) ||
      !owner_->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &selEnd)) {
    return nil;
  }

  if (selStart > selEnd)
    std::swap(selStart, selEnd);

  int selLength = selEnd - selStart;
  base::string16 value = owner_->GetValue();
  return base::SysUTF16ToNSString(value.substr(selStart, selLength));
}

- (NSValue*)selectedTextRange {
  if (![self instanceActive])
    return nil;

  // TODO(nektar): Deprecate sel_start and sel_end attributes.
  int selStart, selEnd;
  if (!owner_->GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                               &selStart) ||
      !owner_->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &selEnd)) {
    return nil;
  }

  if (selStart > selEnd)
    std::swap(selStart, selEnd);

  int selLength = selEnd - selStart;
  return [NSValue valueWithRange:NSMakeRange(selStart, selLength)];
}

- (id)selectedTextMarkerRange {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityManager* manager = owner_->manager();
  if (!manager)
    return nil;

  int32_t anchorId = manager->GetTreeData().sel_anchor_object_id;
  const BrowserAccessibility* anchorObject = manager->GetFromID(anchorId);
  if (!anchorObject)
    return nil;

  int32_t focusId = manager->GetTreeData().sel_focus_object_id;
  const BrowserAccessibility* focusObject = manager->GetFromID(focusId);
  if (!focusObject)
    return nil;

  int anchorOffset = manager->GetTreeData().sel_anchor_offset;
  int focusOffset = manager->GetTreeData().sel_focus_offset;
  if (anchorOffset < 0 || focusOffset < 0)
    return nil;

  ax::mojom::TextAffinity anchorAffinity =
      manager->GetTreeData().sel_anchor_affinity;
  ax::mojom::TextAffinity focusAffinity =
      manager->GetTreeData().sel_focus_affinity;

  return CreateTextMarkerRange(CreateTextRange(*anchorObject, anchorOffset,
                                               anchorAffinity, *focusObject,
                                               focusOffset, focusAffinity));
}

- (NSValue*)size {
  if (![self instanceActive])
    return nil;
  gfx::Rect bounds = owner_->GetPageBoundsRect();
  return  [NSValue valueWithSize:NSMakeSize(bounds.width(), bounds.height())];
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
- (NSString*) subrole {
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

  for (uint i=0; i < [[self children] count]; ++i) {
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

  return nil;
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
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityIncrementorRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole] ||
             ([role isEqualToString:NSAccessibilitySplitterRole] &&
              owner_->HasState(ax::mojom::State::kFocusable))) {
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
    return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1",
                red / 255., green / 255., blue / 255.];
  }

  return base::SysUTF16ToNSString(owner_->GetValue());
}

// TODO(crbug.com/865101) Remove this once the autofill state works.
- (BOOL)isFocusedInputWithSuggestions {
  if (!owner_->IsPlainTextField())
    return false;
  BrowserAccessibilityManager* manager = owner_->manager();
  if (manager->GetFocus() != owner_)
    return false;
  return ui::AXPlatformNode::HasInputSuggestions();
}

- (NSNumber*)valueAutofillAvailable {
  if (![self instanceActive])
    return nil;
  // TODO(crbug.com/865101) Use this instead:
  // return owner_->HasState(ax::mojom::State::kAutofillAvailable) ? @YES : @NO;
  return [self isFocusedInputWithSuggestions] ? @YES : @NO;
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

  return [manager->GetParentView() window];
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
    std::vector<const BrowserAccessibility*> textOnlyObjects =
        BrowserAccessibilityManager::FindTextOnlyObjectsInRange(*owner_,
                                                                *owner_);
    AddMisspelledTextAttributes(textOnlyObjects, attributedValue);
  }

  return [attributedValue attributedSubstringFromRange:range];
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (![self instanceActive])
    return nil;

  SEL selector =
      NSSelectorFromString([self methodNameForAttribute:attribute]);
  if (selector)
    return [self performSelector:selector];

  return nil;
}

// Returns the accessibility value for the given attribute and parameter. If the
// value isn't supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (![self instanceActive])
    return nil;

  const std::vector<int> line_breaks = owner_->GetLineStartOffsets();
  base::string16 value = owner_->GetValue();
  int len = static_cast<int>(value.size());

  if ([attribute isEqualToString:
      NSAccessibilityStringForRangeParameterizedAttribute]) {
    return [self valueForRange:[(NSValue*)parameter rangeValue]];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityAttributedStringForRangeParameterizedAttribute]) {
    return [self attributedValueForRange:[(NSValue*)parameter rangeValue]];
  }

  if ([attribute isEqualToString:
      NSAccessibilityLineForIndexParameterizedAttribute]) {
    int index = [(NSNumber*)parameter intValue];
    for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
      if (line_breaks[i] > index)
        return [NSNumber numberWithInt:i];
    }
    return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
  }

  if ([attribute isEqualToString:
      NSAccessibilityRangeForLineParameterizedAttribute]) {
    int line_index = [(NSNumber*)parameter intValue];
    int line_count = static_cast<int>(line_breaks.size()) + 1;
    if (line_index < 0 || line_index >= line_count)
      return nil;
    int start = line_index > 0 ? line_breaks[line_index - 1] : 0;
    int end = line_index < line_count - 1 ? line_breaks[line_index] : len;
    return [NSValue valueWithRange:
        NSMakeRange(start, end - start)];
  }

  if ([attribute isEqualToString:
      NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
    if ([self internalRole] != ax::mojom::Role::kTable &&
        [self internalRole] != ax::mojom::Role::kGrid) {
      return nil;
    }
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;
    if (2 != [parameter count])
      return nil;
    NSArray* array = parameter;
    int column = [[array objectAtIndex:0] intValue];
    int row = [[array objectAtIndex:1] intValue];

    ui::AXNode* cell_node = owner_->node()->GetTableCellFromCoords(row, column);
    if (!cell_node)
      return nil;

    BrowserAccessibility* cell = owner_->manager()->GetFromID(cell_node->id());
    if (cell)
      return ToBrowserAccessibilityCocoa(cell);
  }

  if ([attribute isEqualToString:@"AXUIElementForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (!position->IsNullPosition())
      return ToBrowserAccessibilityCocoa(position->GetAnchor());

    return nil;
  }

  if ([attribute isEqualToString:@"AXTextMarkerRangeForUIElement"]) {
    BrowserAccessibilityPositionInstance startPosition =
        owner_->CreatePositionAt(0);
    BrowserAccessibilityPositionInstance endPosition =
        startPosition->CreatePositionAtEndOfAnchor();
    AXPlatformRange range =
        AXPlatformRange(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute isEqualToString:@"AXStringForTextMarkerRange"])
    return GetTextForTextMarkerRange(parameter);

  if ([attribute isEqualToString:@"AXAttributedStringForTextMarkerRange"])
    return GetAttributedTextForTextMarkerRange(parameter);

  if ([attribute isEqualToString:@"AXNextTextMarkerForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextCharacterPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute isEqualToString:@"AXPreviousTextMarkerForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousCharacterPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute isEqualToString:@"AXLeftWordTextMarkerRangeForTextMarker"]) {
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

  if ([attribute isEqualToString:@"AXRightWordTextMarkerRangeForTextMarker"]) {
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

  if ([attribute isEqualToString:@"AXNextWordEndTextMarkerForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextWordEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:@"AXPreviousWordStartTextMarkerForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousWordStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute isEqualToString:@"AXTextMarkerRangeForLine"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startPosition =
        position->CreatePreviousLineStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibilityPositionInstance endPosition =
        position->CreateNextLineEndPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute isEqualToString:@"AXLeftLineTextMarkerRangeForTextMarker"]) {
    BrowserAccessibilityPositionInstance endPosition =
        CreatePositionFromTextMarker(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startLinePosition =
        endPosition->CreatePreviousLineStartPosition(
            ui::AXBoundaryBehavior::CrossBoundary);
    BrowserAccessibilityPositionInstance endLinePosition =
        endPosition->CreatePreviousLineEndPosition(
            ui::AXBoundaryBehavior::CrossBoundary);
    BrowserAccessibilityPositionInstance startPosition =
        *startLinePosition <= *endLinePosition ? std::move(endLinePosition)
                                               : std::move(startLinePosition);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute isEqualToString:@"AXRightLineTextMarkerRangeForTextMarker"]) {
    BrowserAccessibilityPositionInstance startPosition =
        CreatePositionFromTextMarker(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    BrowserAccessibilityPositionInstance startLinePosition =
        startPosition->CreateNextLineStartPosition(
            ui::AXBoundaryBehavior::CrossBoundary);
    BrowserAccessibilityPositionInstance endLinePosition =
        startPosition->CreateNextLineEndPosition(
            ui::AXBoundaryBehavior::CrossBoundary);
    BrowserAccessibilityPositionInstance endPosition =
        *startLinePosition <= *endLinePosition ? std::move(startLinePosition)
                                               : std::move(endLinePosition);
    AXPlatformRange range(std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute isEqualToString:@"AXNextLineEndTextMarkerForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextLineEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:@"AXPreviousLineStartTextMarkerForTextMarker"]) {
    BrowserAccessibilityPositionInstance position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousLineStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute isEqualToString:@"AXLengthForTextMarkerRange"]) {
    NSString* text = GetTextForTextMarkerRange(parameter);
    return [NSNumber numberWithInt:[text length]];
  }

  if ([attribute isEqualToString:
      NSAccessibilityBoundsForRangeParameterizedAttribute]) {
    if ([self internalRole] != ax::mojom::Role::kStaticText)
      return nil;
    NSRange range = [(NSValue*)parameter rangeValue];
    gfx::Rect rect =
        owner_->GetScreenBoundsForRange(range.location, range.length);
    NSPoint origin = NSMakePoint(rect.x(), rect.y());
    NSSize size = NSMakeSize(rect.width(), rect.height());
    NSPoint pointInScreen = [self pointInScreen:origin size:size];
    NSRect nsrect = NSMakeRect(
        pointInScreen.x, pointInScreen.y, rect.width(), rect.height());
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

  if ([attribute isEqualToString:
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

  if ([attribute isEqualToString:
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

    gfx::Rect rect = BrowserAccessibilityManager::GetPageBoundsForRange(
        *startObject, startOffset, *endObject, endOffset);
    NSPoint origin = NSMakePoint(rect.x(), rect.y());
    NSSize size = NSMakeSize(rect.width(), rect.height());
    NSPoint pointInScreen = [self pointInScreen:origin size:size];
    NSRect nsrect = NSMakeRect(
        pointInScreen.x, pointInScreen.y, rect.width(), rect.height());
    return [NSValue valueWithRect:nsrect];
  }

  if ([attribute isEqualToString:
           NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute]) {
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;

    NSArray* text_marker_array = parameter;
    if ([text_marker_array count] != 2)
      return nil;

    BrowserAccessibilityPositionInstance startPosition =
        CreatePositionFromTextMarker([text_marker_array objectAtIndex:0]);
    BrowserAccessibilityPositionInstance endPosition =
        CreatePositionFromTextMarker([text_marker_array objectAtIndex:1]);
    if (*startPosition <= *endPosition) {
      return CreateTextMarkerRange(
          AXPlatformRange(std::move(startPosition), std::move(endPosition)));
    } else {
      return CreateTextMarkerRange(
          AXPlatformRange(std::move(endPosition), std::move(startPosition)));
    }
  }

  if ([attribute isEqualToString:
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
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:
          @"AXUIElementForTextMarker", @"AXTextMarkerRangeForUIElement",
          @"AXLineForTextMarker", @"AXTextMarkerRangeForLine",
          @"AXStringForTextMarkerRange", @"AXTextMarkerForPosition",
          @"AXAttributedStringForTextMarkerRange",
          @"AXNextTextMarkerForTextMarker",
          @"AXPreviousTextMarkerForTextMarker",
          @"AXLeftWordTextMarkerRangeForTextMarker",
          @"AXRightWordTextMarkerRangeForTextMarker",
          @"AXLeftLineTextMarkerRangeForTextMarker",
          @"AXRightLineTextMarkerRangeForTextMarker",
          @"AXSentenceTextMarkerRangeForTextMarker",
          @"AXParagraphTextMarkerRangeForTextMarker",
          @"AXNextWordEndTextMarkerForTextMarker",
          @"AXPreviousWordStartTextMarkerForTextMarker",
          @"AXNextLineEndTextMarkerForTextMarker",
          @"AXPreviousLineStartTextMarkerForTextMarker",
          @"AXNextSentenceEndTextMarkerForTextMarker",
          @"AXPreviousSentenceStartTextMarkerForTextMarker",
          @"AXNextParagraphEndTextMarkerForTextMarker",
          @"AXPreviousParagraphStartTextMarkerForTextMarker",
          @"AXStyleTextMarkerRangeForTextMarker", @"AXLengthForTextMarkerRange",
          NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
          NSAccessibilityIndexForChildUIElementParameterizedAttribute,
          NSAccessibilityBoundsForRangeParameterizedAttribute,
          NSAccessibilityStringForRangeParameterizedAttribute,
          NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute,
          NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
          NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
          NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
          NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute,
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
    [ret addObjectsFromArray: @[
                 NSAccessibilityTextMarkerIsValidParameterizedAttribute,
                     NSAccessibilityIndexForTextMarkerParameterizedAttribute,
                     NSAccessibilityTextMarkerForIndexParameterizedAttribute]];
  }

  return ret;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
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

// Returns a sub-array of values for the given attribute value, starting at
// index, with up to maxCount items.  If the given index is out of bounds,
// or there are no values for the given attribute, it will return nil.
// This method is used for querying subsets of values, without having to
// return a large set of data, such as elements with a large number of
// children.
- (NSArray*)accessibilityArrayAttributeValues:(NSString*)attribute
                                        index:(NSUInteger)index
                                     maxCount:(NSUInteger)maxCount {
  if (![self instanceActive])
    return nil;

  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  if (!fullArray)
    return nil;
  NSUInteger arrayCount = [fullArray count];
  if (index >= arrayCount)
    return nil;
  NSRange subRange;
  if ((index + maxCount) > arrayCount) {
    subRange = NSMakeRange(index, arrayCount - index);
  } else {
    subRange = NSMakeRange(index, maxCount);
  }
  return [fullArray subarrayWithRange:subRange];
}

// Returns the count of the specified accessibility array attribute.
- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute {
  if (![self instanceActive])
    return 0;

  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

// Returns the list of accessibility attributes that this object supports.
- (NSArray*)accessibilityAttributeNames {
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:NSAccessibilityAccessKeyAttribute,
                       NSAccessibilityChildrenAttribute,
                       NSAccessibilityDescriptionAttribute,
                       NSAccessibilityDOMIdentifierAttribute,
                       NSAccessibilityEnabledAttribute,
                       NSAccessibilityEndTextMarkerAttribute,
                       NSAccessibilityFocusedAttribute,
                       NSAccessibilityHelpAttribute,
                       NSAccessibilityInvalidAttribute,
                       NSAccessibilityLinkedUIElementsAttribute,
                       NSAccessibilityParentAttribute,
                       NSAccessibilityPositionAttribute,
                       NSAccessibilityRoleAttribute,
                       NSAccessibilityRoleDescriptionAttribute,
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
      NSAccessibilityColumnsAttribute, NSAccessibilityVisibleColumnsAttribute,
      NSAccessibilityRowsAttribute, NSAccessibilityVisibleRowsAttribute,
      NSAccessibilityVisibleCellsAttribute, NSAccessibilityHeaderAttribute,
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
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityIncrementorRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole] ||
             ([role isEqualToString:NSAccessibilitySplitterRole] &&
              owner_->HasState(ax::mojom::State::kFocusable))) {
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
    [ret addObjectsFromArray:@[
      NSAccessibilityAutocompleteValueAttribute,
      NSAccessibilityInsertionPointLineNumberAttribute,
      NSAccessibilityNumberOfCharactersAttribute,
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

  // Add ancestor attributes if not a web area.
  if (![role isEqualToString:@"AXWebArea"]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityEditableAncestorAttribute,
      NSAccessibilityFocusableAncestorAttribute,
      NSAccessibilityHighestEditableAncestorAttribute
    ]];
  }

  // Add the url attribute only if it has a valid url.
  if ([self url] != nil) {
    [ret addObjectsFromArray:@[ NSAccessibilityURLAttribute ]];
  }

  // Position in set and Set size
  if (owner_->HasIntAttribute(ax::mojom::IntAttribute::kPosInSet)) {
    [ret addObjectsFromArray:@[ NSAccessibilityARIAPosInSetAttribute ]];
  }
  if (owner_->HasIntAttribute(ax::mojom::IntAttribute::kSetSize)) {
    [ret addObjectsFromArray:@[ NSAccessibilityARIASetSizeAttribute ]];
  }

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

  if (owner_->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder)) {
    [ret addObjectsFromArray:@[ NSAccessibilityPlaceholderValueAttribute ]];
  }

  if (GetState(owner_, ax::mojom::State::kRequired)) {
    [ret addObjectsFromArray:@[ @"AXRequired" ]];
  }

  if (owner_->HasStringAttribute(ax::mojom::StringAttribute::kLanguage)) {
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
  // TODO(aboxhall): expose NSAccessibilityServesAsTitleForUIElementsAttribute
  // for elements which are referred to by labelledby or are labels

  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
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
  if (![self instanceActive])
    return YES;

  return [self isIgnored];
}

// Performs the given accessibility action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  if (![self instanceActive])
    return;

  // TODO(dmazzoni): Support more actions.
  BrowserAccessibilityManager* manager = owner_->manager();
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    manager->DoDefaultAction(*owner_);
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
  if (![self instanceActive])
    return nil;

  return NSAccessibilityActionDescription(action);
}

// Sets an override value for a specific accessibility attribute.
// This class does not support this.
- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString*)attribute {
  if (![self instanceActive])
    return NO;
  return NO;
}

// Sets the value for an accessibility attribute via the accessibility API.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
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
  // Indicate that BrowserAccessibilityCocoa will post a notification when it's
  // destroyed (see -detach). This allows VoiceOver to do some internal things
  // more efficiently.
  return YES;
}

@end
