/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_H_

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

namespace blink {

class CSSParserContext;
class CSSSelectorList;
class Document;
class StyleRule;

// This class represents a simple selector for a StyleRule.

// CSS selector representation is somewhat complicated and subtle. A
// representative list of selectors is in CSSSelectorTest; run it in a debug
// build to see useful debugging output.
//
// ** NextSimpleSelector() and Relation():
//
// Selectors are represented as an array of simple selectors (defined more
// or less according to
// http://www.w3.org/TR/css3-selectors/#simple-selectors-dfn).
// The NextInSimpleSelector() member function returns the next simple selector
// in the list. The Relation() member function returns the relationship of the
// current simple selector to the one in NextSimpleSelector(). For example, the
// CSS selector .a.b #c is represented as:
//
// SelectorText(): .a.b #c
// --> (relation == kDescendant)
//   SelectorText(): .a.b
//   --> (relation == kSubSelector)
//     SelectorText(): .b
//
// The ordering of the simple selectors varies depending on the situation.
// * Relations using combinators
//   (http://www.w3.org/TR/css3-selectors/#combinators), such as descendant,
//   sibling, etc., are parsed right-to-left (in the example above, this is why
//   #c is earlier in the simple selector chain than .a.b).
// * SubSelector relations are parsed left-to-right, such as the .a.b example
//   above.
// * ShadowPseudo relations are parsed right-to-left. Example:
//   summary::-webkit-details-marker is parsed as: selectorText():
//   summary::-webkit-details-marker --> (relation == ShadowPseudo)
//   selectorText(): summary
//
// ** match():
//
// The match of the current simple selector tells us the type of selector, such
// as class, id, tagname, or pseudo-class. Inline comments in the Match enum
// give examples of when each type would occur.
//
// ** value(), attribute():
//
// value() tells you the value of the simple selector. For example, for class
// selectors, value() will tell you the class string, and for id selectors it
// will tell you the id(). See below for the special case of attribute
// selectors.
//
// ** Attribute selectors.
//
// Attribute selectors return the attribute name in the attribute() method. The
// value() method returns the value matched against in case of selectors like
// [attr="value"].
//
class CORE_EXPORT CSSSelector {
  // CSSSelector typically lives on Oilpan; either in StyleRule's
  // AdditionalBytes, as part of CSSSelectorList, or (during parsing)
  // in a HeapVector. However, it is never really allocated as a separate
  // Oilpan object, so it does not inherit from GarbageCollected.
  DISALLOW_NEW();

 public:
  CSSSelector();

  // NOTE: Will not deep-copy the selector list, if any.
  CSSSelector(const CSSSelector&);

  CSSSelector(CSSSelector&&);
  explicit CSSSelector(const QualifiedName&, bool tag_is_implicit = false);
  explicit CSSSelector(const StyleRule* parent_rule, bool is_implicit);
  explicit CSSSelector(const AtomicString& pseudo_name, bool is_implicit);

  ~CSSSelector();

  String SelectorText() const;
  String SimpleSelectorTextForDebug() const;

  CSSSelector& operator=(const CSSSelector&) = delete;
  CSSSelector& operator=(CSSSelector&&);
  bool operator==(const CSSSelector&) const = delete;
  bool operator!=(const CSSSelector&) const = delete;

  static constexpr unsigned kIdSpecificity = 0x010000;
  static constexpr unsigned kClassLikeSpecificity = 0x000100;
  static constexpr unsigned kTagSpecificity = 0x000001;

  static constexpr unsigned kMaxValueMask = 0xffffff;
  static constexpr unsigned kIdMask = 0xff0000;
  static constexpr unsigned kClassMask = 0x00ff00;
  static constexpr unsigned kElementMask = 0x0000ff;

  // http://www.w3.org/TR/css3-selectors/#specificity
  // We use 256 as the base of the specificity number system.
  unsigned Specificity() const;
  // Returns specificity components in decreasing order of significance.
  std::array<uint8_t, 3> SpecificityTuple() const;

