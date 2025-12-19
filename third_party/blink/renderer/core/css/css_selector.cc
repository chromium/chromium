/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/css_selector.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/compiler_specific.h"
#include "base/strings/string_view_util.h"
#include "style_rule.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/link_condition.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif  // DCHECK_IS_ON()

namespace blink {

namespace {

constexpr bool kExpandPseudoReferences = true;

unsigned MaximumSpecificity(const CSSSelectorList* list) {
  if (!list) {
    return 0;
  }
  return list->MaximumSpecificity();
}

}  // namespace

// Returns the maximum specificity across a selector list, only including
// the (complex) selectors for which the `predicate` returns true.
template <typename Predicate>
unsigned MaximumSpecificity(
    const CSSSelector* first_selector,
    Predicate predicate = [](const CSSSelector*) { return true; }) {
  unsigned specificity = 0;
  for (const CSSSelector* s = first_selector; s;
       s = CSSSelectorList::Next(*s)) {
    if (predicate(s)) {
      specificity = std::max(specificity, s->Specificity());
    }
  }
  return specificity;
}

struct SameSizeAsCSSSelector {
  unsigned bitfields;
  union {
    AtomicString value_;
    QualifiedName tag_q_name_or_attribute_;
    Member<void*> rare_data_;
  } pointers;
};

ASSERT_SIZE(CSSSelector, SameSizeAsCSSSelector);

CSSSelector::CSSSelector(MatchType match_type,
                         const QualifiedName& attribute,
                         AttributeMatchType case_sensitivity)
    : bits_(
          RelationField::encode(kSubSelector) | MatchField::encode(match_type) |
          PseudoTypeField::encode(kPseudoUnknown) |
          IsLastInSelectorListField::encode(false) |
          IsLastInComplexSelectorField::encode(false) |
          HasRareDataField::encode(false) | IsForPageField::encode(false) |
          IsImplicitlyAddedField::encode(false) |
          IsCoveredByBucketingField::encode(false) |
          AttributeMatchField::encode(static_cast<unsigned>(case_sensitivity)) |
          LegacyCaseInsensitiveMatchField::encode(
              !HTMLDocument::IsCaseSensitiveAttribute(attribute) &&
              case_sensitivity != AttributeMatchType::kCaseSensitiveAlways) |
          IsScopeContainingField::encode(false)),
      data_(attribute) {
  DCHECK_EQ(match_type, kAttributeSet);
}

CSSSelector::CSSSelector(MatchType match_type,
                         const QualifiedName& attribute,
                         AttributeMatchType case_sensitivity,
                         const AtomicString& value)
    : bits_(
          RelationField::encode(kSubSelector) |
          MatchField::encode(static_cast<unsigned>(match_type)) |
          PseudoTypeField::encode(kPseudoUnknown) |
          IsLastInSelectorListField::encode(false) |
          IsLastInComplexSelectorField::encode(false) |
          HasRareDataField::encode(true) | IsForPageField::encode(false) |
          IsImplicitlyAddedField::encode(false) |
          IsCoveredByBucketingField::encode(false) |
          AttributeMatchField::encode(static_cast<unsigned>(case_sensitivity)) |
          LegacyCaseInsensitiveMatchField::encode(
              !HTMLDocument::IsCaseSensitiveAttribute(attribute) &&
              case_sensitivity != AttributeMatchType::kCaseSensitiveAlways) |
          IsScopeContainingField::encode(false)),
      data_(MakeGarbageCollected<RareData>(value)) {
  DCHECK(IsAttributeSelector());
  data_.rare_data_->attribute_ = attribute;
}

void CSSSelector::CreateRareData() {
  DCHECK_NE(Match(), kTag);
  DCHECK_NE(Match(), kUniversalTag);
  if (HasRareData()) {
    return;
  }
  // This transitions the DataUnion from |value_| to |rare_data_| and thus needs
  // to be careful to correctly manage explicitly destruction of |value_|
  // followed by placement new of |rare_data_|. A straight-assignment will
  // compile and may kinda work, but will be undefined behavior.
  auto* rare_data = MakeGarbageCollected<RareData>(data_.value_);
  data_.value_.~AtomicString();
  data_.rare_data_ = rare_data;
  bits_.set<HasRareDataField>(true);
}

unsigned CSSSelector::Specificity() const {
  if (IsForPage()) {
    return SpecificityForPage() & CSSSelector::kMaxValueMask;
  }

  unsigned total = 0;
  unsigned temp = 0;

  for (const CSSSelector* selector = this; selector;
       selector = selector->NextSimpleSelector()) {
    temp = total + selector->SpecificityForOneSelector();
    // Clamp each component to its max in the case of overflow.
    if ((temp & kIdMask) < (total & kIdMask)) {
      total |= kIdMask;
    } else if ((temp & kClassMask) < (total & kClassMask)) {
      total |= kClassMask;
    } else if ((temp & kElementMask) < (total & kElementMask)) {
      total |= kElementMask;
    } else {
      total = temp;
    }
  }
  return total;
}

std::array<uint8_t, 3> CSSSelector::SpecificityTuple() const {
  unsigned specificity = Specificity();

  uint8_t a = (specificity & kIdMask) >> 16;
  uint8_t b = (specificity & kClassMask) >> 8;
  uint8_t c = (specificity & kElementMask);

  return {a, b, c};
}

inline unsigned CSSSelector::SpecificityForOneSelector() const {
  // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity.
  // This function isn't quite correct.
  // http://www.w3.org/TR/selectors/#specificity
  switch (Match()) {
    case kId:
      return kIdSpecificity;
    case kPseudoClass:
      switch (GetPseudoType()) {
        case kPseudoWhere:
          return 0;
        case kPseudoHost:
          if (!SelectorList()) {
            return kClassLikeSpecificity;
          }
          [[fallthrough]];
        case kPseudoHostContext:
          DCHECK(SelectorList()->IsSingleComplexSelector());
          return kClassLikeSpecificity + SelectorList()->First()->Specificity();
        case kPseudoNot:
          DCHECK(SelectorList());
          [[fallthrough]];
        case kPseudoIs:
          return MaximumSpecificity(SelectorList());
        case kPseudoHas:
          return MaximumSpecificity(SelectorList());
        case kPseudoParent:
          if (data_.parent_rule_ == nullptr) {
            // & in a non-nesting context matches nothing.
            return 0;
          }
          return MaximumSpecificity(
              data_.parent_rule_->FirstSelector(),
              [](const CSSSelector* selector) {
                return selector->IsAllowedInParentPseudo();
              });
        case kPseudoNthChild:
        case kPseudoNthLastChild:
          if (SelectorList()) {
            return kClassLikeSpecificity + MaximumSpecificity(SelectorList());
          } else {
            return kClassLikeSpecificity;
          }
        case kPseudoRelativeAnchor:
          return 0;
        case kPseudoScope:
          if (IsImplicit()) {
            // Implicit :scope pseudo-classes are added to selectors
            // within @scope. Such pseudo-classes must not have any effect
            // on the specificity of the scoped selector.
            //
            // https://drafts.csswg.org/css-cascade-6/#scope-effects
            return 0;
          }
          break;
        // FIXME: PseudoAny should base the specificity on the sub-selectors.
        // See http://lists.w3.org/Archives/Public/www-style/2010Sep/0530.html
        case kPseudoAny:
        default:
          break;
      }
      return kClassLikeSpecificity;
    case kPseudoElement:
      switch (GetPseudoType()) {
        case kPseudoSlotted:
          DCHECK(SelectorList()->IsSingleComplexSelector());
          return kTagSpecificity + SelectorList()->First()->Specificity();
        case kPseudoViewTransitionGroup:
        case kPseudoViewTransitionGroupChildren:
        case kPseudoViewTransitionImagePair:
        case kPseudoViewTransitionOld:
        case kPseudoViewTransitionNew: {
          CHECK(!IdentList().empty());
          return (IdentList().size() == 1u && IdentList()[0].IsNull())
                     ? 0
                     : kTagSpecificity;
        }
        default:
          break;
      }
      return kTagSpecificity;
    case kClass:
    case kAttributeExact:
    case kAttributeSet:
    case kAttributeList:
    case kAttributeHyphen:
    case kAttributeContain:
    case kAttributeBegin:
    case kAttributeEnd:
      return kClassLikeSpecificity;
    case kTag:
      return kTagSpecificity;
    case kUniversalTag:
      return 0;
    case kInvalidList:
    case kPagePseudoClass:
      NOTREACHED();
    case kUnknown:
      return 0;
  }
  NOTREACHED();
}

unsigned CSSSelector::SpecificityForPage() const {
  // See https://drafts.csswg.org/css-page/#cascading-and-page-context
  unsigned s = 0;

  for (const CSSSelector* component = this; component;
       component = component->NextSimpleSelector()) {
    switch (component->Match()) {
      case kTag:
        s += 4;
        break;
      case kPagePseudoClass:
        switch (component->GetPseudoType()) {
          case kPseudoFirstPage:
            s += 2;
            break;
          case kPseudoLeftPage:
          case kPseudoRightPage:
            s += 1;
            break;
          default:
            NOTREACHED();
        }
        break;
      default:
        break;
    }
  }
  return s;
}

PseudoId CSSSelector::GetPseudoId(PseudoType type) {
  switch (type) {
    case kPseudoFirstLine:
      return kPseudoIdFirstLine;
    case kPseudoFirstLetter:
      return kPseudoIdFirstLetter;
    case kPseudoSelection:
      return kPseudoIdSelection;
    case kPseudoCheckMark:
      return kPseudoIdCheckMark;
    case kPseudoBefore:
      return kPseudoIdBefore;
    case kPseudoAfter:
      return kPseudoIdAfter;
    case kPseudoPickerIcon:
      return kPseudoIdPickerIcon;
    case kPseudoInterestHint:
      return kPseudoIdInterestHint;
    case kPseudoMarker:
      return kPseudoIdMarker;
    case kPseudoBackdrop:
      return kPseudoIdBackdrop;
    case kPseudoScrollbar:
      return kPseudoIdScrollbar;
    case kPseudoScrollMarker:
      return kPseudoIdScrollMarker;
    case kPseudoScrollMarkerGroup:
      return kPseudoIdScrollMarkerGroup;
    case kPseudoScrollButton:
      return kPseudoIdScrollButton;
    case kPseudoColumn:
      return kPseudoIdColumn;
    case kPseudoScrollbarButton:
      return kPseudoIdScrollbarButton;
    case kPseudoScrollbarCorner:
      return kPseudoIdScrollbarCorner;
    case kPseudoScrollbarThumb:
      return kPseudoIdScrollbarThumb;
    case kPseudoScrollbarTrack:
      return kPseudoIdScrollbarTrack;
    case kPseudoScrollbarTrackPiece:
      return kPseudoIdScrollbarTrackPiece;
    case kPseudoResizer:
      return kPseudoIdResizer;
    case kPseudoSearchText:
      return kPseudoIdSearchText;
    case kPseudoTargetText:
      return kPseudoIdTargetText;
    case kPseudoHighlight:
      return kPseudoIdHighlight;
    case kPseudoSpellingError:
      return kPseudoIdSpellingError;
    case kPseudoGrammarError:
      return kPseudoIdGrammarError;
    case kPseudoPlaceholder:
      return kPseudoIdPlaceholder;
    case kPseudoFileSelectorButton:
      return kPseudoIdFileSelectorButton;
    case kPseudoDetailsContent:
      return kPseudoIdDetailsContent;
    case kPseudoPermissionIcon:
      return kPseudoIdPermissionIcon;
    case kPseudoPicker:
      // NOTE: When we support more than one argument to ::picker() we will
      // need to refactor something here (possibly the callers of this method)
      // to account for this.
      return kPseudoIdPickerSelect;
    case kPseudoViewTransition:
      return kPseudoIdViewTransition;
    case kPseudoViewTransitionGroup:
      return kPseudoIdViewTransitionGroup;
    case kPseudoViewTransitionGroupChildren:
      return kPseudoIdViewTransitionGroupChildren;
    case kPseudoViewTransitionImagePair:
      return kPseudoIdViewTransitionImagePair;
    case kPseudoViewTransitionOld:
      return kPseudoIdViewTransitionOld;
    case kPseudoViewTransitionNew:
      return kPseudoIdViewTransitionNew;
    case kPseudoOverscrollAreaParent:
      return kPseudoIdOverscrollAreaParent;
    case kPseudoActive:
    case kPseudoActiveViewTransition:
    case kPseudoActiveViewTransitionType:
    case kPseudoAny:
    case kPseudoAnyLink:
    case kPseudoAutofill:
    case kPseudoAutofillPreviewed:
    case kPseudoAutofillSelected:
    case kPseudoBlinkInternalElement:
    case kPseudoChecked:
    case kPseudoCornerPresent:
    case kPseudoCue:
    case kPseudoCurrent:
    case kPseudoDecrement:
    case kPseudoDefault:
    case kPseudoDefined:
    case kPseudoDialogInTopLayer:
    case kPseudoDir:
    case kPseudoDisabled:
    case kPseudoDoubleButton:
    case kPseudoDrag:
    case kPseudoEmpty:
    case kPseudoEnabled:
    case kPseudoEnd:
    case kPseudoFirstChild:
    case kPseudoFirstOfType:
    case kPseudoFirstPage:
    case kPseudoFocus:
    case kPseudoFocusVisible:
    case kPseudoFocusWithin:
    case kPseudoFullPageMedia:
    case kPseudoFullScreen:
    case kPseudoFullScreenAncestor:
    case kPseudoFullscreen:
    case kPseudoFutureCue:
    case kPseudoHas:
    case kPseudoHasSlotted:
    case kPseudoHasDatalist:
    case kPseudoHorizontal:
    case kPseudoHost:
    case kPseudoHostContext:
    case kPseudoHostHasNonAutoAppearance:
    case kPseudoHover:
    case kPseudoInRange:
    case kPseudoIncrement:
    case kPseudoIndeterminate:
    case kPseudoInterestSource:
    case kPseudoInterestTarget:
    case kPseudoInvalid:
    case kPseudoIs:
    case kPseudoIsHtml:
    case kPseudoLang:
    case kPseudoLastChild:
    case kPseudoLastOfType:
    case kPseudoLeftPage:
    case kPseudoLink:
    case kPseudoLinkTo:
    case kPseudoListBox:
    case kPseudoMenulistPopoverWithMenubarAnchor:
    case kPseudoMenulistPopoverWithMenulistAnchor:
    case kPseudoModal:
    case kPseudoMultiSelectFocus:
    case kPseudoNoButton:
    case kPseudoNot:
    case kPseudoNthChild:
    case kPseudoNthLastChild:
    case kPseudoNthLastOfType:
    case kPseudoNthOfType:
    case kPseudoOnlyChild:
    case kPseudoOnlyOfType:
    case kPseudoOpen:
    case kPseudoOptional:
    case kPseudoOutOfRange:
    case kPseudoOverscrollTarget:
    case kPseudoParent:
    case kPseudoPart:
    case kPseudoPastCue:
    case kPseudoPaused:
    case kPseudoPermissionGranted:
    case kPseudoPictureInPicture:
    case kPseudoPlaceholderShown:
    case kPseudoPlaying:
    case kPseudoPopoverInTopLayer:
    case kPseudoPopoverOpen:
    case kPseudoReadOnly:
    case kPseudoReadWrite:
    case kPseudoRelativeAnchor:
    case kPseudoRequired:
    case kPseudoRightPage:
    case kPseudoRoot:
    case kPseudoScope:
    case kPseudoSelectorFragmentAnchor:
    case kPseudoSingleButton:
    case kPseudoSlotted:
    case kPseudoSpatialNavigationFocus:
    case kPseudoStart:
    case kPseudoState:
    case kPseudoTarget:
    case kPseudoTargetCurrent:
    case kPseudoTargetBefore:
    case kPseudoTargetAfter:
    case kPseudoUnknown:
    case kPseudoUnparsed:
    case kPseudoUserInvalid:
    case kPseudoUserValid:
    case kPseudoValid:
    case kPseudoVertical:
    case kPseudoVideoPersistent:
    case kPseudoVideoPersistentAncestor:
    case kPseudoVisited:
    case kPseudoWebKitAutofill:
    case kPseudoWebKitCustomElement:
    case kPseudoWebkitAnyLink:
    case kPseudoWhere:
    case kPseudoWindowInactive:
    case kPseudoXrOverlay:
      return kPseudoIdNone;
  }

  NOTREACHED();
}

std::optional<CSSSelector> CSSSelector::Renest(StyleRule* new_parent) const {
  if (GetPseudoType() == CSSSelector::kPseudoParent &&
      data_.parent_rule_ != new_parent) {
    CSSSelector selector(*this);
    selector.data_.parent_rule_ = new_parent;
    return selector;
  } else if (HasRareData()) {
    // Handles cases where simple selectors hold an inner selector list,
    // e.g. :is(), :where(), :not().
    RareData* old_rare_data = data_.rare_data_;
    RareData* new_rare_data = old_rare_data->Renest(new_parent);
    if (old_rare_data != new_rare_data) {
      CSSSelector selector(*this);
      selector.data_.rare_data_ = new_rare_data;
      return selector;
    }
  }
  // Note that :scope (which isn't handled by any of the branches above)
  // does not need re-nesting, because it does not contain any reference
  // to a parent rule. The relationship between :scope, and the elements
  // matched by it, are instead handled dynamically at selector-matching time.

  return std::nullopt;
}

// Could be made smaller and faster by replacing pointer with an
// offset into a string buffer and making the bit fields smaller but
// that could not be maintained by hand.
struct NameToPseudoStruct {
  const char* string;
  unsigned type : 8;
};

// These tables must be kept sorted.
constexpr static NameToPseudoStruct kPseudoTypeWithoutArgumentsMap[] = {
    {"-internal-autofill-previewed", CSSSelector::kPseudoAutofillPreviewed},
    {"-internal-autofill-selected", CSSSelector::kPseudoAutofillSelected},
    {"-internal-dialog-in-top-layer", CSSSelector::kPseudoDialogInTopLayer},
    {"-internal-has-datalist", CSSSelector::kPseudoHasDatalist},
    {"-internal-is-html", CSSSelector::kPseudoIsHtml},
    {"-internal-list-box", CSSSelector::kPseudoListBox},
    {"-internal-media-controls-overlay-cast-button",
     CSSSelector::kPseudoWebKitCustomElement},
    {"-internal-menulist-popover-with-menubar-anchor",
     CSSSelector::kPseudoMenulistPopoverWithMenubarAnchor},
    {"-internal-menulist-popover-with-menulist-anchor",
     CSSSelector::kPseudoMenulistPopoverWithMenulistAnchor},
    {"-internal-multi-select-focus", CSSSelector::kPseudoMultiSelectFocus},
    {"-internal-overscroll-target", CSSSelector::kPseudoOverscrollTarget},
    {"-internal-popover-in-top-layer", CSSSelector::kPseudoPopoverInTopLayer},
    {"-internal-relative-anchor", CSSSelector::kPseudoRelativeAnchor},
    {"-internal-selector-fragment-anchor",
     CSSSelector::kPseudoSelectorFragmentAnchor},
    {"-internal-shadow-host-has-non-auto-appearance",
     CSSSelector::kPseudoHostHasNonAutoAppearance},
    {"-internal-spatial-navigation-focus",
     CSSSelector::kPseudoSpatialNavigationFocus},
    {"-internal-video-persistent", CSSSelector::kPseudoVideoPersistent},
    {"-internal-video-persistent-ancestor",
     CSSSelector::kPseudoVideoPersistentAncestor},
    {"-webkit-any-link", CSSSelector::kPseudoWebkitAnyLink},
    {"-webkit-autofill", CSSSelector::kPseudoWebKitAutofill},
    {"-webkit-drag", CSSSelector::kPseudoDrag},
    {"-webkit-full-page-media", CSSSelector::kPseudoFullPageMedia},
    {"-webkit-full-screen", CSSSelector::kPseudoFullScreen},
    {"-webkit-full-screen-ancestor", CSSSelector::kPseudoFullScreenAncestor},
    {"-webkit-resizer", CSSSelector::kPseudoResizer},
    {"-webkit-scrollbar", CSSSelector::kPseudoScrollbar},
    {"-webkit-scrollbar-button", CSSSelector::kPseudoScrollbarButton},
    {"-webkit-scrollbar-corner", CSSSelector::kPseudoScrollbarCorner},
    {"-webkit-scrollbar-thumb", CSSSelector::kPseudoScrollbarThumb},
    {"-webkit-scrollbar-track", CSSSelector::kPseudoScrollbarTrack},
    {"-webkit-scrollbar-track-piece", CSSSelector::kPseudoScrollbarTrackPiece},
    {"active", CSSSelector::kPseudoActive},
    {"active-view-transition", CSSSelector::kPseudoActiveViewTransition},
    {"after", CSSSelector::kPseudoAfter},
    {"any-link", CSSSelector::kPseudoAnyLink},
    {"autofill", CSSSelector::kPseudoAutofill},
    {"backdrop", CSSSelector::kPseudoBackdrop},
    {"before", CSSSelector::kPseudoBefore},
    {"checked", CSSSelector::kPseudoChecked},
    {"checkmark", CSSSelector::kPseudoCheckMark},
    {"column", CSSSelector::kPseudoColumn},
    {"corner-present", CSSSelector::kPseudoCornerPresent},
    {"cue", CSSSelector::kPseudoWebKitCustomElement},
    {"current", CSSSelector::kPseudoCurrent},
    {"decrement", CSSSelector::kPseudoDecrement},
    {"default", CSSSelector::kPseudoDefault},
    {"defined", CSSSelector::kPseudoDefined},
    {"details-content", CSSSelector::kPseudoDetailsContent},
    {"disabled", CSSSelector::kPseudoDisabled},
    {"double-button", CSSSelector::kPseudoDoubleButton},
    {"empty", CSSSelector::kPseudoEmpty},
    {"enabled", CSSSelector::kPseudoEnabled},
    {"end", CSSSelector::kPseudoEnd},
    {"file-selector-button", CSSSelector::kPseudoFileSelectorButton},
    {"first", CSSSelector::kPseudoFirstPage},
    {"first-child", CSSSelector::kPseudoFirstChild},
    {"first-letter", CSSSelector::kPseudoFirstLetter},
    {"first-line", CSSSelector::kPseudoFirstLine},
    {"first-of-type", CSSSelector::kPseudoFirstOfType},
    {"focus", CSSSelector::kPseudoFocus},
    {"focus-visible", CSSSelector::kPseudoFocusVisible},
    {"focus-within", CSSSelector::kPseudoFocusWithin},
    {"fullscreen", CSSSelector::kPseudoFullscreen},
    {"future", CSSSelector::kPseudoFutureCue},
    {"grammar-error", CSSSelector::kPseudoGrammarError},
    {"granted", CSSSelector::kPseudoPermissionGranted},
    {"has-slotted", CSSSelector::kPseudoHasSlotted},
    {"horizontal", CSSSelector::kPseudoHorizontal},
    {"host", CSSSelector::kPseudoHost},
    {"hover", CSSSelector::kPseudoHover},
    {"in-range", CSSSelector::kPseudoInRange},
    {"increment", CSSSelector::kPseudoIncrement},
    {"indeterminate", CSSSelector::kPseudoIndeterminate},
    {"interest-hint", CSSSelector::kPseudoInterestHint},
    {"interest-source", CSSSelector::kPseudoInterestSource},
    {"interest-target", CSSSelector::kPseudoInterestTarget},
    {"invalid", CSSSelector::kPseudoInvalid},
    {"last-child", CSSSelector::kPseudoLastChild},
    {"last-of-type", CSSSelector::kPseudoLastOfType},
    {"left", CSSSelector::kPseudoLeftPage},
    {"link", CSSSelector::kPseudoLink},
    {"marker", CSSSelector::kPseudoMarker},
    {"modal", CSSSelector::kPseudoModal},
    {"no-button", CSSSelector::kPseudoNoButton},
    {"only-child", CSSSelector::kPseudoOnlyChild},
    {"only-of-type", CSSSelector::kPseudoOnlyOfType},
    {"open", CSSSelector::kPseudoOpen},
    {"optional", CSSSelector::kPseudoOptional},
    {"out-of-range", CSSSelector::kPseudoOutOfRange},
    {"past", CSSSelector::kPseudoPastCue},
    {"paused", CSSSelector::kPseudoPaused},
    {"permission-icon", CSSSelector::kPseudoPermissionIcon},
    {"picker-icon", CSSSelector::kPseudoPickerIcon},
    {"picture-in-picture", CSSSelector::kPseudoPictureInPicture},
    {"placeholder", CSSSelector::kPseudoPlaceholder},
    {"placeholder-shown", CSSSelector::kPseudoPlaceholderShown},
    {"playing", CSSSelector::kPseudoPlaying},
    {"popover-open", CSSSelector::kPseudoPopoverOpen},
    {"read-only", CSSSelector::kPseudoReadOnly},
    {"read-write", CSSSelector::kPseudoReadWrite},
    {"required", CSSSelector::kPseudoRequired},
    {"right", CSSSelector::kPseudoRightPage},
    {"root", CSSSelector::kPseudoRoot},
    {"scope", CSSSelector::kPseudoScope},
    {"scroll-marker", CSSSelector::kPseudoScrollMarker},
    {"scroll-marker-group", CSSSelector::kPseudoScrollMarkerGroup},
    {"search-text", CSSSelector::kPseudoSearchText},
    {"selection", CSSSelector::kPseudoSelection},
    {"single-button", CSSSelector::kPseudoSingleButton},
    {"spelling-error", CSSSelector::kPseudoSpellingError},
    {"start", CSSSelector::kPseudoStart},
    {"target", CSSSelector::kPseudoTarget},
    {"target-after", CSSSelector::kPseudoTargetAfter},
    {"target-before", CSSSelector::kPseudoTargetBefore},
    {"target-current", CSSSelector::kPseudoTargetCurrent},
    {"target-text", CSSSelector::kPseudoTargetText},
    {"user-invalid", CSSSelector::kPseudoUserInvalid},
    {"user-valid", CSSSelector::kPseudoUserValid},
    {"valid", CSSSelector::kPseudoValid},
    {"vertical", CSSSelector::kPseudoVertical},
    {"view-transition", CSSSelector::kPseudoViewTransition},
    {"visited", CSSSelector::kPseudoVisited},
    {"window-inactive", CSSSelector::kPseudoWindowInactive},
    {"xr-overlay", CSSSelector::kPseudoXrOverlay},
};

constexpr static NameToPseudoStruct kPseudoTypeWithArgumentsMap[] = {
    {"-internal-overscroll-area-parent",
     CSSSelector::kPseudoOverscrollAreaParent},
    {"-webkit-any", CSSSelector::kPseudoAny},
    {"active-view-transition-type",
     CSSSelector::kPseudoActiveViewTransitionType},
    {"cue", CSSSelector::kPseudoCue},
    {"dir", CSSSelector::kPseudoDir},
    {"has", CSSSelector::kPseudoHas},
    {"highlight", CSSSelector::kPseudoHighlight},
    {"host", CSSSelector::kPseudoHost},
    {"host-context", CSSSelector::kPseudoHostContext},
    {"is", CSSSelector::kPseudoIs},
    {"lang", CSSSelector::kPseudoLang},
    {"link-to", CSSSelector::kPseudoLinkTo},
    {"not", CSSSelector::kPseudoNot},
    {"nth-child", CSSSelector::kPseudoNthChild},
    {"nth-last-child", CSSSelector::kPseudoNthLastChild},
    {"nth-last-of-type", CSSSelector::kPseudoNthLastOfType},
    {"nth-of-type", CSSSelector::kPseudoNthOfType},
    {"part", CSSSelector::kPseudoPart},
    {"picker", CSSSelector::kPseudoPicker},
    {"scroll-button", CSSSelector::kPseudoScrollButton},
    {"slotted", CSSSelector::kPseudoSlotted},
    {"state", CSSSelector::kPseudoState},
    {"view-transition-group", CSSSelector::kPseudoViewTransitionGroup},
    {"view-transition-group-children",
     CSSSelector::kPseudoViewTransitionGroupChildren},
    {"view-transition-image-pair", CSSSelector::kPseudoViewTransitionImagePair},
    {"view-transition-new", CSSSelector::kPseudoViewTransitionNew},
    {"view-transition-old", CSSSelector::kPseudoViewTransitionOld},
    {"where", CSSSelector::kPseudoWhere},
};

// TODO(sesse): This function should probably be gperf-generated, like
// everything else converting strings to enums, instead of hand-coded.
CSSSelector::PseudoType CSSSelector::NameToPseudoType(
    StringView name,
    bool has_arguments,
    const Document* document) {
  if (name.IsNull()) {
    return CSSSelector::kPseudoUnknown;
  }
  if (!name.Is8Bit()) {
    Vector<LChar, 50> latin1_name;
    for (UChar ch : name.Span16()) {
      if (ch > 0xFF) {
        return CSSSelector::kPseudoUnknown;
      }
      latin1_name.push_back(ch);
    }
    return NameToPseudoType(StringView(base::span(latin1_name)), has_arguments,
                            document);
  }

  const NameToPseudoStruct* pseudo_type_map;
  const NameToPseudoStruct* pseudo_type_map_end;
  if (has_arguments) {
    pseudo_type_map = std::begin(kPseudoTypeWithArgumentsMap);
    pseudo_type_map_end = std::end(kPseudoTypeWithArgumentsMap);
  } else {
    pseudo_type_map = std::begin(kPseudoTypeWithoutArgumentsMap);
    pseudo_type_map_end = std::end(kPseudoTypeWithoutArgumentsMap);
  }
  DCHECK(name.Is8Bit());
  std::string_view latin1_name = base::as_string_view(name.Span8());
  const NameToPseudoStruct* match =
      std::lower_bound(pseudo_type_map, pseudo_type_map_end, latin1_name,
                       [](const NameToPseudoStruct& entry,
                          const std::string_view& latin1_name) -> bool {
                         DCHECK(entry.string);
                         return std::string_view(entry.string) < latin1_name;
                       });
  if (match == pseudo_type_map_end || match->string != name) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoPaused &&
      !RuntimeEnabledFeatures::CSSPseudoPlayingPausedEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoPlaying &&
      !RuntimeEnabledFeatures::CSSPseudoPlayingPausedEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoPermissionGranted &&
      !RuntimeEnabledFeatures::PermissionElementEnabled(
          document ? document->GetExecutionContext() : nullptr) &&
      !RuntimeEnabledFeatures::GeolocationElementEnabled(
          document ? document->GetExecutionContext() : nullptr) &&
      !RuntimeEnabledFeatures::UserMediaElementEnabled(
          document ? document->GetExecutionContext() : nullptr)) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoTargetCurrent &&
      !RuntimeEnabledFeatures::CSSPseudoScrollMarkersEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if ((match->type == CSSSelector::kPseudoTargetBefore ||
       match->type == CSSSelector::kPseudoTargetAfter) &&
      !RuntimeEnabledFeatures::CSSScrollMarkerTargetBeforeAfterEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if ((match->type == CSSSelector::kPseudoScrollMarker ||
       match->type == CSSSelector::kPseudoScrollMarkerGroup) &&
      !RuntimeEnabledFeatures::CSSPseudoScrollMarkersEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoScrollButton &&
      !RuntimeEnabledFeatures::CSSPseudoScrollButtonsEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoColumn &&
      !RuntimeEnabledFeatures::CSSPseudoColumnEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if ((match->type == CSSSelector::kPseudoSearchText ||
       match->type == CSSSelector::kPseudoCurrent) &&
      !RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if ((match->type == CSSSelector::kPseudoInterestSource ||
       match->type == CSSSelector::kPseudoInterestTarget) &&
      !RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoHasSlotted &&
      !RuntimeEnabledFeatures::CSSPseudoHasSlottedEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoOverscrollAreaParent &&
      !RuntimeEnabledFeatures::CSSOverscrollGesturesEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  return static_cast<CSSSelector::PseudoType>(match->type);
}

#if DCHECK_IS_ON()
void CSSSelector::Show(int indent) const {
  printf("%*sSelectorText(): %s\n", indent, "", SelectorText().Ascii().c_str());
  printf("%*smatch_: %d\n", indent, "", Match());
  if (Match() != kTag && Match() != kUniversalTag && !IsPseudoParent()) {
    printf("%*sValue(): %s\n", indent, "", Value().Ascii().c_str());
  }
  printf("%*sGetPseudoType(): %d\n", indent, "", GetPseudoType());
  if (Match() == kTag && Match() != kUniversalTag) {
    printf("%*sTagQName().LocalName(): %s\n", indent, "",
           TagQName().LocalName().Ascii().c_str());
  }
  printf("%*sIsAttributeSelector(): %d\n", indent, "", IsAttributeSelector());
  if (IsAttributeSelector()) {
    printf("%*sAttribute(): %s\n", indent, "",
           Attribute().LocalName().Ascii().c_str());
  }
  printf("%*sArgument(): %s\n", indent, "", Argument().Ascii().c_str());
  printf("%*sSpecificity(): %u\n", indent, "", Specificity());
  if (NextSimpleSelector()) {
    printf("\n%*s--> (Relation() == %d)\n", indent, "", Relation());
    NextSimpleSelector()->Show(indent + 2);
  } else {
    printf("\n%*s--> (Relation() == %d)\n", indent, "", Relation());
  }
}

void CSSSelector::Show() const {
  printf("\n******* CSSSelector::Show(\"%s\") *******\n",
         SelectorText().Ascii().c_str());
  Show(2);
  printf("******* end *******\n");
}
#endif  // DCHECK_IS_ON()

void CSSSelector::UpdatePseudoPage(const AtomicString& value,
                                   const Document* document) {
  DCHECK_EQ(Match(), kPagePseudoClass);
  SetValue(value);
  PseudoType type = CSSSelectorParser::ParsePseudoType(value, false, document);
  if (type != kPseudoFirstPage && type != kPseudoLeftPage &&
      type != kPseudoRightPage) {
    type = kPseudoUnknown;
  }
  bits_.set<PseudoTypeField>(type);
}

void CSSSelector::UpdatePseudoType(const AtomicString& value,
                                   const CSSParserContext& context,
                                   bool has_arguments,
                                   CSSParserMode mode) {
  DCHECK(Match() == kPseudoClass || Match() == kPseudoElement);
  AtomicString lower_value = value.LowerASCII();
  PseudoType pseudo_type = CSSSelectorParser::ParsePseudoType(
      lower_value, has_arguments, context.GetDocument());
  SetPseudoType(pseudo_type);
  SetValue(lower_value);

  switch (GetPseudoType()) {
    case kPseudoAfter:
    case kPseudoBefore:
    case kPseudoFirstLetter:
    case kPseudoFirstLine:
      // The spec says some pseudos allow both single and double colons like
      // :before for backwards compatibility. Single colon becomes PseudoClass,
      // but should be PseudoElement like double colon.
      if (Match() == kPseudoClass) {
        bits_.set<MatchField>(kPseudoElement);
      }
      [[fallthrough]];
    // For pseudo-elements
    case kPseudoPickerIcon:
    case kPseudoInterestHint:
    case kPseudoCheckMark:
    case kPseudoBackdrop:
    case kPseudoCue:
    case kPseudoMarker:
    case kPseudoPart:
    case kPseudoPlaceholder:
    case kPseudoFileSelectorButton:
    case kPseudoResizer:
    case kPseudoScrollbar:
    case kPseudoScrollbarCorner:
    case kPseudoScrollbarButton:
    case kPseudoScrollbarThumb:
    case kPseudoScrollbarTrack:
    case kPseudoScrollbarTrackPiece:
    case kPseudoScrollMarker:
    case kPseudoScrollMarkerGroup:
    case kPseudoScrollButton:
    case kPseudoColumn:
    case kPseudoPicker:
    case kPseudoSelection:
    case kPseudoWebKitCustomElement:
    case kPseudoSlotted:
    case kPseudoSearchText:
    case kPseudoTargetText:
    case kPseudoHighlight:
    case kPseudoSpellingError:
    case kPseudoGrammarError:
    case kPseudoViewTransition:
    case kPseudoViewTransitionGroup:
    case kPseudoViewTransitionGroupChildren:
    case kPseudoViewTransitionImagePair:
    case kPseudoViewTransitionOld:
    case kPseudoViewTransitionNew:
    case kPseudoDetailsContent:
      if (Match() != kPseudoElement) {
        bits_.set<PseudoTypeField>(kPseudoUnknown);
      }
      break;
    case kPseudoPermissionIcon:
      if (Match() != kPseudoElement) {
        bits_.set<PseudoTypeField>(kPseudoUnknown);
      }
      break;
    case kPseudoOverscrollAreaParent:
    case kPseudoBlinkInternalElement:
      if (Match() != kPseudoElement || mode != kUASheetMode) {
        bits_.set<PseudoTypeField>(kPseudoUnknown);
      }
      break;
    case kPseudoHasDatalist:
    case kPseudoHostHasNonAutoAppearance:
    case kPseudoIsHtml:
    case kPseudoListBox:
    case kPseudoMultiSelectFocus:
    case kPseudoSpatialNavigationFocus:
    case kPseudoVideoPersistent:
    case kPseudoVideoPersistentAncestor:
      if (mode != kUASheetMode) {
        bits_.set<PseudoTypeField>(kPseudoUnknown);
        break;
      }
      [[fallthrough]];
    // For pseudo-classes
    case kPseudoActive:
    case kPseudoActiveViewTransition:
    case kPseudoActiveViewTransitionType:
    case kPseudoAny:
    case kPseudoAnyLink:
    case kPseudoAutofill:
    case kPseudoAutofillPreviewed:
    case kPseudoAutofillSelected:
    case kPseudoChecked:
    case kPseudoCornerPresent:
    case kPseudoCurrent:
    case kPseudoDecrement:
    case kPseudoDefault:
    case kPseudoDefined:
    case kPseudoDialogInTopLayer:
    case kPseudoDir:
    case kPseudoDisabled:
    case kPseudoDoubleButton:
    case kPseudoDrag:
    case kPseudoEmpty:
    case kPseudoEnabled:
    case kPseudoEnd:
    case kPseudoFirstChild:
    case kPseudoFirstOfType:
    case kPseudoFocus:
    case kPseudoFocusVisible:
    case kPseudoFocusWithin:
    case kPseudoFullPageMedia:
    case kPseudoFullScreen:
    case kPseudoFullScreenAncestor:
    case kPseudoFullscreen:
    case kPseudoFutureCue:
    case kPseudoHas:
    case kPseudoHasSlotted:
    case kPseudoHorizontal:
    case kPseudoHost:
    case kPseudoHostContext:
    case kPseudoHover:
    case kPseudoInRange:
    case kPseudoIncrement:
    case kPseudoIndeterminate:
    case kPseudoInterestSource:
    case kPseudoInterestTarget:
    case kPseudoInvalid:
    case kPseudoIs:
    case kPseudoLang:
    case kPseudoLastChild:
    case kPseudoLastOfType:
    case kPseudoLink:
    case kPseudoLinkTo:
    case kPseudoMenulistPopoverWithMenubarAnchor:
    case kPseudoMenulistPopoverWithMenulistAnchor:
    case kPseudoModal:
    case kPseudoNoButton:
    case kPseudoNot:
    case kPseudoNthChild:
    case kPseudoNthLastChild:
    case kPseudoNthLastOfType:
    case kPseudoNthOfType:
    case kPseudoOnlyChild:
    case kPseudoOnlyOfType:
    case kPseudoOpen:
    case kPseudoOptional:
    case kPseudoOutOfRange:
    case kPseudoOverscrollTarget:
    case kPseudoParent:
    case kPseudoPastCue:
    case kPseudoPaused:
    case kPseudoPermissionGranted:
    case kPseudoPictureInPicture:
    case kPseudoPlaceholderShown:
    case kPseudoPlaying:
    case kPseudoPopoverInTopLayer:
    case kPseudoPopoverOpen:
    case kPseudoReadOnly:
    case kPseudoReadWrite:
    case kPseudoRelativeAnchor:
    case kPseudoRequired:
    case kPseudoRoot:
    case kPseudoScope:
    case kPseudoSelectorFragmentAnchor:
    case kPseudoSingleButton:
    case kPseudoStart:
    case kPseudoState:
    case kPseudoTarget:
    case kPseudoTargetCurrent:
    case kPseudoTargetBefore:
    case kPseudoTargetAfter:
    case kPseudoUnknown:
    case kPseudoUnparsed:
    case kPseudoUserInvalid:
    case kPseudoUserValid:
    case kPseudoValid:
    case kPseudoVertical:
    case kPseudoVisited:
    case kPseudoWebKitAutofill:
    case kPseudoWebkitAnyLink:
    case kPseudoWhere:
    case kPseudoWindowInactive:
    case kPseudoXrOverlay:
      if (Match() != kPseudoClass) {
        bits_.set<PseudoTypeField>(kPseudoUnknown);
      }
      break;
    case kPseudoFirstPage:
    case kPseudoLeftPage:
    case kPseudoRightPage:
      bits_.set<PseudoTypeField>(kPseudoUnknown);
      break;
  }
}

void CSSSelector::SetUnparsedPlaceholder(CSSNestingType unparsed_nesting_type,
                                         const AtomicString& value) {
  DCHECK(Match() == kPseudoClass);
  SetPseudoType(kPseudoUnparsed);
  CreateRareData();
  SetValue(value);
  data_.rare_data_->bits_.unparsed_nesting_type_ = unparsed_nesting_type;
}

CSSNestingType CSSSelector::GetNestingType() const {
  switch (GetPseudoType()) {
    case CSSSelector::kPseudoParent:
      return CSSNestingType::kNesting;
    case CSSSelector::kPseudoUnparsed:
      return data_.rare_data_->bits_.unparsed_nesting_type_;
    case CSSSelector::kPseudoScope:
      // TODO(crbug.com/1280240): Handle unparsed :scope.
      return CSSNestingType::kScope;
    default:
      return CSSNestingType::kNone;
  }
}

void CSSSelector::SetWhere(CSSSelectorList* selector_list) {
  SetMatch(kPseudoClass);
  SetPseudoType(kPseudoWhere);
  SetSelectorList(selector_list);
}

static void SerializeIdentifierOrAny(const AtomicString& identifier,
                                     const AtomicString& any,
                                     StringBuilder& builder) {
  if (identifier != any) {
    SerializeIdentifier(identifier, builder);
  } else {
    builder.Append(g_star_atom);
  }
}

static void SerializeNamespacePrefixIfNeeded(const AtomicString& prefix,
                                             const AtomicString& any,
                                             StringBuilder& builder,
                                             bool is_attribute_selector) {
  if (prefix.IsNull() || (prefix.empty() && is_attribute_selector)) {
    return;
  }
  SerializeIdentifierOrAny(prefix, any, builder);
  builder.Append('|');
}

template <typename ListType>
static void SerializeIdentifierList(StringBuilder& builder,
                                    const ListType& list) {
  bool is_first = true;
  for (const AtomicString& item : list) {
    if (!is_first) {
      builder.Append(", ");
    }
    SerializeIdentifier(item, builder);
    is_first = false;
  }
}

// static
template <bool expand_pseudo_references>
void CSSSelector::SerializeSelectorList(const CSSSelectorList* selector_list,
                                        StringBuilder& builder,
                                        uintptr_t scope_id) {
  const CSSSelector* first_sub_selector = selector_list->First();
  for (const CSSSelector* sub_selector = first_sub_selector; sub_selector;
       sub_selector = CSSSelectorList::Next(*sub_selector)) {
    if (sub_selector != first_sub_selector) {
      builder.Append(", ");
    }
    builder.Append(
        sub_selector->SelectorTextInternal<expand_pseudo_references>(scope_id));
  }
}

String CSSSelector::SelectorText() const {
  // The value of `scope_id` does not matter when
  // `expand_pseudo_references` is `false`.
  return SelectorTextInternal<!kExpandPseudoReferences>(/*scope_id=*/0);
}

String CSSSelector::SelectorTextExpandingPseudoReferences(
    uintptr_t scope_id) const {
  return SelectorTextInternal<kExpandPseudoReferences>(scope_id);
}

template <bool expand_pseudo_references>
void CSSSelector::SerializeSimpleSelector(StringBuilder& builder,
                                          uintptr_t scope_id) const {
  bool suppress_selector_list = false;
  if ((Match() == kTag || Match() == kUniversalTag) && !IsImplicit()) {
    SerializeNamespacePrefixIfNeeded(TagQName().Prefix(), g_star_atom, builder,
                                     IsAttributeSelector());
    SerializeIdentifierOrAny(TagQName().LocalName(), UniversalSelectorAtom(),
                             builder);
  } else if (Match() == kId) {
    builder.Append('#');
    SerializeIdentifier(SerializingValue(), builder);
  } else if (Match() == kClass) {
    builder.Append('.');
    SerializeIdentifier(SerializingValue(), builder);
  } else if (Match() == kPseudoClass || Match() == kPagePseudoClass) {
    if (GetPseudoType() == kPseudoUnparsed) {
      builder.Append(Value());
    } else if (GetPseudoType() != kPseudoParent &&
               GetPseudoType() != kPseudoScope) {
      builder.Append(':');
      builder.Append(SerializingValue());
    }

    switch (GetPseudoType()) {
      case kPseudoNthChild:
      case kPseudoNthLastChild:
      case kPseudoNthOfType:
      case kPseudoNthLastOfType: {
        builder.Append('(');

        // https://drafts.csswg.org/css-syntax/#serializing-anb
        int a = data_.rare_data_->NthAValue();
        int b = data_.rare_data_->NthBValue();
        if (a == 0) {
          builder.Append(String::Number(b));
        } else {
          if (a == 1) {
            builder.Append('n');
          } else if (a == -1) {
            builder.Append("-n");
          } else {
            builder.AppendFormat("%dn", a);
          }

          if (b < 0) {
            builder.Append(String::Number(b));
          } else if (b > 0) {
            builder.AppendFormat("+%d", b);
          }
        }

        // Only relevant for :nth-child, not :nth-of-type.
        if (data_.rare_data_->selector_list_ != nullptr) {
          builder.Append(" of ");
          SerializeSelectorList<expand_pseudo_references>(
              data_.rare_data_->selector_list_, builder, scope_id);
          suppress_selector_list = true;
        }

        builder.Append(')');
        break;
      }
      case kPseudoDir:
      case kPseudoState:
        builder.Append('(');
        SerializeIdentifier(Argument(), builder);
        builder.Append(')');
        break;
      case kPseudoLang: {
        builder.Append('(');
        SerializeIdentifierList(builder, *ArgumentList());
        builder.Append(')');
        break;
      }
      case kPseudoHas:
      case kPseudoNot:
        DCHECK(SelectorList());
        break;
      case kPseudoHost:
      case kPseudoHostContext:
      case kPseudoAny:
      case kPseudoIs:
      case kPseudoWhere:
        break;
      case kPseudoParent:
        if constexpr (expand_pseudo_references) {
          // Replace parent pseudo with equivalent :is() pseudo.
          builder.Append(":is");
          if (auto* parent = SelectorListOrParent()) {
            builder.Append('(');
            builder.Append(
                parent->SelectorTextExpandingPseudoReferences(scope_id));
            builder.Append(')');
          }
        } else {
          builder.Append('&');
        }
        break;
      case kPseudoScope:
        if constexpr (expand_pseudo_references) {
          builder.Append(":-internal-scope-");
          builder.AppendNumber(scope_id);
        } else {
          builder.Append(':');
          builder.Append(SerializingValue());
        }
        break;
      case kPseudoRelativeAnchor:
        NOTREACHED();
      case kPseudoActiveViewTransitionType: {
        CHECK(!IdentList().empty());
        builder.Append('(');
        SerializeIdentifierList(builder, IdentList());
        builder.Append(')');
        break;
      }
      case kPseudoLinkTo: {
        DCHECK(GetLinkCondition());
        builder.Append("(");
        GetLinkCondition()->SerializeTo(builder);
        builder.Append(")");
        break;
      }
      default:
        break;
    }
  } else if (Match() == kPseudoElement) {
    builder.Append("::");
    SerializeIdentifier(SerializingValue(), builder);
    switch (GetPseudoType()) {
      case kPseudoPart: {
        char separator = '(';
        for (AtomicString part : IdentList()) {
          builder.Append(separator);
          if (separator == '(') {
            separator = ' ';
          }
          SerializeIdentifier(part, builder);
        }
        builder.Append(')');
        break;
      }
      case kPseudoHighlight: {
        builder.Append('(');
        builder.Append(Argument());
        builder.Append(')');
        break;
      }
      case kPseudoScrollButton: {
        builder.Append('(');
        builder.Append(Argument());
        builder.Append(')');
        break;
      }
      case kPseudoViewTransitionGroup:
      case kPseudoViewTransitionGroupChildren:
      case kPseudoViewTransitionImagePair:
      case kPseudoViewTransitionNew:
      case kPseudoViewTransitionOld: {
        builder.Append('(');
        bool first = true;
        for (const AtomicString& name_or_class : IdentList()) {
          if (!first) {
            builder.Append('.');
          }

          first = false;
          if (name_or_class == UniversalSelectorAtom()) {
            builder.Append(g_star_atom);
          } else {
            SerializeIdentifier(name_or_class, builder);
          }
        }
        builder.Append(')');
        break;
      }
      case kPseudoOverscrollAreaParent:
        builder.Append('(');
        builder.Append(Argument());
        builder.Append(')');
        break;
      default:
        break;
    }
  } else if (IsAttributeSelector()) {
    builder.Append('[');
    SerializeNamespacePrefixIfNeeded(Attribute().Prefix(), g_star_atom, builder,
                                     IsAttributeSelector());
    SerializeIdentifier(Attribute().LocalName(), builder);
    switch (Match()) {
      case kAttributeExact:
        builder.Append('=');
        break;
      case kAttributeSet:
        // set has no operator or value, just the attrName
        builder.Append(']');
        break;
      case kAttributeList:
        builder.Append("~=");
        break;
      case kAttributeHyphen:
        builder.Append("|=");
        break;
      case kAttributeBegin:
        builder.Append("^=");
        break;
      case kAttributeEnd:
        builder.Append("$=");
        break;
      case kAttributeContain:
        builder.Append("*=");
        break;
      default:
        break;
    }
    if (Match() != kAttributeSet) {
      SerializeString(SerializingValue(), builder);
      if (AttributeMatch() == AttributeMatchType::kCaseInsensitive) {
        builder.Append(" i");
      } else if (AttributeMatch() == AttributeMatchType::kCaseSensitiveAlways) {
        DCHECK(RuntimeEnabledFeatures::CSSCaseSensitiveSelectorEnabled());
        builder.Append(" s");
      }
      builder.Append(']');
    }
  }

  if (SelectorList() && !suppress_selector_list) {
    builder.Append('(');
    SerializeSelectorList<expand_pseudo_references>(SelectorList(), builder,
                                                    scope_id);
    builder.Append(')');
  }
}

template <bool expand_pseudo_references>
const CSSSelector* CSSSelector::SerializeCompound(StringBuilder& builder,
                                                  uintptr_t scope_id) const {
  for (const CSSSelector* simple_selector = this; simple_selector;
       simple_selector = simple_selector->NextSimpleSelector()) {
    simple_selector->SerializeSimpleSelector<expand_pseudo_references>(
        builder, scope_id);
    if (simple_selector->Relation() != kSubSelector) {
      return simple_selector;
    }
  }
  return nullptr;
}

template <bool expand_pseudo_references>
String CSSSelector::SelectorTextInternal(uintptr_t scope_id) const {
  String result;
  for (const CSSSelector* compound = this; compound;
       compound = compound->NextSimpleSelector()) {
    StringBuilder builder;
    compound = compound->SerializeCompound<expand_pseudo_references>(builder,
                                                                     scope_id);
    if (!compound) {
      return StrCat({builder.ReleaseString(), result});
    }

    RelationType relation = compound->Relation();
    DCHECK_NE(relation, kSubSelector);

    const CSSSelector* next_compound = compound->NextSimpleSelector();
    DCHECK(next_compound);

    // If we are combining with an implicit :scope, it is as if we
    // used a relative combinator. However, when expand_pseudo_references=true,
    // we must serialize the implicit compound anyway, since the :has() cache
    // needs this to be a part of the key.
    bool implicit = next_compound->IsImplicit() && !expand_pseudo_references;
    if (!next_compound ||
        (next_compound->Match() == kPseudoClass &&
         next_compound->GetPseudoType() == kPseudoScope && implicit)) {
      relation = ConvertRelationToRelative(relation);
    }

    switch (relation) {
      case kDescendant:
        result = StrCat({" ", builder.ReleaseString(), result});
        break;
      case kChild:
        result = StrCat({" > ", builder.ReleaseString(), result});
        break;
      case kDirectAdjacent:
        result = StrCat({" + ", builder.ReleaseString(), result});
        break;
      case kIndirectAdjacent:
        result = StrCat({" ~ ", builder.ReleaseString(), result});
        break;
      case kSubSelector:
      case kPseudoChild:
      case kShadowPart:
      case kUAShadow:
      case kShadowSlot:
        result = StrCat({builder.ReleaseString(), result});
        break;
      case kRelativeDescendant:
        return StrCat({builder.ReleaseString(), result});
      case kRelativeChild:
        return StrCat({"> ", builder.ReleaseString(), result});
      case kRelativeDirectAdjacent:
        return StrCat({"+ ", builder.ReleaseString(), result});
      case kRelativeIndirectAdjacent:
        return StrCat({"~ ", builder.ReleaseString(), result});
    }
  }
  NOTREACHED();
}

String CSSSelector::SimpleSelectorTextForDebug() const {
  StringBuilder builder;
  // `scope_id` is ignored when `expand_pseudo_references` is false.
  SerializeSimpleSelector<!kExpandPseudoReferences>(builder, /*scope_id=*/0);
  return builder.ToString();
}

void CSSSelector::SetArgument(const AtomicString& value) {
  CreateRareData();
  data_.rare_data_->argument_ = value;
}

void CSSSelector::SetArgumentList(
    std::unique_ptr<Vector<AtomicString>> arguments) {
  CreateRareData();
  data_.rare_data_->argument_list_ = std::move(arguments);
}

void CSSSelector::SetSelectorList(CSSSelectorList* selector_list) {
  CreateRareData();
  data_.rare_data_->selector_list_ = selector_list;
}

void CSSSelector::SetLinkCondition(LinkCondition* condition) {
  CreateRareData();
  data_.rare_data_->link_condition_ = condition;
}

void CSSSelector::SetContainsPseudoInsideHasPseudoClass() {
  CreateRareData();
  data_.rare_data_->bits_.has_.contains_pseudo_ = true;
}

void CSSSelector::SetContainsComplexLogicalCombinationsInsideHasPseudoClass() {
  CreateRareData();
  data_.rare_data_->bits_.has_.contains_complex_logical_combinations_ = true;
}

void CSSSelector::SetHasArgumentMatchInShadowTree() {
  CreateRareData();
  data_.rare_data_->bits_.has_.argument_match_in_shadow_tree_ = true;
}

static bool ValidateSubSelector(const CSSSelector* selector) {
  switch (selector->Match()) {
    case CSSSelector::kTag:
    case CSSSelector::kUniversalTag:
    case CSSSelector::kId:
    case CSSSelector::kClass:
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeHyphen:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
      return true;
    case CSSSelector::kPseudoElement:
    case CSSSelector::kUnknown:
      return false;
    case CSSSelector::kPagePseudoClass:
    case CSSSelector::kPseudoClass:
      break;
    case CSSSelector::kInvalidList:
      NOTREACHED();
  }

  switch (selector->GetPseudoType()) {
    case CSSSelector::kPseudoEmpty:
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoTarget:
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoChecked:
    case CSSSelector::kPseudoIndeterminate:
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastOfType:
    case CSSSelector::kPseudoFirstChild:
    case CSSSelector::kPseudoLastChild:
    case CSSSelector::kPseudoFirstOfType:
    case CSSSelector::kPseudoLastOfType:
    case CSSSelector::kPseudoOnlyOfType:
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
    case CSSSelector::kPseudoNot:
    case CSSSelector::kPseudoSpatialNavigationFocus:
    case CSSSelector::kPseudoHasDatalist:
    case CSSSelector::kPseudoIsHtml:
    case CSSSelector::kPseudoListBox:
    case CSSSelector::kPseudoHostHasNonAutoAppearance:
      // TODO(https://crbug.com/1346456): Many pseudos should probably be
      // added to this list.  The default: case below should also be removed
      // so that those adding new pseudos know they need to choose one path or
      // the other here.
      //
      // However, it's not clear why a pseudo should be in one list or the
      // other.  It's also entirely possible that this entire switch() should
      // be removed and all cases should return true.
      return true;
    default:
      return false;
  }
}

bool CSSSelector::IsCompound() const {
  if (!ValidateSubSelector(this)) {
    return false;
  }

  const CSSSelector* prev_sub_selector = this;
  const CSSSelector* sub_selector = NextSimpleSelector();

  while (sub_selector) {
    if (prev_sub_selector->Relation() != kSubSelector) {
      return false;
    }
    if (!ValidateSubSelector(sub_selector)) {
      return false;
    }

    prev_sub_selector = sub_selector;
    sub_selector = sub_selector->NextSimpleSelector();
  }

  return true;
}

bool CSSSelector::HasLinkOrVisited() const {
  for (const CSSSelector* current = this; current;
       current = current->NextSimpleSelector()) {
    CSSSelector::PseudoType pseudo = current->GetPseudoType();
    if (pseudo == CSSSelector::kPseudoLink ||
        pseudo == CSSSelector::kPseudoVisited) {
      return true;
    }
    for (const CSSSelector* sub_selector = current->SelectorListOrParent();
         sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
      if (sub_selector->HasLinkOrVisited()) {
        return true;
      }
    }
  }
  return false;
}

bool CSSSelector::HasVisited() const {
  for (const CSSSelector* current = this; current;
       current = current->NextSimpleSelector()) {
    CSSSelector::PseudoType pseudo = current->GetPseudoType();
    if (pseudo == CSSSelector::kPseudoVisited) {
      return true;
    }
    for (const CSSSelector* sub_selector = current->SelectorListOrParent();
         sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
      if (sub_selector->HasVisited()) {
        return true;
      }
    }
  }
  return false;
}

void CSSSelector::SetNth(int a, int b, CSSSelectorList* sub_selectors) {
  CreateRareData();
  data_.rare_data_->bits_.nth_.a_ = a;
  data_.rare_data_->bits_.nth_.b_ = b;
  data_.rare_data_->selector_list_ = sub_selectors;
}

bool CSSSelector::MatchNth(unsigned count) const {
  DCHECK(HasRareData());
  return data_.rare_data_->MatchNth(count);
}

bool CSSSelector::MatchesPseudoElement() const {
  for (const CSSSelector* current = this; current;
       current = current->NextSimpleSelector()) {
    if (current->Match() == kPseudoElement) {
      return true;
    }
    if (current->Relation() != kSubSelector) {
      return false;
    }
  }
  return false;
}

bool CSSSelector::IsAllowedInParentPseudo() const {
  // Pseudo-elements are not allowed (parse-time) within :is(), but using
  // nesting you can still get effectively that same situation using
  // e.g. "div, ::before { & {} }". Since '::before' is "contextually invalid",
  // it should not contribute to specificity.
  //
  // https://github.com/w3c/csswg-drafts/issues/9600
  return !MatchesPseudoElement();
}

bool CSSSelector::IsTreeAbidingPseudoElement() const {
  return Match() == CSSSelector::kPseudoElement &&
         (GetPseudoType() == kPseudoCheckMark ||
          GetPseudoType() == kPseudoBefore || GetPseudoType() == kPseudoAfter ||
          GetPseudoType() == kPseudoPickerIcon ||
          GetPseudoType() == kPseudoInterestHint ||
          GetPseudoType() == kPseudoMarker ||
          GetPseudoType() == kPseudoPlaceholder ||
          GetPseudoType() == kPseudoFileSelectorButton ||
          GetPseudoType() == kPseudoBackdrop ||
          GetPseudoType() == kPseudoViewTransition ||
          GetPseudoType() == kPseudoViewTransitionGroup ||
          GetPseudoType() == kPseudoViewTransitionGroupChildren ||
          GetPseudoType() == kPseudoViewTransitionImagePair ||
          GetPseudoType() == kPseudoViewTransitionOld ||
          GetPseudoType() == kPseudoViewTransitionNew ||
          GetPseudoType() == kPseudoOverscrollAreaParent ||
          IsElementBackedPseudoElement(GetPseudoType()));
}

/* static */ bool CSSSelector::IsElementBackedPseudoElement(
    CSSSelector::PseudoType pseudo) {
  return pseudo == kPseudoDetailsContent || pseudo == kPseudoPicker ||
         pseudo == kPseudoPermissionIcon;
}

bool CSSSelector::IsElementBackedPseudoElement() const {
  return Match() == CSSSelector::kPseudoElement &&
         IsElementBackedPseudoElement(GetPseudoType());
}

bool CSSSelector::IsAllowedAfterPart() const {
  if (Match() != CSSSelector::kPseudoElement &&
      Match() != CSSSelector::kPseudoClass) {
    return false;
  }
  switch (GetPseudoType()) {
    // Pseudo-elements
    //
    // All pseudo-elements other than ::part() should be allowed after
    // ::part().
    case kPseudoCheckMark:
    case kPseudoBefore:
    case kPseudoAfter:
    case kPseudoPickerIcon:
    case kPseudoInterestHint:
    case kPseudoPlaceholder:
    case kPseudoFileSelectorButton:
    case kPseudoFirstLine:
    case kPseudoFirstLetter:
    case kPseudoPicker:
    case kPseudoSelection:
    case kPseudoSearchText:
    case kPseudoTargetText:
    case kPseudoHighlight:
    case kPseudoSpellingError:
    case kPseudoGrammarError:
    case kPseudoBackdrop:
    case kPseudoCue:
    case kPseudoMarker:
    case kPseudoResizer:
    case kPseudoScrollbar:
    case kPseudoScrollbarButton:
    case kPseudoScrollbarCorner:
    case kPseudoScrollbarThumb:
    case kPseudoScrollbarTrack:
    case kPseudoScrollbarTrackPiece:
    case kPseudoScrollMarker:
    case kPseudoScrollMarkerGroup:
    case kPseudoScrollButton:
    case kPseudoColumn:
    case kPseudoWebKitCustomElement:
    case kPseudoBlinkInternalElement:
    case kPseudoDetailsContent:
    case kPseudoPermissionIcon:
    case kPseudoViewTransition:
    case kPseudoViewTransitionGroup:
    case kPseudoViewTransitionGroupChildren:
    case kPseudoViewTransitionImagePair:
    case kPseudoViewTransitionNew:
    case kPseudoViewTransitionOld:
    case kPseudoOverscrollAreaParent:
      return true;

    // It's possible that we should support ::slotted() after ::part().
    // (WebKit accepts it at parse time but it doesn't appear to work;
    // Gecko doesn't accept it.)  However, making it work isn't trivial.
    // https://github.com/w3c/csswg-drafts/issues/10807
    case kPseudoSlotted:
      return false;

    case kPseudoPart:
      return false;

    // Pseudo-classes
    //
    // All non-structural pseudo-classes should be allowed, and structural
    // pseudo-classes should be forbidden.
    case kPseudoAutofill:
    case kPseudoAutofillPreviewed:
    case kPseudoAutofillSelected:
    case kPseudoWebKitAutofill:
    case kPseudoActive:
    case kPseudoActiveViewTransition:
    case kPseudoActiveViewTransitionType:
    case kPseudoAnyLink:
    case kPseudoChecked:
    case kPseudoDefault:
    case kPseudoDialogInTopLayer:
    case kPseudoDisabled:
    case kPseudoDrag:
    case kPseudoEnabled:
    case kPseudoFocus:
    case kPseudoFocusVisible:
    case kPseudoFocusWithin:
    case kPseudoFullPageMedia:
    case kPseudoHasSlotted:
    case kPseudoHover:
    case kPseudoIndeterminate:
    case kPseudoInterestSource:
    case kPseudoInterestTarget:
    case kPseudoInvalid:
    case kPseudoLang:
    case kPseudoLink:
    case kPseudoLinkTo:
    case kPseudoMenulistPopoverWithMenubarAnchor:
    case kPseudoMenulistPopoverWithMenulistAnchor:
    case kPseudoModal:
    case kPseudoOptional:
    case kPseudoOverscrollTarget:
    case kPseudoPermissionGranted:
    case kPseudoPlaceholderShown:
    case kPseudoReadOnly:
    case kPseudoReadWrite:
    case kPseudoRequired:
    case kPseudoSelectorFragmentAnchor:
    case kPseudoState:
    case kPseudoTarget:
    case kPseudoUserInvalid:
    case kPseudoUserValid:
    case kPseudoValid:
    case kPseudoVisited:
    case kPseudoWebkitAnyLink:
    case kPseudoWindowInactive:
    case kPseudoFullScreen:
    case kPseudoFullScreenAncestor:
    case kPseudoFullscreen:
    case kPseudoInRange:
    case kPseudoOutOfRange:
    case kPseudoPaused:
    case kPseudoPictureInPicture:
    case kPseudoPlaying:
    case kPseudoXrOverlay:
    case kPseudoDefined:
    case kPseudoDir:
    case kPseudoFutureCue:
    case kPseudoIsHtml:
    case kPseudoListBox:
    case kPseudoMultiSelectFocus:
    case kPseudoOpen:
    case kPseudoPastCue:
    case kPseudoPopoverInTopLayer:
    case kPseudoPopoverOpen:
    case kPseudoRelativeAnchor:
    case kPseudoSpatialNavigationFocus:
    case kPseudoTargetCurrent:
    case kPseudoTargetBefore:
    case kPseudoTargetAfter:
    case kPseudoVideoPersistent:
    case kPseudoVideoPersistentAncestor:
      return true;

    // IsSimpleSelectorValidAfterPseudoElement allows these selectors after
    // ::part() regardless of what we do here.  However, since they are in
    // fact allowed, tell the truth here.
    case kPseudoIs:
    case kPseudoNot:
    case kPseudoWhere:
      return true;

    // :-webkit-any() should in theory be allowed too like :is() and :where(),
    // but it's a legacy feature so just leave it disallowed.
    case kPseudoAny:
      return false;

    // TODO(https://crbug.com/40623497): Figure out what to do with this.
    case kPseudoParent:
      return false;

    // These are supported only after ::webkit-scrollbar, which *maybe* makes
    // them structural?  Leave them unsupported for now
    case kPseudoHorizontal:
    case kPseudoVertical:
    case kPseudoDecrement:
    case kPseudoIncrement:
    case kPseudoStart:
    case kPseudoEnd:
    case kPseudoDoubleButton:
    case kPseudoSingleButton:
    case kPseudoNoButton:
    case kPseudoCornerPresent:
    // Likewise, this matches only after ::search-text.
    case kPseudoCurrent:
      return false;

    // These are supported only on @page, so not allowed after ::part().
    case kPseudoFirstPage:
    case kPseudoLeftPage:
    case kPseudoRightPage:
      return false;

    // These are structural pseudo-classes, which should not be allowed.
    case kPseudoEmpty:
    case kPseudoFirstChild:
    case kPseudoFirstOfType:
    case kPseudoLastChild:
    case kPseudoLastOfType:
    case kPseudoNthChild:
    case kPseudoNthLastChild:
    case kPseudoNthLastOfType:
    case kPseudoNthOfType:
    case kPseudoOnlyChild:
    case kPseudoOnlyOfType:
    case kPseudoRoot:
      return false;

    // These are other pseudo-classes that match based on tree information
    // rather than local element information, which should not be allowed.
    case kPseudoHas:
    case kPseudoHasDatalist:
    case kPseudoHost:
    case kPseudoHostContext:
    case kPseudoHostHasNonAutoAppearance:
    case kPseudoScope:
      return false;

    case kPseudoUnparsed:
    case kPseudoUnknown:
      return false;
  }
}

bool CSSSelector::IsOrContainsHostPseudoClass() const {
  if (IsHostPseudoClass()) {
    return true;
  }
  // Accept selector lists like :is(:host, .foo).
  for (const CSSSelector* sub_selector = SelectorListOrParent(); sub_selector;
       sub_selector = CSSSelectorList::Next(*sub_selector)) {
    if (sub_selector->IsOrContainsHostPseudoClass()) {
      return true;
    }
  }
  return false;
}

bool CSSSelector::IsDeeplyHostPseudoClass() const {
  if ((GetPseudoType() == kPseudoIs || GetPseudoType() == kPseudoWhere ||
       GetPseudoType() == kPseudoParent) &&
      SelectorListOrParent() &&
      CSSSelectorList::IsSingleComplexSelector(*SelectorListOrParent())) {
    return SelectorListOrParent()->IsDeeplyHostPseudoClass();
  }
  return IsHostPseudoClass();
}

template <typename Functor>
static bool ForAnyInComplexSelector(const Functor& functor,
                                    const CSSSelector& selector) {
  for (const CSSSelector* current = &selector; current;
       current = current->NextSimpleSelector()) {
    if (functor(*current)) {
      return true;
    }
    if (const CSSSelectorList* selector_list = current->SelectorList()) {
      for (const CSSSelector* sub_selector = selector_list->First();
           sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
        if (ForAnyInComplexSelector(functor, *sub_selector)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool CSSSelector::CrossesTreeScopes() const {
  for (const CSSSelector* s = this; s; s = s->NextSimpleSelector()) {
    switch (s->Relation()) {
      case kShadowPart:
      case kUAShadow:
      case kShadowSlot:
        return true;
      default:
        break;
    }
  }
  return false;
}

String CSSSelector::FormatPseudoTypeForDebugging(PseudoType type) {
  for (const auto& s : kPseudoTypeWithoutArgumentsMap) {
    if (s.type == type) {
      return s.string;
    }
  }
  for (const auto& s : kPseudoTypeWithArgumentsMap) {
    if (s.type == type) {
      return s.string;
    }
  }
  StringBuilder builder;
  builder.Append("pseudo-");
  builder.AppendNumber(static_cast<int>(type));
  return builder.ReleaseString();
}

CSSSelector::RareData::RareData(const AtomicString& value)
    : matching_value_(value),
      serializing_value_(value),
      bits_(),
      attribute_(AnyQName()),
      argument_(g_null_atom) {}

CSSSelector::RareData::RareData(const RareData& other)
    : matching_value_(other.matching_value_),
      serializing_value_(other.serializing_value_),
      bits_(other.bits_),
      attribute_(other.attribute_),
      argument_(other.argument_),
      selector_list_(other.selector_list_),
      ident_list_(other.ident_list_ ? std::make_unique<Vector<AtomicString>>(
                                          *other.ident_list_)
                                    : nullptr) {}

CSSSelector::RareData::~RareData() = default;

// a helper function for checking nth-arguments
bool CSSSelector::RareData::MatchNth(unsigned unsigned_count) {
  // These very large values for aN + B or count can't ever match, so
  // give up immediately if we see them.
  int max_value = std::numeric_limits<int>::max() / 2;
  int min_value = std::numeric_limits<int>::min() / 2;
  if (unsigned_count > static_cast<unsigned>(max_value) ||
      NthAValue() > max_value || NthAValue() < min_value ||
      NthBValue() > max_value || NthBValue() < min_value) [[unlikely]] {
    return false;
  }

  int count = static_cast<int>(unsigned_count);
  if (!NthAValue()) {
    return count == NthBValue();
  }
  if (NthAValue() > 0) {
    if (count < NthBValue()) {
      return false;
    }
    return (count - NthBValue()) % NthAValue() == 0;
  }
  if (count > NthBValue()) {
    return false;
  }
  return (NthBValue() - count) % (-NthAValue()) == 0;
}

CSSSelector::RareData* CSSSelector::RareData::Renest(StyleRule* new_parent) {
  CSSSelectorList* old_list = selector_list_.Get();
  CSSSelectorList* new_list = old_list ? old_list->Renest(new_parent) : nullptr;
  if (old_list == new_list) {
    return this;
  }
  auto* rare_data = MakeGarbageCollected<RareData>(*this);
  rare_data->selector_list_ = new_list;
  return rare_data;
}

void CSSSelector::SetIdentList(
    std::unique_ptr<Vector<AtomicString>> ident_list) {
  CreateRareData();
  data_.rare_data_->ident_list_ = std::move(ident_list);
}

void CSSSelector::Trace(Visitor* visitor) const {
  if (MatchForOilpan() == kPseudoClass && GetPseudoType() == kPseudoParent) {
    visitor->Trace(data_.parent_rule_);
  } else if (HasRareDataForOilpan()) {
    visitor->Trace(data_.rare_data_);
  }
}

void CSSSelector::RareData::Trace(Visitor* visitor) const {
  visitor->Trace(selector_list_);
  visitor->Trace(link_condition_);
}

const CSSSelector* CSSSelector::SelectorListOrParent() const {
  if (Match() == kPseudoClass && GetPseudoType() == kPseudoParent) {
    if (ParentRule()) {
      return ParentRule()->FirstSelector();
    } else {
      return nullptr;
    }
  } else if (HasRareData() && data_.rare_data_->selector_list_) {
    return data_.rare_data_->selector_list_->First();
  } else {
    return nullptr;
  }
}

bool CSSSelector::IsChildIndexedSelector() const {
  switch (GetPseudoType()) {
    case kPseudoFirstChild:
    case kPseudoFirstOfType:
    case kPseudoLastChild:
    case kPseudoLastOfType:
    case kPseudoNthChild:
    case kPseudoNthLastChild:
    case kPseudoNthLastOfType:
    case kPseudoNthOfType:
    case kPseudoOnlyChild:
    case kPseudoOnlyOfType:
      return true;
    default:
      return false;
  }
}

CSSSelector::RelationType ConvertRelationToRelative(
    CSSSelector::RelationType relation) {
  switch (relation) {
    case CSSSelector::kSubSelector:
    case CSSSelector::kDescendant:
      return CSSSelector::kRelativeDescendant;
    case CSSSelector::kChild:
      return CSSSelector::kRelativeChild;
    case CSSSelector::kDirectAdjacent:
      return CSSSelector::kRelativeDirectAdjacent;
    case CSSSelector::kIndirectAdjacent:
      return CSSSelector::kRelativeIndirectAdjacent;
    default:
      NOTREACHED();
  }
}

constexpr bool IsPseudoMapSorted(const NameToPseudoStruct* map, unsigned size) {
  for (unsigned i = 0; i < size - 1; i++) {
    // strcmp/strncmp would be much better here, but unfortunately they aren't
    // constexpr.
    const char* current_string = UNSAFE_TODO(map[i].string);
    const char* next_string = UNSAFE_TODO(map[i + 1].string);
    while (true) {
      if (*current_string > *next_string) {
        return false;
      }
      if (*current_string < *next_string) {
        break;
      }
      if (!*current_string) {
        break;
      }
      if (!*next_string) {
        return false;
      }
      UNSAFE_TODO(current_string++);
      UNSAFE_TODO(next_string++);
    }
  }
  return true;
}

static_assert(IsPseudoMapSorted(kPseudoTypeWithoutArgumentsMap,
                                std::size(kPseudoTypeWithoutArgumentsMap)),
              "kPseudoTypeWithoutArgumentsMap must be sorted.");
static_assert(IsPseudoMapSorted(kPseudoTypeWithArgumentsMap,
                                std::size(kPseudoTypeWithArgumentsMap)),
              "kPseudoTypeWithArgumentsMap must be sorted.");

}  // namespace blink
