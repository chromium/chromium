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
#include <memory>

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

namespace {

unsigned MaximumSpecificity(const CSSSelectorList* list,
                            CSSSelector::SpecificityMode mode) {
  if (!list)
    return 0;

  unsigned result = 0;
  const CSSSelector* selector;
  for (selector = list->First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    unsigned specificity = selector->Specificity(mode);
    if (result < specificity)
      result = specificity;
  }
  return result;
}

}  // namespace

struct SameSizeAsCSSSelector {
  unsigned bitfields;
  void* pointers[1];
};

ASSERT_SIZE(CSSSelector, SameSizeAsCSSSelector);

void CSSSelector::CreateRareData() {
  DCHECK_NE(Match(), kTag);
  if (has_rare_data_)
    return;
  // This transitions the DataUnion from |value_| to |rare_data_| and thus needs to be careful to
  // correctly manage explicitly destruction of |value_| followed by placement new of |rare_data_|.
  // A straight-assignment will compile and may kinda work, but will be undefined behavior.
  auto rare_data = RareData::Create(data_.value_);
  data_.value_.~AtomicString();
  new (&data_.rare_data_) scoped_refptr<RareData>(std::move(rare_data));
  has_rare_data_ = true;
}

unsigned CSSSelector::Specificity(SpecificityMode mode) const {
  // make sure the result doesn't overflow
  static const unsigned kMaxValueMask = 0xffffff;
  static const unsigned kIdMask = 0xff0000;
  static const unsigned kClassMask = 0x00ff00;
  static const unsigned kElementMask = 0x0000ff;

  if (IsForPage())
    return SpecificityForPage() & kMaxValueMask;

  unsigned total = 0;
  unsigned temp = 0;

  for (const CSSSelector* selector = this; selector;
       selector = selector->TagHistory()) {
    temp = total + selector->SpecificityForOneSelector(mode);
    // Clamp each component to its max in the case of overflow.
    if ((temp & kIdMask) < (total & kIdMask))
      total |= kIdMask;
    else if ((temp & kClassMask) < (total & kClassMask))
      total |= kClassMask;
    else if ((temp & kElementMask) < (total & kElementMask))
      total |= kElementMask;
    else
      total = temp;
  }
  return total;
}

inline unsigned CSSSelector::SpecificityForOneSelector(
    SpecificityMode mode) const {
  // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity.
  // This function isn't quite correct.
  // http://www.w3.org/TR/selectors/#specificity
  switch (match_) {
    case kId:
      return kIdSpecificity;
    case kPseudoClass:
      switch (GetPseudoType()) {
        case kPseudoWhere:
          return 0;
        case kPseudoHost:
        case kPseudoHostContext:
          if (mode == SpecificityMode::kNormal) {
            // We dynamically compute the specificity of :host and :host-context
            // during matching.
            return 0;
          } else {
            DCHECK_EQ(SpecificityMode::kIncludeHostPseudos, mode);
            return kClassLikeSpecificity +
                   MaximumSpecificity(SelectorList(), mode);
          }
        case kPseudoNot:
          DCHECK(SelectorList());
          FALLTHROUGH;
        case kPseudoIs:
          return MaximumSpecificity(SelectorList(), mode);
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
          DCHECK(SelectorList()->HasOneSelector());
          return kClassLikeSpecificity + SelectorList()->First()->Specificity();
        default:
          break;
      }
      return kClassLikeSpecificity;
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
      if (TagQName().LocalName() == UniversalSelectorAtom())
        return 0;
      return kTagSpecificity;
    case kUnknown:
      return 0;
  }
  NOTREACHED();
  return 0;
}