  /* how the attribute value has to match.... Default is Exact */
  enum MatchType {
    kUnknown,
    kInvalidList,       // Used as a marker in CSSSelectorList.
    kTag,               // Example: div
    kId,                // Example: #id
    kClass,             // Example: .class
    kPseudoClass,       // Example: :nth-child(2)
    kPseudoElement,     // Example: ::first-line
    kPagePseudoClass,   // ??
    kAttributeExact,    // Example: E[foo="bar"]
    kAttributeSet,      // Example: E[foo]
    kAttributeHyphen,   // Example: E[foo|="bar"]
    kAttributeList,     // Example: E[foo~="bar"]
    kAttributeContain,  // css3: E[foo*="bar"]
    kAttributeBegin,    // css3: E[foo^="bar"]
    kAttributeEnd,      // css3: E[foo$="bar"]
    kFirstAttributeSelectorMatch = kAttributeExact,
  };

  enum RelationType {
    // No combinator. Used between simple selectors within the same compound.
    kSubSelector,
    // "Space" combinator
    kDescendant,
    // > combinator
    kChild,
    // + combinator
    kDirectAdjacent,
    // ~ combinator
    kIndirectAdjacent,
    // The relation types below are implicit combinators inserted at parse time
    // before pseudo elements which match another flat tree element than the
    // rest of the compound.
    //
    // Implicit combinator inserted before pseudo elements matching an element
    // inside a UA shadow tree. This combinator allows the selector matching to
    // cross a shadow root.
    //
    // Examples:
    // input::placeholder, video::cue(i), video::--webkit-media-controls-panel
    kUAShadow,
    // Implicit combinator inserted before ::slotted() selectors.
    kShadowSlot,
    // Implicit combinator inserted before ::part() selectors which allows
    // matching a ::part in shadow-including descendant tree for #host in
    // "#host::part(button)".
    kShadowPart,

    // leftmost "Space" combinator of relative selector
    kRelativeDescendant,
    // leftmost > combinator of relative selector
    kRelativeChild,
    // leftmost + combinator of relative selector
    kRelativeDirectAdjacent,
    // leftmost ~ combinator of relative selector
    kRelativeIndirectAdjacent,

    // The following applies to selectors within @scope
    // (see CSSNestingType::kScope):
    //
    // The kScopeActivation relation is implicitly inserted parse-time before
    // any compound selector which contains either :scope or the parent
    // selector (&). When encountered during selector matching,
    // kScopeActivation will try the to match the rest of the selector (i.e. the
    // NextSimpleSelector from that point) using activation roots as the :scope
    // element, trying the nearest activation root first.
    //
    // See also StyleScopeActivation.
    kScopeActivation,
  };

  enum PseudoType {
    kPseudoActive,
    kPseudoActiveViewTransition,
    kPseudoAfter,
    kPseudoAny,
    kPseudoAnyLink,
    kPseudoAutofill,
    kPseudoAutofillPreviewed,
    kPseudoAutofillSelected,
    kPseudoBackdrop,
    kPseudoBefore,
    kPseudoChecked,
    kPseudoCornerPresent,
    kPseudoDecrement,
    kPseudoDefault,
    kPseudoDetailsContent,
    kPseudoDetailsSummary,
    kPseudoDialogInTopLayer,
    kPseudoDisabled,
    kPseudoDoubleButton,
    kPseudoDrag,
    kPseudoEmpty,
    kPseudoEnabled,
    kPseudoEnd,
    kPseudoFileSelectorButton,
    kPseudoFirstChild,
    kPseudoFirstLetter,
    kPseudoFirstLine,
    kPseudoFirstOfType,
    kPseudoFirstPage,
    kPseudoFocus,
    kPseudoFocusVisible,
    kPseudoFocusWithin,
    kPseudoFullPageMedia,
    kPseudoHorizontal,
    kPseudoHover,
    kPseudoIncrement,
    kPseudoIndeterminate,
    kPseudoInvalid,
    kPseudoIs,
    kPseudoLang,
    kPseudoLastChild,
    kPseudoLastOfType,
    kPseudoLeftPage,
    kPseudoLink,
    kPseudoMarker,
    kPseudoModal,
    kPseudoNoButton,
    kPseudoNot,
    kPseudoNthChild,  // Includes :nth-child(An+B of <selector>)
    kPseudoNthLastChild,
    kPseudoNthLastOfType,
    kPseudoNthOfType,
    kPseudoOnlyChild,
    kPseudoOnlyOfType,
    kPseudoOptional,
    kPseudoParent,  // Written as & (in nested rules).
    kPseudoPart,
    kPseudoPermissionGranted,
    kPseudoPlaceholder,
    kPseudoPlaceholderShown,
    kPseudoReadOnly,
    kPseudoReadWrite,
    kPseudoRequired,
    kPseudoResizer,
    kPseudoRightPage,
    kPseudoRoot,
    kPseudoScope,
    kPseudoScrollbar,
    kPseudoScrollbarButton,
    kPseudoScrollbarCorner,
    kPseudoScrollbarThumb,
    kPseudoScrollbarTrack,
    kPseudoScrollbarTrackPiece,
    kPseudoSelection,
    kPseudoSelectorFragmentAnchor,
    kPseudoSingleButton,
    kPseudoStart,
    kPseudoState,
    kPseudoTarget,
    kPseudoUnknown,
    // Something that was unparsable, but contained either a nesting
    // selector (&), or a :scope pseudo-class, and must therefore be kept
    // for serialization purposes.
    kPseudoUnparsed,
    kPseudoUserInvalid,
    kPseudoUserValid,
    kPseudoValid,
    kPseudoVertical,
    kPseudoVisited,
    kPseudoWebKitAutofill,
    kPseudoWebkitAnyLink,
    kPseudoWhere,
    kPseudoWindowInactive,
    // TODO(foolip): When the unprefixed Fullscreen API is enabled, merge
    // kPseudoFullScreen and kPseudoFullscreen into one. (kPseudoFullscreen is
    // controlled by the FullscreenUnprefixed REF, but is otherwise an alias.)
    kPseudoFullScreen,
    kPseudoFullScreenAncestor,
    kPseudoFullscreen,
    kPseudoInRange,
    kPseudoOutOfRange,
    kPseudoPaused,
    kPseudoPictureInPicture,
    kPseudoPlaying,
    kPseudoXrOverlay,
    // Pseudo elements in UA ShadowRoots. Available in any stylesheets.
    kPseudoWebKitCustomElement,
    // Pseudo elements in UA ShadowRoots. Available only in UA stylesheets.
    kPseudoBlinkInternalElement,
    kPseudoClosed,
    kPseudoCue,
    kPseudoDefined,
    kPseudoDir,
    kPseudoFutureCue,
    kPseudoGrammarError,
    kPseudoHas,
    kPseudoHasDatalist,
    kPseudoHighlight,
    kPseudoHost,
    kPseudoHostContext,
    kPseudoHostHasAppearance,
    kPseudoIsHtml,
    kPseudoListBox,
    kPseudoMultiSelectFocus,
    kPseudoOpen,
    kPseudoPastCue,
    kPseudoPopoverInTopLayer,
    kPseudoPopoverOpen,
    kPseudoRelativeAnchor,
    kPseudoSlotted,
    kPseudoSpatialNavigationFocus,
    kPseudoSpatialNavigationInterest,
    kPseudoSpellingError,
    kPseudoTargetText,
    // Always matches. See SetTrue().
    kPseudoTrue,
    kPseudoVideoPersistent,
    kPseudoVideoPersistentAncestor,

    // The following selectors are used to target pseudo elements created for
    // ViewTransition.
    // See
    // https://github.com/WICG/view-transitions/blob/main/explainer.md
    // for details.
    kPseudoViewTransition,
    kPseudoViewTransitionGroup,
    kPseudoViewTransitionImagePair,
    kPseudoViewTransitionNew,
    kPseudoViewTransitionOld,
  };

  enum class AttributeMatchType : int {
    kCaseSensitive,
    kCaseInsensitive,
    kCaseSensitiveAlways,
  };

  PseudoType GetPseudoType() const {
    return static_cast<PseudoType>(pseudo_type_);
  }