unsigned CSSSelector::SpecificityForPage() const {
  // See https://drafts.csswg.org/css-page/#cascading-and-page-context
  unsigned s = 0;

  for (const CSSSelector* component = this; component;
       component = component->TagHistory()) {
    switch (component->match_) {
      case kTag:
        s += TagQName().LocalName() == UniversalSelectorAtom() ? 0 : 4;
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
    case kPseudoBefore:
      return kPseudoIdBefore;
    case kPseudoAfter:
      return kPseudoIdAfter;
    case kPseudoMarker:
      return kPseudoIdMarker;
    case kPseudoBackdrop:
      return kPseudoIdBackdrop;
    case kPseudoScrollbar:
      return kPseudoIdScrollbar;
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
    case kPseudoTargetText:
      return kPseudoIdTargetText;
    case kPseudoUnknown:
    case kPseudoEmpty:
    case kPseudoFirstChild:
    case kPseudoFirstOfType:
    case kPseudoLastChild:
    case kPseudoLastOfType:
    case kPseudoOnlyChild:
    case kPseudoOnlyOfType:
    case kPseudoNthChild:
    case kPseudoNthOfType:
    case kPseudoNthLastChild:
    case kPseudoNthLastOfType:
    case kPseudoLink:
    case kPseudoVisited:
    case kPseudoAny:
    case kPseudoIs:
    case kPseudoWhere:
    case kPseudoAnyLink:
    case kPseudoWebkitAnyLink:
    case kPseudoAutofill:
    case kPseudoAutofillPreviewed:
    case kPseudoAutofillSelected:
    case kPseudoHover:
    case kPseudoDrag:
    case kPseudoFocus:
    case kPseudoFocusVisible:
    case kPseudoFocusWithin:
    case kPseudoActive:
    case kPseudoChecked:
    case kPseudoEnabled:
    case kPseudoFullPageMedia:
    case kPseudoDefault:
    case kPseudoDisabled:
    case kPseudoOptional:
    case kPseudoPlaceholder:
    case kPseudoPlaceholderShown:
    case kPseudoRequired:
    case kPseudoReadOnly:
    case kPseudoReadWrite:
    case kPseudoValid:
    case kPseudoInvalid:
    case kPseudoIndeterminate:
    case kPseudoTarget:
    case kPseudoLang:
    case kPseudoNot:
    case kPseudoRoot:
    case kPseudoScope:
    case kPseudoWindowInactive:
    case kPseudoCornerPresent:
    case kPseudoDecrement:
    case kPseudoIncrement:
    case kPseudoHorizontal:
    case kPseudoVertical:
    case kPseudoStart:
    case kPseudoEnd:
    case kPseudoDoubleButton:
    case kPseudoSingleButton:
    case kPseudoNoButton:
    case kPseudoFirstPage:
    case kPseudoLeftPage:
    case kPseudoRightPage:
    case kPseudoInRange:
    case kPseudoOutOfRange:
    case kPseudoWebKitCustomElement:
    case kPseudoBlinkInternalElement:
    case kPseudoCue:
    case kPseudoFutureCue:
    case kPseudoPastCue:
    case kPseudoUnresolved:
    case kPseudoDefined:
    case kPseudoContent:
    case kPseudoHost:
    case kPseudoHostContext:
    case kPseudoPart:
    case kPseudoState:
    case kPseudoShadow:
    case kPseudoFullScreen:
    case kPseudoFullScreenAncestor:
    case kPseudoFullscreen:
    case kPseudoPictureInPicture:
    case kPseudoSpatialNavigationFocus:
    case kPseudoSpatialNavigationInterest:
    case kPseudoHasDatalist:
    case kPseudoIsHtml:
    case kPseudoListBox:
    case kPseudoMultiSelectFocus:
    case kPseudoHostHasAppearance:
    case kPseudoSlotted:
    case kPseudoVideoPersistent:
    case kPseudoVideoPersistentAncestor:
    case kPseudoXrOverlay:
    case kPseudoModal:
      return kPseudoIdNone;
  }

  NOTREACHED();
  return kPseudoIdNone;
}

// Could be made smaller and faster by replacing pointer with an
// offset into a string buffer and making the bit fields smaller but
// that could not be maintained by hand.
struct NameToPseudoStruct {
  const char* string;
  unsigned type : 8;
};

// These tables should be kept sorted.
const static NameToPseudoStruct kPseudoTypeWithoutArgumentsMap[] = {
    {"-internal-autofill-previewed", CSSSelector::kPseudoAutofillPreviewed},
    {"-internal-autofill-selected", CSSSelector::kPseudoAutofillSelected},
    {"-internal-has-datalist", CSSSelector::kPseudoHasDatalist},
    {"-internal-is-html", CSSSelector::kPseudoIsHtml},
    {"-internal-list-box", CSSSelector::kPseudoListBox},
    {"-internal-media-controls-overlay-cast-button",
     CSSSelector::kPseudoWebKitCustomElement},
    {"-internal-modal", CSSSelector::kPseudoModal},
    {"-internal-multi-select-focus", CSSSelector::kPseudoMultiSelectFocus},
    {"-internal-shadow-host-has-appearance",
     CSSSelector::kPseudoHostHasAppearance},
    {"-internal-spatial-navigation-focus",
     CSSSelector::kPseudoSpatialNavigationFocus},
    {"-internal-spatial-navigation-interest",
     CSSSelector::kPseudoSpatialNavigationInterest},
    {"-internal-video-persistent", CSSSelector::kPseudoVideoPersistent},
    {"-internal-video-persistent-ancestor",
     CSSSelector::kPseudoVideoPersistentAncestor},
    {"-webkit-any-link", CSSSelector::kPseudoWebkitAnyLink},
    {"-webkit-autofill", CSSSelector::kPseudoAutofill},
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
    {"after", CSSSelector::kPseudoAfter},
    {"any-link", CSSSelector::kPseudoAnyLink},
    {"backdrop", CSSSelector::kPseudoBackdrop},
    {"before", CSSSelector::kPseudoBefore},
    {"checked", CSSSelector::kPseudoChecked},
    {"content", CSSSelector::kPseudoContent},
    {"corner-present", CSSSelector::kPseudoCornerPresent},
    {"cue", CSSSelector::kPseudoWebKitCustomElement},
    {"decrement", CSSSelector::kPseudoDecrement},
    {"default", CSSSelector::kPseudoDefault},
    {"defined", CSSSelector::kPseudoDefined},
    {"disabled", CSSSelector::kPseudoDisabled},
    {"double-button", CSSSelector::kPseudoDoubleButton},
    {"empty", CSSSelector::kPseudoEmpty},
    {"enabled", CSSSelector::kPseudoEnabled},
    {"end", CSSSelector::kPseudoEnd},
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
    {"horizontal", CSSSelector::kPseudoHorizontal},
    {"host", CSSSelector::kPseudoHost},
    {"hover", CSSSelector::kPseudoHover},
    {"in-range", CSSSelector::kPseudoInRange},
    {"increment", CSSSelector::kPseudoIncrement},
    {"indeterminate", CSSSelector::kPseudoIndeterminate},
    {"invalid", CSSSelector::kPseudoInvalid},
    {"last-child", CSSSelector::kPseudoLastChild},
    {"last-of-type", CSSSelector::kPseudoLastOfType},
    {"left", CSSSelector::kPseudoLeftPage},
    {"link", CSSSelector::kPseudoLink},
    {"marker", CSSSelector::kPseudoMarker},
    {"no-button", CSSSelector::kPseudoNoButton},
    {"only-child", CSSSelector::kPseudoOnlyChild},
    {"only-of-type", CSSSelector::kPseudoOnlyOfType},
    {"optional", CSSSelector::kPseudoOptional},
    {"out-of-range", CSSSelector::kPseudoOutOfRange},
    {"past", CSSSelector::kPseudoPastCue},
    {"picture-in-picture", CSSSelector::kPseudoPictureInPicture},
    {"placeholder", CSSSelector::kPseudoPlaceholder},
    {"placeholder-shown", CSSSelector::kPseudoPlaceholderShown},
    {"read-only", CSSSelector::kPseudoReadOnly},
    {"read-write", CSSSelector::kPseudoReadWrite},
    {"required", CSSSelector::kPseudoRequired},
    {"right", CSSSelector::kPseudoRightPage},
    {"root", CSSSelector::kPseudoRoot},
    {"scope", CSSSelector::kPseudoScope},
    {"selection", CSSSelector::kPseudoSelection},
    {"shadow", CSSSelector::kPseudoShadow},
    {"single-button", CSSSelector::kPseudoSingleButton},
    {"start", CSSSelector::kPseudoStart},
    {"target", CSSSelector::kPseudoTarget},
    {"target-text", CSSSelector::kPseudoTargetText},
    {"unresolved", CSSSelector::kPseudoUnresolved},
    {"valid", CSSSelector::kPseudoValid},
    {"vertical", CSSSelector::kPseudoVertical},
    {"visited", CSSSelector::kPseudoVisited},
    {"window-inactive", CSSSelector::kPseudoWindowInactive},
    {"xr-overlay", CSSSelector::kPseudoXrOverlay},
};

const static NameToPseudoStruct kPseudoTypeWithArgumentsMap[] = {
    {"-webkit-any", CSSSelector::kPseudoAny},
    {"cue", CSSSelector::kPseudoCue},
    {"host", CSSSelector::kPseudoHost},
    {"host-context", CSSSelector::kPseudoHostContext},
    {"is", CSSSelector::kPseudoIs},
    {"lang", CSSSelector::kPseudoLang},
    {"not", CSSSelector::kPseudoNot},
    {"nth-child", CSSSelector::kPseudoNthChild},
    {"nth-last-child", CSSSelector::kPseudoNthLastChild},
    {"nth-last-of-type", CSSSelector::kPseudoNthLastOfType},
    {"nth-of-type", CSSSelector::kPseudoNthOfType},
    {"part", CSSSelector::kPseudoPart},
    {"slotted", CSSSelector::kPseudoSlotted},
    {"state", CSSSelector::kPseudoState},
    {"where", CSSSelector::kPseudoWhere},
};

static CSSSelector::PseudoType NameToPseudoType(const AtomicString& name,
                                                bool has_arguments) {
  if (name.IsNull() || !name.Is8Bit())
    return CSSSelector::kPseudoUnknown;

  const NameToPseudoStruct* pseudo_type_map;
  const NameToPseudoStruct* pseudo_type_map_end;
  if (has_arguments) {
    pseudo_type_map = kPseudoTypeWithArgumentsMap;
    pseudo_type_map_end =
        kPseudoTypeWithArgumentsMap + base::size(kPseudoTypeWithArgumentsMap);
  } else {
    pseudo_type_map = kPseudoTypeWithoutArgumentsMap;
    pseudo_type_map_end = kPseudoTypeWithoutArgumentsMap +
                          base::size(kPseudoTypeWithoutArgumentsMap);
  }
  const NameToPseudoStruct* match = std::lower_bound(
      pseudo_type_map, pseudo_type_map_end, name,
      [](const NameToPseudoStruct& entry, const AtomicString& name) -> bool {
        DCHECK(name.Is8Bit());
        DCHECK(entry.string);
        // If strncmp returns 0, then either the keys are equal, or |name| sorts
        // before |entry|.
        return strncmp(entry.string,
                       reinterpret_cast<const char*>(name.Characters8()),
                       name.length()) < 0;
      });
  if (match == pseudo_type_map_end || match->string != name.GetString())
    return CSSSelector::kPseudoUnknown;

  if (match->type == CSSSelector::kPseudoFocusVisible &&
      !RuntimeEnabledFeatures::CSSFocusVisibleEnabled())
    return CSSSelector::kPseudoUnknown;

  if (match->type == CSSSelector::kPseudoPictureInPicture &&
      !RuntimeEnabledFeatures::CSSPictureInPictureEnabled())
    return CSSSelector::kPseudoUnknown;

  if (match->type == CSSSelector::kPseudoState &&
      !RuntimeEnabledFeatures::CustomStatePseudoClassEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  if (match->type == CSSSelector::kPseudoTargetText &&
      !RuntimeEnabledFeatures::CSSTargetTextPseudoElementEnabled()) {
    return CSSSelector::kPseudoUnknown;
  }

  return static_cast<CSSSelector::PseudoType>(match->type);
}

#ifndef NDEBUG
void CSSSelector::Show(int indent) const {
  printf("%*sSelectorText(): %s\n", indent, "", SelectorText().Ascii().c_str());
  printf("%*smatch_: %d\n", indent, "", match_);
  if (match_ != kTag)
    printf("%*sValue(): %s\n", indent, "", Value().Ascii().c_str());
  printf("%*sGetPseudoType(): %d\n", indent, "", GetPseudoType());
  if (match_ == kTag) {
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
  if (TagHistory()) {
    printf("\n%*s--> (Relation() == %d)\n", indent, "", Relation());
    TagHistory()->Show(indent + 2);
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
#endif

CSSSelector::PseudoType CSSSelector::ParsePseudoType(const AtomicString& name,
                                                     bool has_arguments) {
  PseudoType pseudo_type = NameToPseudoType(name, has_arguments);
  if (pseudo_type != kPseudoUnknown)
    return pseudo_type;

  if (name.StartsWith("-webkit-"))
    return kPseudoWebKitCustomElement;
  if (name.StartsWith("-internal-"))
    return kPseudoBlinkInternalElement;

  return kPseudoUnknown;
}

PseudoId CSSSelector::ParsePseudoId(const String& name, const Node* parent) {
  unsigned name_without_colons_start =
      name[0] == ':' ? (name[1] == ':' ? 2 : 1) : 0;
  PseudoId pseudo_id = GetPseudoId(ParsePseudoType(
      AtomicString(name.Substring(name_without_colons_start)), false));
  if (!PseudoElement::IsWebExposed(pseudo_id, parent))
    return kPseudoIdNone;
  return pseudo_id;
}

void CSSSelector::UpdatePseudoPage(const AtomicString& value) {
  DCHECK_EQ(Match(), kPagePseudoClass);
  SetValue(value);
  PseudoType type = ParsePseudoType(value, false);
  if (type != kPseudoFirstPage && type != kPseudoLeftPage &&
      type != kPseudoRightPage) {
    type = kPseudoUnknown;
  }
  pseudo_type_ = type;
}

void CSSSelector::UpdatePseudoType(const AtomicString& value,
                                   const CSSParserContext& context,
                                   bool has_arguments,
                                   CSSParserMode mode) {
  DCHECK(match_ == kPseudoClass || match_ == kPseudoElement);
  SetValue(value);
  SetPseudoType(ParsePseudoType(value, has_arguments));

  switch (GetPseudoType()) {
    case kPseudoAfter:
    case kPseudoBefore:
    case kPseudoFirstLetter:
    case kPseudoFirstLine:
      // The spec says some pseudos allow both single and double colons like
      // :before for backwards compatability. Single colon becomes PseudoClass,
      // but should be PseudoElement like double colon.
      if (match_ == kPseudoClass)
        match_ = kPseudoElement;
      FALLTHROUGH;
    // For pseudo elements
    case kPseudoBackdrop:
    case kPseudoCue:
    case kPseudoMarker:
    case kPseudoPart:
    case kPseudoPlaceholder:
    case kPseudoResizer:
    case kPseudoScrollbar:
    case kPseudoScrollbarCorner:
    case kPseudoScrollbarButton:
    case kPseudoScrollbarThumb:
    case kPseudoScrollbarTrack:
    case kPseudoScrollbarTrackPiece:
    case kPseudoSelection:
    case kPseudoWebKitCustomElement:
    case kPseudoContent:
    case kPseudoSlotted:
    case kPseudoTargetText:
      if (match_ != kPseudoElement)
        pseudo_type_ = kPseudoUnknown;
      break;
    case kPseudoShadow:
      if (match_ != kPseudoElement || context.IsLiveProfile())
        pseudo_type_ = kPseudoUnknown;
      break;
    case kPseudoBlinkInternalElement:
      if (match_ != kPseudoElement || mode != kUASheetMode)
        pseudo_type_ = kPseudoUnknown;
      break;
    case kPseudoHasDatalist:
    case kPseudoHostHasAppearance:
    case kPseudoIsHtml:
    case kPseudoListBox:
    case kPseudoModal:
    case kPseudoMultiSelectFocus:
    case kPseudoSpatialNavigationFocus:
    case kPseudoSpatialNavigationInterest:
    case kPseudoVideoPersistent:
    case kPseudoVideoPersistentAncestor:
      if (mode != kUASheetMode) {
        pseudo_type_ = kPseudoUnknown;
        break;
      }
      FALLTHROUGH;
    // For pseudo classes
    case kPseudoActive:
    case kPseudoAny:
    case kPseudoAnyLink:
    case kPseudoAutofill:
    case kPseudoAutofillPreviewed:
    case kPseudoAutofillSelected:
    case kPseudoChecked:
    case kPseudoCornerPresent:
    case kPseudoDecrement:
    case kPseudoDefault:
    case kPseudoDefined:
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
    case kPseudoHorizontal:
    case kPseudoHost:
    case kPseudoHostContext:
    case kPseudoHover:
    case kPseudoInRange:
    case kPseudoIncrement:
    case kPseudoIndeterminate:
    case kPseudoInvalid:
    case kPseudoWhere:
    case kPseudoLang:
    case kPseudoLastChild:
    case kPseudoLastOfType:
    case kPseudoLink:
    case kPseudoIs:
    case kPseudoNoButton:
    case kPseudoNot:
    case kPseudoNthChild:
    case kPseudoNthLastChild:
    case kPseudoNthLastOfType:
    case kPseudoNthOfType:
    case kPseudoOnlyChild:
    case kPseudoOnlyOfType:
    case kPseudoOptional:
    case kPseudoPictureInPicture:
    case kPseudoPlaceholderShown:
    case kPseudoOutOfRange:
    case kPseudoPastCue:
    case kPseudoReadOnly:
    case kPseudoReadWrite:
    case kPseudoRequired:
    case kPseudoRoot:
    case kPseudoScope:
    case kPseudoSingleButton:
    case kPseudoStart:
    case kPseudoState:
    case kPseudoTarget:
    case kPseudoUnknown:
    case kPseudoValid:
    case kPseudoVertical:
    case kPseudoVisited:
    case kPseudoWebkitAnyLink:
    case kPseudoWindowInactive:
    case kPseudoXrOverlay:
      if (match_ != kPseudoClass)
        pseudo_type_ = kPseudoUnknown;
      break;
    case kPseudoFirstPage:
    case kPseudoLeftPage:
    case kPseudoRightPage:
      pseudo_type_ = kPseudoUnknown;
      break;
    case kPseudoUnresolved:
      if (match_ != kPseudoClass || !context.CustomElementsV0Enabled())
        pseudo_type_ = kPseudoUnknown;
      break;
  }
}

bool CSSSelector::operator==(const CSSSelector& other) const {
  const CSSSelector* sel1 = this;
  const CSSSelector* sel2 = &other;

  while (sel1 && sel2) {
    if (sel1->Attribute() != sel2->Attribute() ||
        sel1->Relation() != sel2->Relation() || sel1->match_ != sel2->match_ ||
        sel1->Value() != sel2->Value() ||
        sel1->GetPseudoType() != sel2->GetPseudoType() ||
        sel1->Argument() != sel2->Argument()) {
      return false;
    }
    if (sel1->match_ == kTag) {
      if (sel1->TagQName() != sel2->TagQName())
        return false;
    }
    sel1 = sel1->TagHistory();
    sel2 = sel2->TagHistory();
  }

  if (sel1 || sel2)
    return false;

  return true;
}

static void SerializeIdentifierOrAny(const AtomicString& identifier,
                                     const AtomicString& any,
                                     StringBuilder& builder) {
  if (identifier != any)
    SerializeIdentifier(identifier, builder);
  else
    builder.Append(g_star_atom);
}

static void SerializeNamespacePrefixIfNeeded(const AtomicString& prefix,
                                             const AtomicString& any,
                                             StringBuilder& builder,
                                             bool is_attribute_selector) {
  if (prefix.IsNull() || (prefix.IsEmpty() && is_attribute_selector))
    return;
  SerializeIdentifierOrAny(prefix, any, builder);
  builder.Append('|');
}

const CSSSelector* CSSSelector::SerializeCompound(
    StringBuilder& builder) const {
  if (match_ == kTag && !tag_is_implicit_) {
    SerializeNamespacePrefixIfNeeded(TagQName().Prefix(), g_star_atom, builder,
                                     IsAttributeSelector());
    SerializeIdentifierOrAny(TagQName().LocalName(), UniversalSelectorAtom(),
                             builder);
  }

  for (const CSSSelector* simple_selector = this; simple_selector;
       simple_selector = simple_selector->TagHistory()) {
    if (simple_selector->match_ == kId) {
      builder.Append('#');
      SerializeIdentifier(simple_selector->SerializingValue(), builder);
    } else if (simple_selector->match_ == kClass) {
      builder.Append('.');
      SerializeIdentifier(simple_selector->SerializingValue(), builder);
    } else if (simple_selector->match_ == kPseudoClass ||
               simple_selector->match_ == kPagePseudoClass) {
      builder.Append(':');
      builder.Append(simple_selector->SerializingValue());

      switch (simple_selector->GetPseudoType()) {
        case kPseudoNthChild:
        case kPseudoNthLastChild:
        case kPseudoNthOfType:
        case kPseudoNthLastOfType: {
          builder.Append('(');

          // https://drafts.csswg.org/css-syntax/#serializing-anb
          int a = simple_selector->data_.rare_data_->NthAValue();
          int b = simple_selector->data_.rare_data_->NthBValue();
          if (a == 0) {
            builder.Append(String::Number(b));
          } else {
            if (a == 1)
              builder.Append('n');
            else if (a == -1)
              builder.Append("-n");
            else
              builder.AppendFormat("%dn", a);

            if (b < 0)
              builder.Append(String::Number(b));
            else if (b > 0)
              builder.AppendFormat("+%d", b);
          }

          builder.Append(')');
          break;
        }
        case kPseudoLang:
        case kPseudoState:
          builder.Append('(');
          SerializeIdentifier(simple_selector->Argument(), builder);
          builder.Append(')');
          break;
        case kPseudoNot:
          DCHECK(simple_selector->SelectorList());
          break;
        case kPseudoHost:
        case kPseudoHostContext:
        case kPseudoAny:
        case kPseudoIs:
        case kPseudoWhere:
          break;
        default:
          break;
      }
    } else if (simple_selector->match_ == kPseudoElement) {
      builder.Append("::");
      SerializeIdentifier(simple_selector->SerializingValue(), builder);
      switch (simple_selector->GetPseudoType()) {
        case kPseudoPart: {
          char separator = '(';
          for (AtomicString part : *simple_selector->PartNames()) {
            builder.Append(separator);
            if (separator == '(')
              separator = ' ';
            SerializeIdentifier(part, builder);
          }
          builder.Append(')');
          break;
        }
        default:
          break;
      }
    } else if (simple_selector->IsAttributeSelector()) {
      builder.Append('[');
      SerializeNamespacePrefixIfNeeded(simple_selector->Attribute().Prefix(),
                                       g_star_atom, builder,
                                       simple_selector->IsAttributeSelector());
      SerializeIdentifier(simple_selector->Attribute().LocalName(), builder);
      switch (simple_selector->match_) {
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
      if (simple_selector->match_ != kAttributeSet) {
        SerializeString(simple_selector->SerializingValue(), builder);
        if (simple_selector->AttributeMatch() ==
            AttributeMatchType::kCaseInsensitive)
          builder.Append(" i");
        builder.Append(']');
      }
    }

    if (simple_selector->SelectorList()) {
      builder.Append('(');
      const CSSSelector* first_sub_selector =
          simple_selector->SelectorList()->FirstForCSSOM();
      for (const CSSSelector* sub_selector = first_sub_selector; sub_selector;
           sub_selector = CSSSelectorList::Next(*sub_selector)) {
        if (sub_selector != first_sub_selector)
          builder.Append(", ");
        builder.Append(sub_selector->SelectorText());
      }
      builder.Append(')');
    }

    if (simple_selector->Relation() != kSubSelector)
      return simple_selector;
  }
  return nullptr;
}

String CSSSelector::SelectorText() const {
  String result;
  for (const CSSSelector* compound = this; compound;
       compound = compound->TagHistory()) {
    StringBuilder builder;
    compound = compound->SerializeCompound(builder);
    if (!compound)
      return builder.ToString() + result;

    DCHECK(compound->Relation() != kSubSelector);
    switch (compound->Relation()) {
      case kDescendant:
        result = " " + builder.ToString() + result;
        break;
      case kChild:
        result = " > " + builder.ToString() + result;
        break;
      case kShadowDeep:
      case kShadowDeepAsDescendant:
        result = " /deep/ " + builder.ToString() + result;
        break;
      case kDirectAdjacent:
        result = " + " + builder.ToString() + result;
        break;
      case kIndirectAdjacent:
        result = " ~ " + builder.ToString() + result;
        break;
      case kSubSelector:
        NOTREACHED();
        break;
      case kShadowPart:
      case kShadowPseudo:
      case kShadowSlot:
        result = builder.ToString() + result;
        break;
    }
  }
  NOTREACHED();
  return String();
}

void CSSSelector::SetAttribute(const QualifiedName& value,
                               AttributeMatchType match_type) {
  CreateRareData();
  data_.rare_data_->attribute_ = value;
  data_.rare_data_->bits_.attribute_match_ = match_type;
}

void CSSSelector::SetArgument(const AtomicString& value) {
  CreateRareData();
  data_.rare_data_->argument_ = value;
}

void CSSSelector::SetSelectorList(
    std::unique_ptr<CSSSelectorList> selector_list) {
  CreateRareData();
  data_.rare_data_->selector_list_ = std::move(selector_list);
}

static bool ValidateSubSelector(const CSSSelector* selector) {
  switch (selector->Match()) {
    case CSSSelector::kTag:
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
    case CSSSelector::kPseudoSpatialNavigationInterest:
    case CSSSelector::kPseudoHasDatalist:
    case CSSSelector::kPseudoIsHtml:
    case CSSSelector::kPseudoListBox:
    case CSSSelector::kPseudoHostHasAppearance:
      return true;
    default:
      return false;
  }
}

bool CSSSelector::IsCompound() const {
  if (!ValidateSubSelector(this))
    return false;

  const CSSSelector* prev_sub_selector = this;
  const CSSSelector* sub_selector = TagHistory();

  while (sub_selector) {
    if (prev_sub_selector->Relation() != kSubSelector)
      return false;
    if (!ValidateSubSelector(sub_selector))
      return false;

    prev_sub_selector = sub_selector;
    sub_selector = sub_selector->TagHistory();
  }

  return true;
}

bool CSSSelector::HasLinkOrVisited() const {
  for (const CSSSelector* current = this; current;
       current = current->TagHistory()) {
    CSSSelector::PseudoType pseudo = current->GetPseudoType();
    if (pseudo == CSSSelector::kPseudoLink ||
        pseudo == CSSSelector::kPseudoVisited) {
      return true;
    }
    if (const CSSSelectorList* list = current->SelectorList()) {
      for (const CSSSelector* sub_selector = list->First(); sub_selector;
           sub_selector = CSSSelectorList::Next(*sub_selector)) {
        if (sub_selector->HasLinkOrVisited())
          return true;
      }
    }
  }
  return false;
}

void CSSSelector::SetNth(int a, int b) {
  CreateRareData();
  data_.rare_data_->bits_.nth_.a_ = a;
  data_.rare_data_->bits_.nth_.b_ = b;
}

bool CSSSelector::MatchNth(unsigned count) const {
  DCHECK(has_rare_data_);
  return data_.rare_data_->MatchNth(count);
}

bool CSSSelector::MatchesPseudoElement() const {
  for (const CSSSelector* current = this; current;
       current = current->TagHistory()) {
    if (current->Match() == kPseudoElement)
      return true;
    if (current->Relation() != kSubSelector)
      return false;
  }
  return false;
}

bool CSSSelector::IsTreeAbidingPseudoElement() const {
  return Match() == CSSSelector::kPseudoElement &&
         (GetPseudoType() == kPseudoBefore || GetPseudoType() == kPseudoAfter ||
          GetPseudoType() == kPseudoMarker ||
          GetPseudoType() == kPseudoPlaceholder);
}

bool CSSSelector::IsAllowedAfterPart() const {
  if (Match() != CSSSelector::kPseudoElement) {
    return false;
  }
  // Everything that makes sense should work following ::part. This list
  // restricts it to what has been tested.
  switch (GetPseudoType()) {
    case kPseudoBefore:
    case kPseudoAfter:
    case kPseudoPlaceholder:
    case kPseudoFirstLine:
    case kPseudoFirstLetter:
    case kPseudoSelection:
    case kPseudoTargetText:
      return true;
    default:
      return false;
  }
}

template <typename Functor>
static bool ForAnyInTagHistory(const Functor& functor,
                               const CSSSelector& selector) {
  for (const CSSSelector* current = &selector; current;
       current = current->TagHistory()) {
    if (functor(*current))
      return true;
    if (const CSSSelectorList* selector_list = current->SelectorList()) {
      for (const CSSSelector* sub_selector = selector_list->First();
           sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
        if (ForAnyInTagHistory(functor, *sub_selector))
          return true;
      }
    }
  }

  return false;
}

bool CSSSelector::HasContentPseudo() const {
  return ForAnyInTagHistory(
      [](const CSSSelector& selector) -> bool {
        return selector.RelationIsAffectedByPseudoContent();
      },
      *this);
}

bool CSSSelector::HasSlottedPseudo() const {
  return ForAnyInTagHistory(
      [](const CSSSelector& selector) -> bool {
        return selector.GetPseudoType() == CSSSelector::kPseudoSlotted;
      },
      *this);
}

bool CSSSelector::HasDeepCombinatorOrShadowPseudo() const {
  return ForAnyInTagHistory(
      [](const CSSSelector& selector) -> bool {
        return selector.Relation() == CSSSelector::kShadowDeep ||
               selector.GetPseudoType() == CSSSelector::kPseudoShadow;
      },
      *this);
}

bool CSSSelector::FollowsPart() const {
  const CSSSelector* previous = TagHistory();
  if (!previous)
    return false;
  return previous->GetPseudoType() == kPseudoPart;
}

bool CSSSelector::NeedsUpdatedDistribution() const {
  return ForAnyInTagHistory(
      [](const CSSSelector& selector) -> bool {
        return selector.RelationIsAffectedByPseudoContent() ||
               selector.GetPseudoType() == CSSSelector::kPseudoHostContext;
      },
      *this);
}

String CSSSelector::FormatPseudoTypeForDebugging(PseudoType type) {
  for (const auto& s : kPseudoTypeWithoutArgumentsMap) {
    if (s.type == type)
      return s.string;
  }
  for (const auto& s : kPseudoTypeWithArgumentsMap) {
    if (s.type == type)
      return s.string;
  }
  StringBuilder builder;
  builder.Append("pseudo-");
  builder.AppendNumber(static_cast<int>(type));
  return builder.ToString();
}

CSSSelector::RareData::RareData(const AtomicString& value)
    : matching_value_(value),
      serializing_value_(value),
      bits_(),
      attribute_(AnyQName()),
      argument_(g_null_atom) {}

CSSSelector::RareData::~RareData() = default;

// a helper function for checking nth-arguments
bool CSSSelector::RareData::MatchNth(unsigned unsigned_count) {
  // These very large values for aN + B or count can't ever match, so
  // give up immediately if we see them.
  int max_value = std::numeric_limits<int>::max() / 2;
  int min_value = std::numeric_limits<int>::min() / 2;
  if (UNLIKELY(unsigned_count > static_cast<unsigned>(max_value) ||
               NthAValue() > max_value || NthAValue() < min_value ||
               NthBValue() > max_value || NthBValue() < min_value))
    return false;

  int count = static_cast<int>(unsigned_count);
  if (!NthAValue())
    return count == NthBValue();
  if (NthAValue() > 0) {
    if (count < NthBValue())
      return false;
    return (count - NthBValue()) % NthAValue() == 0;
  }
  if (count > NthBValue())
    return false;
  return (NthBValue() - count) % (-NthAValue()) == 0;
}

void CSSSelector::SetPartNames(
    std::unique_ptr<Vector<AtomicString>> part_names) {
  CreateRareData();
  data_.rare_data_->part_names_ = std::move(part_names);
}

}  // namespace blink