  void UpdatePseudoType(const AtomicString&,
                        const CSSParserContext&,
                        bool has_arguments,
                        CSSParserMode);
  void SetUnparsedPlaceholder(CSSNestingType, const AtomicString&);
  // If this simple selector contains a parent selector (&), returns kNesting.
  // Otherwise, if this simple selector contains a :scope pseudo-class,
  // returns kScope. Otherwise, returns kNone.
  //
  // Note that this means that a selector which contains both '&' and :scope
  // (which can happen for kPseudoUnparsed) will return kNesting. This is OK,
  // since any selector which is nest-containing is also treated as
  // scope-containing during parsing.
  CSSNestingType GetNestingType() const;
  // Set this CSSSelector as a :true pseudo-class. This can be useful if you
  // need to insert a special RelationType into a selector's NextSimpleSelector,
  // but lack any existing/suitable CSSSelector to attach that RelationType to.
  //
  // Note that :true is always implicit (see IsImplicit).
  void SetTrue();
  void UpdatePseudoPage(const AtomicString&, const Document*);
  static PseudoType NameToPseudoType(const AtomicString&,
                                     bool has_arguments,
                                     const Document* document);
  static PseudoId GetPseudoId(PseudoType);

  // Replaces the parent pointer held by kPseudoParent selectors found
  // within this simple selector (including inner selector lists).
  //
  // See also StyleRule::Reparent().
  void Reparent(StyleRule* old_parent, StyleRule* new_parent);

  // Selectors are kept in an array by CSSSelectorList. The next component of
  // the selector is the next item in the array.
  const CSSSelector* NextSimpleSelector() const {
    return is_last_in_complex_selector_ ? nullptr : this + 1;
  }
  CSSSelector* NextSimpleSelector() {
    return is_last_in_complex_selector_ ? nullptr : this + 1;
  }

  static const AtomicString& UniversalSelectorAtom() { return g_null_atom; }
  const QualifiedName& TagQName() const;
  const StyleRule* ParentRule() const;  // Only valid for kPseudoParent.
  const AtomicString& Value() const;
  const AtomicString& SerializingValue() const;

  // WARNING: Use of QualifiedName by attribute() is a lie.
  // attribute() will return a QualifiedName with prefix and namespaceURI
  // set to g_star_atom to mean "matches any namespace". Be very careful
  // how you use the returned QualifiedName.
  // http://www.w3.org/TR/css3-selectors/#attrnmsp
  const QualifiedName& Attribute() const;
  AttributeMatchType AttributeMatch() const;
  bool IsCaseSensitiveAttribute() const;
  // Returns the argument of a parameterized selector. For example, :lang(en-US)
  // would have an argument of en-US.
  // Note that :nth-* selectors don't store an argument and just store the
  // numbers.
  const AtomicString& Argument() const {
    return has_rare_data_ ? data_.rare_data_->argument_ : g_null_atom;
  }
  const CSSSelectorList* SelectorList() const {
    return has_rare_data_ ? data_.rare_data_->selector_list_.Get() : nullptr;
  }
  // Similar to SelectorList(), but also works for kPseudoParent
  // (i.e., nested selectors); on &, will give the parent's selector list.
  // Will return nullptr if no such list exists (e.g. if we are not a
  // pseudo selector at all), or if we are a & rule that's in a non-nesting
  // context (which is valid, but won't match anything).
  const CSSSelector* SelectorListOrParent() const;
  const Vector<AtomicString>& IdentList() const {
    CHECK(has_rare_data_ && data_.rare_data_->ident_list_);
    return *data_.rare_data_->ident_list_;
  }
  bool ContainsPseudoInsideHasPseudoClass() const {
    return has_rare_data_ ? data_.rare_data_->bits_.has_.contains_pseudo_
                          : false;
  }
  bool ContainsComplexLogicalCombinationsInsideHasPseudoClass() const {
    return has_rare_data_ ? data_.rare_data_->bits_.has_
                                .contains_complex_logical_combinations_
                          : false;
  }

#if DCHECK_IS_ON()
  void Show() const;
  void Show(int indent) const;
#endif  // DCHECK_IS_ON()

  bool IsASCIILower(const AtomicString& value);
  void SetValue(const AtomicString&, bool match_lower_case);
  void SetAttribute(const QualifiedName&, AttributeMatchType);
  void SetArgument(const AtomicString&);
  void SetSelectorList(CSSSelectorList*);
  void SetIdentList(std::unique_ptr<Vector<AtomicString>>);
  void SetContainsPseudoInsideHasPseudoClass();
  void SetContainsComplexLogicalCombinationsInsideHasPseudoClass();

  void SetNth(int a, int b, CSSSelectorList* sub_selector);
  bool MatchNth(unsigned count) const;

  static bool IsAdjacentRelation(RelationType relation) {
    return relation == kDirectAdjacent || relation == kIndirectAdjacent;
  }
  bool IsAttributeSelector() const {
    return match_ >= kFirstAttributeSelectorMatch;
  }
  bool IsHostPseudoClass() const {
    return pseudo_type_ == kPseudoHost || pseudo_type_ == kPseudoHostContext;
  }
  bool IsUserActionPseudoClass() const;
  bool IsIdClassOrAttributeSelector() const;

  RelationType Relation() const { return static_cast<RelationType>(relation_); }
  void SetRelation(RelationType relation) {
    relation_ = relation;
    DCHECK_EQ(static_cast<RelationType>(relation_),
              relation);  // using a bitfield.
  }

  MatchType Match() const { return static_cast<MatchType>(match_); }
  void SetMatch(MatchType match) {
    match_ = match;
    DCHECK_EQ(static_cast<MatchType>(match_), match);  // using a bitfield.
  }

  bool IsLastInSelectorList() const { return is_last_in_selector_list_; }
  void SetLastInSelectorList(bool is_last) {
    is_last_in_selector_list_ = is_last;
  }

  bool IsLastInComplexSelector() const { return is_last_in_complex_selector_; }
  void SetLastInComplexSelector(bool is_last) {
    is_last_in_complex_selector_ = is_last;
  }

  // https://drafts.csswg.org/selectors/#compound
  bool IsCompound() const;

  enum LinkMatchMask {
    kMatchLink = 1,
    kMatchVisited = 2,
    kMatchAll = kMatchLink | kMatchVisited
  };

  // True if :link or :visited pseudo-classes are found anywhere in
  // the selector.
  bool HasLinkOrVisited() const;

  bool IsForPage() const { return is_for_page_; }
  void SetForPage() { is_for_page_ = true; }

  bool IsCoveredByBucketing() const { return is_covered_by_bucketing_; }
  void SetCoveredByBucketing(bool value) { is_covered_by_bucketing_ = value; }

  bool MatchesPseudoElement() const;
  bool IsTreeAbidingPseudoElement() const;
  bool IsAllowedAfterPart() const;

  // Returns true if the immediately preceeding simple selector is ::part.
  bool FollowsPart() const;
  // Returns true if the immediately preceeding simple selector is ::slotted.
  bool FollowsSlotted() const;

  // True if the selector was added implicitly. This can happen for e.g.
  // nested rules that would otherwise lack the nesting selector (&).
  bool IsImplicit() const { return is_implicitly_added_; }

  // Returns true for simple selectors whose evaluation depends on DOM tree
  // position like :first-of-type and :nth-child().
  bool IsChildIndexedSelector() const;

  void Trace(Visitor* visitor) const;

  static String FormatPseudoTypeForDebugging(PseudoType);

 private:
  unsigned relation_ : 4;     // enum RelationType
  unsigned match_ : 4;        // enum MatchType
  unsigned pseudo_type_ : 8;  // enum PseudoType
  unsigned is_last_in_selector_list_ : 1;
  unsigned is_last_in_complex_selector_ : 1;
  unsigned has_rare_data_ : 1;
  unsigned is_for_page_ : 1;
  unsigned is_implicitly_added_ : 1;

  // If set, we don't need to check this simple selector when matching;
  // it will always match, since we can only see the selector if we
  // checked a given bucket. For instance, if we have a rule like
  // #foo.bar, it will be put in the rule set bucket for #foo
  // (ID selectors are prioritized over nearly everything), and we can
  // mark #foo as covered by bucketing (but still need to check .bar).
  // Of course, this doesn't cover ancestors or siblings; if we have
  // something like .c .c.c, only the two rightmost selectors will get
  // this bit set. Also, we often get into things like namespaces which
  // makes this more conservative than we'd like (bucketing on e.g.
  // tag names do not generally care about it).
  //
  // Furthermore, as a convention, matching such a rule would never set
  // flags in MatchResult.
  //
  // This always starts out false, and is set when we bucket a given
  // RuleData (by calling MarkAsCoveredByBucketing()).
  unsigned is_covered_by_bucketing_ : 1;

  void SetPseudoType(PseudoType pseudo_type) {
    pseudo_type_ = pseudo_type;
    DCHECK_EQ(static_cast<PseudoType>(pseudo_type_),
              pseudo_type);  // using a bitfield.
  }

  unsigned SpecificityForOneSelector() const;
  unsigned SpecificityForPage() const;
  bool SerializeSimpleSelector(StringBuilder& builder) const;
  const CSSSelector* SerializeCompound(StringBuilder&) const;

  struct RareData : public GarbageCollected<RareData> {
    explicit RareData(const AtomicString& value);
    ~RareData();

    bool MatchNth(unsigned count);
    int NthAValue() const { return bits_.nth_.a_; }
    int NthBValue() const { return bits_.nth_.b_; }

    AtomicString matching_value_;
    AtomicString serializing_value_;
    union {
      struct {
        int a_;  // Used for :nth-*
        int b_;  // Used for :nth-*
      } nth_;

      struct {
        AttributeMatchType
            attribute_match_;  // used for attribute selector (with value)
        bool is_case_sensitive_attribute_;
      } attr_;

      struct {
        // Used for :has() with pseudos in its argument. e.g. :has(:hover)
        bool contains_pseudo_;

        // Used for :has() with logical combinations (:is(), :where(), :not())
        // containing complex selector in its argument. e.g. :has(:is(.a .b))
        bool contains_complex_logical_combinations_;
      } has_;

      // See GetNestingType.
      CSSNestingType unparsed_nesting_type_;
    } bits_;
    QualifiedName attribute_;  // Used for attribute selector
    AtomicString argument_;    // Used for :contains, :lang, :dir, etc.
    Member<CSSSelectorList>
        selector_list_;  // Used :is, :not, :-webkit-any, etc.
    std::unique_ptr<Vector<AtomicString>>
        ident_list_;  // Used for ::part(), :active-view-transition().

    void Trace(Visitor* visitor) const;
  };
  void CreateRareData();

  // The type tag for DataUnion is actually inferred from multiple state
  // variables in the containing CSSSelector using the following rules.
  //
  //  if (match_ == kTag) {
  //     /* data_.tag_q_name_ is valid */
  //  } else if (match_ == kPseudoClass && pseudo_type_ == kPseudoParent) {
  //     /* data_.parent_rule_ is valid */
  //  } else if (has_rare_data_) {
  //     /* data_.rare_data_ is valid */
  //  } else {
  //     /* data_.value_ is valid */
  //  }
  //
  // Note that it is important to placement-new and explicitly destruct the
  // fields when shifting between types tags for a DataUnion! Otherwise there
  // will be undefined behavior! This luckily only happens when transitioning
  // from a normal |value_| to a |rare_data_|.
  GC_PLUGIN_IGNORE("crbug.com/1146383")
  union DataUnion {
    enum ConstructUninitializedTag { kConstructUninitialized };
    explicit DataUnion(ConstructUninitializedTag) {}

    enum ConstructEmptyValueTag { kConstructEmptyValue };
    explicit DataUnion(ConstructEmptyValueTag) : value_() {}

    // A string `value` is used by many different selectors to store the string
    // part of the selector. For example the name of a pseudo class (without
    // the colon), the class name of a class selector (without the dot),
    // the attribute of an attribute selector (without the brackets), etc.
    explicit DataUnion(const AtomicString& value) : value_(value) {}

    explicit DataUnion(const QualifiedName& tag_q_name)
        : tag_q_name_(tag_q_name) {}

    explicit DataUnion(const StyleRule* parent_rule)
        : parent_rule_(parent_rule) {}

    ~DataUnion() {}

    AtomicString value_;
    QualifiedName tag_q_name_;
    Member<RareData> rare_data_;
    Member<const StyleRule> parent_rule_;  // For & (parent in nest).
  } data_;
};

inline const QualifiedName& CSSSelector::Attribute() const {
  DCHECK(IsAttributeSelector());
  DCHECK(has_rare_data_);
  return data_.rare_data_->attribute_;
}

inline CSSSelector::AttributeMatchType CSSSelector::AttributeMatch() const {
  DCHECK(IsAttributeSelector());
  DCHECK(has_rare_data_);
  return data_.rare_data_->bits_.attr_.attribute_match_;
}

inline bool CSSSelector::IsCaseSensitiveAttribute() const {
  DCHECK(IsAttributeSelector());
  DCHECK(has_rare_data_);
  return data_.rare_data_->bits_.attr_.is_case_sensitive_attribute_;
}

inline bool CSSSelector::IsASCIILower(const AtomicString& value) {
  for (wtf_size_t i = 0; i < value.length(); ++i) {
    if (IsASCIIUpper(value[i])) {
      return false;
    }
  }
  return true;
}

inline void CSSSelector::SetValue(const AtomicString& value,
                                  bool match_lower_case = false) {
  DCHECK_NE(match_, static_cast<unsigned>(kTag));
  DCHECK(!(match_ == kPseudoClass && pseudo_type_ == kPseudoParent));
  if (match_lower_case && !has_rare_data_ && !IsASCIILower(value)) {
    CreateRareData();
  }

  if (!has_rare_data_) {
    data_.value_ = value;
    return;
  }
  data_.rare_data_->matching_value_ =
      match_lower_case ? value.LowerASCII() : value;
  data_.rare_data_->serializing_value_ = value;
}

inline CSSSelector::CSSSelector()
    : relation_(kSubSelector),
      match_(kUnknown),
      pseudo_type_(kPseudoUnknown),
      is_last_in_selector_list_(false),
      is_last_in_complex_selector_(false),
      has_rare_data_(false),
      is_for_page_(false),
      is_implicitly_added_(false),
      is_covered_by_bucketing_(false),
      data_(DataUnion::kConstructEmptyValue) {}

inline CSSSelector::CSSSelector(const QualifiedName& tag_q_name,
                                bool tag_is_implicit)
    : relation_(kSubSelector),
      match_(kTag),
      pseudo_type_(kPseudoUnknown),
      is_last_in_selector_list_(false),
      is_last_in_complex_selector_(false),
      has_rare_data_(false),
      is_for_page_(false),
      is_implicitly_added_(tag_is_implicit),
      is_covered_by_bucketing_(false),
      data_(tag_q_name) {}

inline CSSSelector::CSSSelector(const StyleRule* parent_rule, bool is_implicit)
    : relation_(kSubSelector),
      match_(kPseudoClass),
      pseudo_type_(kPseudoParent),
      is_last_in_selector_list_(false),
      is_last_in_complex_selector_(false),
      has_rare_data_(false),
      is_for_page_(false),
      is_implicitly_added_(is_implicit),
      is_covered_by_bucketing_(false),
      data_(parent_rule) {}

inline CSSSelector::CSSSelector(const AtomicString& pseudo_name,
                                bool is_implicit)
    : relation_(kSubSelector),
      match_(kPseudoClass),
      pseudo_type_(NameToPseudoType(pseudo_name,
                                    /* has_arguments */ false,
                                    /* document */ nullptr)),
      is_last_in_selector_list_(false),
      is_last_in_complex_selector_(false),
      has_rare_data_(false),
      is_for_page_(false),
      is_implicitly_added_(is_implicit),
      is_covered_by_bucketing_(false),
      data_(pseudo_name) {}

inline CSSSelector::CSSSelector(const CSSSelector& o)
    : relation_(o.relation_),
      match_(o.match_),
      pseudo_type_(o.pseudo_type_),
      is_last_in_selector_list_(o.is_last_in_selector_list_),
      is_last_in_complex_selector_(o.is_last_in_complex_selector_),
      has_rare_data_(o.has_rare_data_),
      is_for_page_(o.is_for_page_),
      is_implicitly_added_(o.is_implicitly_added_),
      is_covered_by_bucketing_(o.is_covered_by_bucketing_),
      data_(DataUnion::kConstructUninitialized) {
  if (o.match_ == kTag) {
    new (&data_.tag_q_name_) QualifiedName(o.data_.tag_q_name_);
  } else if (o.match_ == kPseudoClass && o.pseudo_type_ == kPseudoParent) {
    data_.parent_rule_ = o.data_.parent_rule_;
  } else if (o.has_rare_data_) {
    data_.rare_data_ = o.data_.rare_data_;  // Oilpan-managed.
  } else {
    new (&data_.value_) AtomicString(o.data_.value_);
  }
}

inline CSSSelector::CSSSelector(CSSSelector&& o)
    : data_(DataUnion::kConstructUninitialized) {
  // Seemingly Clang started generating terrible code for the obvious move
  // constructor (i.e., using similar code as in the copy constructor above)
  // after moving to Oilpan, copying the bits one by one. We already allow
  // memcpy + memset by traits, so we can do it by ourselves, too.
  memcpy(this, &o, sizeof(*this));
  memset(&o, 0, sizeof(o));
}

inline CSSSelector::~CSSSelector() {
  if (match_ == kTag) {
    data_.tag_q_name_.~QualifiedName();
  } else if (match_ == kPseudoClass && pseudo_type_ == kPseudoParent)
    ;  // Nothing to do.
  else if (has_rare_data_)
    ;  // Nothing to do.
  else {
    data_.value_.~AtomicString();
  }
}

inline CSSSelector& CSSSelector::operator=(CSSSelector&& other) {
  this->~CSSSelector();
  new (this) CSSSelector(std::move(other));
  return *this;
}

inline const QualifiedName& CSSSelector::TagQName() const {
  DCHECK_EQ(match_, static_cast<unsigned>(kTag));
  return data_.tag_q_name_;
}

inline const StyleRule* CSSSelector::ParentRule() const {
  DCHECK_EQ(match_, static_cast<unsigned>(kPseudoClass));
  DCHECK_EQ(pseudo_type_, static_cast<unsigned>(kPseudoParent));
  return data_.parent_rule_.Get();
}

inline const AtomicString& CSSSelector::Value() const {
  DCHECK_NE(match_, static_cast<unsigned>(kTag));
  if (has_rare_data_) {
    return data_.rare_data_->matching_value_;
  }
  return data_.value_;
}

inline const AtomicString& CSSSelector::SerializingValue() const {
  DCHECK_NE(match_, static_cast<unsigned>(kTag));
  if (has_rare_data_) {
    return data_.rare_data_->serializing_value_;
  }
  return data_.value_;
}

inline bool CSSSelector::IsUserActionPseudoClass() const {
  return pseudo_type_ == kPseudoHover || pseudo_type_ == kPseudoActive ||
         pseudo_type_ == kPseudoFocus || pseudo_type_ == kPseudoDrag ||
         pseudo_type_ == kPseudoFocusWithin ||
         pseudo_type_ == kPseudoFocusVisible;
}

inline bool CSSSelector::IsIdClassOrAttributeSelector() const {
  return IsAttributeSelector() || Match() == CSSSelector::kId ||
         Match() == CSSSelector::kClass;
}

inline void swap(CSSSelector& a, CSSSelector& b) {
  char tmp[sizeof(CSSSelector)];
  memcpy(tmp, &a, sizeof(CSSSelector));
  memcpy(&a, &b, sizeof(CSSSelector));
  memcpy(&b, tmp, sizeof(CSSSelector));
}

// Converts descendant to relative descendant, child to relative child
// and so on. Subselector is converted to relative descendant.
// All others that don't have a corresponding relative combinator will
// call NOTREACHED().
CSSSelector::RelationType ConvertRelationToRelative(
    CSSSelector::RelationType relation);

// Returns the maximum specificity within a list of selectors. This is typically
// used to calculate the specificity of selectors that have an inner selector
// list, e.g. :is(), :where() etc.
unsigned MaximumSpecificity(const CSSSelector* first_selector);

}  // namespace blink

namespace WTF {
template <>
struct VectorTraits<blink::CSSSelector> : VectorTraitsBase<blink::CSSSelector> {
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};
}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_H_
