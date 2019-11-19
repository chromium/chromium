// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_CASCADE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_CASCADE_H_

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSCustomPropertyDeclaration;
class CSSParserContext;
class CSSProperty;
class CSSValue;
class CSSVariableData;
class CSSVariableReferenceValue;
class CustomProperty;
class StyleResolverState;

namespace cssvalue {

class CSSPendingSubstitutionValue;
class CSSPendingInterpolationValue;

}  // namespace cssvalue

// The StyleCascade is responsible for managing cascaded values [1], resolving
// dependencies between them, and applying the values to the ComputedStyle.
//
// Its usage pattern is:
//
//   const CSSPropertyName& name = ...;
//   const CSSValue* value1 = ...;
//   const CSSValue* value2 = ...;
//
//   StyleCascade cascade(state);
//   cascade.Add(name, value1, Priority(Origin::kAuthor));
//   cascade.Add(name, value2, Priority(Origin::kUA));
//   cascade.Apply(); // value1 is applied, value2 is ignored.
//
// [1] https://drafts.csswg.org/css-cascade/#cascade
class CORE_EXPORT StyleCascade {
  STACK_ALLOCATED();

  using CSSPendingSubstitutionValue = cssvalue::CSSPendingSubstitutionValue;
  using CSSPendingInterpolationValue = cssvalue::CSSPendingInterpolationValue;

 public:
  class Animator;
  class Resolver;
  class AutoLock;

  StyleCascade(StyleResolverState& state) : state_(state) {}

  static constexpr uint16_t kMaxCascadeOrder = ~static_cast<uint16_t>(0);

  // Represents the origin and importance criteria described by css-cascade [1].
  //
  // Higher values are more significant than lower values. The values are
  // chosen such that an important origin can be produced by inverting the bits
  // of the corresponding non-important origin.
  //
  // [1] https://www.w3.org/TR/css-cascade-3/#cascade-origin
  enum class Origin : uint8_t {
    kNone = 0,
    kUserAgent = 0b0001,
    kUser = 0b0010,
    kAuthor = 0b0011,
    kAnimation = 0b0100,
    kImportantAuthor = 0b1100,
    kImportantUser = 0b1101,
    kImportantUserAgent = 0b1110,
    kTransition = 0b10000,
  };

  // All important Origins (and only those) must have this bit set. This
  // provides a fast way to check if an Origin is important.
  static constexpr uint8_t kImportantBit = 0b1000;

  // The Priority class encapsulates a subset of the cascading criteria
  // described by css-cascade [1], and provides a way to compare priorities.
  //
  // It encompasses, from most significant to least significant: Origin (which
  // includes importance); tree order, which is a number representing the
  // shadow-including tree order [2]; and finally cascade order, which is
  // a monotonically increasing number increased by one every time something
  // is added to the cascade.
  //
  // The cascade order is initially kMaxCascadeOrder for an instance of
  // Priority; an actual value will be assigned by StyleCascade::Add.
  //
  // [1] https://drafts.csswg.org/css-cascade/#cascading
  // [2] https://drafts.csswg.org/css-scoping/#shadow-cascading
  class CORE_EXPORT Priority {
    DISALLOW_NEW();

   public:
    Priority() : Priority(Origin::kNone) {}
    // Deliberately implicit.
    Priority(Origin origin) : Priority(origin, 0) {}
    // For an explanation of 'tree_order', see css-scoping [1].
    // [1] https://drafts.csswg.org/css-scoping/#shadow-cascading
    Priority(Origin, uint16_t tree_order);

    Origin GetOrigin() const;
    bool HasOrigin() const { return GetOrigin() != Origin::kNone; }

    // This function is used to determine if an incoming Value should win
    // over the Value which already exists in the cascade.
    bool operator>=(const Priority&) const;

    // Returns a copy of this Priority, except that the non-important origin
    // has been converted to its important counterpart.
    //
    // Must be used with kUserAgent, kAuthor, and kAuthor only, as importance
    // does not apply to the other origins.
    //
    // https://drafts.csswg.org/css-cascade/#important
    inline Priority AddImportance() const {
      DCHECK_GE(GetOrigin(), Origin::kUserAgent);
      DCHECK_LE(GetOrigin(), Origin::kAuthor);
      // Flip Origin bits, converting non-important to important. We only
      // xor four bits here, because only those bits are in use by
      // k[Important,][User,UserAgent,Author].
      return Priority(priority_ ^ (static_cast<uint64_t>(0b1111) << 32));
    }

   private:
    friend class StyleCascade;
    friend class StyleCascadeTest;

    Priority(uint64_t priority) : priority_(priority) {}

    // Returns a copy of this Priority, with the cascade order set to the
    // specified value.
    //
    // For the purposes of StyleCascade::Add alone, we don't need to store the
    // cascade order at all, since the cascade order is implicit in the order
    // of the calls to ::Add. However, some properties unfortunately require
    // that we store the cascade order and act upon it Apply-time. This is
    // because we have multiple properties that mutate the same field on
    // ComputedStyle, hence the relative ordering must be preserved between
    // them to know which should be applied. (See class Filter).
    inline Priority WithCascadeOrder(uint16_t cascade_order) const {
      return Priority((priority_ & ~0xFFFF) | cascade_order);
    }

    // To make Priority comparisons fast, the origin, tree_order and
    // cascade_order are stored in a single uint64_t, as follows:
    //
    //  Bit  0-15: cascade_order
    //  Bit 16-31: tree_order
    //  Bit 32-39: Origin
    //
    // This way, the numeric value of priority_ can be compared directly
    // for all criteria simultaneously.
    uint64_t priority_;
  };

  // The Value class simply represents the data we store for each property
  // in the cascade. See StyleCascade::cascade_ field.
  class CORE_EXPORT Value {
    DISALLOW_NEW();

   public:
    // The empty Value is needed because we store it in a HashMap.
    Value() = default;
    Value(const CSSValue* value, Priority priority)
        : value_(value), priority_(priority) {}
    bool IsEmpty() const { return !priority_.HasOrigin(); }
    const CSSValue* GetValue() const { return value_; }
    const Priority& GetPriority() const { return priority_; }
    void Trace(blink::Visitor* visitor) { visitor->Trace(value_); }

   private:
    Member<const CSSValue> value_;
    Priority priority_;
  };

  // Add a Value to the cascade. The Value will either become the cascaded
  // value, or be discarded, depending on the Priority of the incoming value
  // vs. the Priority of the existing value.
  void Add(const CSSPropertyName&, const CSSValue*, Priority);

  // Applies all values currently in the cascade to the ComputedStyle.
  // Any CSSPendingInterpolationValues present in the cascade will be ignored.
  void Apply();
  // Applies all values currently in the cascade to the ComputedStyle,
  // dispatching any CSSPendingInterpolationValues to the given Animator.
  void Apply(Animator&);

  // Removes all kAnimationPropertyPriority properties from the cascade,
  // without applying the properties. This is used when pre-emptively copying
  // the cascade in case there are animations.
  //
  // TODO(crbug.com/985010): Improve with non-destructive Apply.
  void RemoveAnimationPriority();

  // The Filter class is responsible for resolving situations where
  // we have multiple (non-alias) properties in the cascade that mutates the
  // same fields on ComputedStyle.
  //
  // An example of this is writing-mode and -webkit-writing-mode, which
  // both result in ComputedStyle::SetWritingMode calls.
  //
  // When applying the cascade (applying each property/value pair to the
  // ComputedStyle), the order of the application is in the general case
  // not defined. (It is determined by the iteration order of the HashMap).
  // This means that if both writing-mode and -webkit-writing-mode exist in
  // the cascade, we would get non-deterministic behavior: the application order
  // would not be defined. To fix this, all Values pass through this Filter
  // before being applied.

  // The Filter stores the Priority of the Value that was previously applied
  // for a certain 'group' of properties (writing_mode_ is one such group).
  // When we're about to apply a Value, we only actually do so if the call  to
  // Filter::Add succeeds. If the call to Filter::Add does not succeed, it means
  // that we  have previously added a Value with higher Priority, and that the
  // current  Value must be ignored.

  // A key difference between discarding Values in the Filter, vs. discarding
  // them cascade-time (StyleCascade::Add), is that we are taking the cascade
  // order into account. This means that, if everything else is equal (origin,
  // tree order), the Value that entered the cascade last wins. This is crucial
  // to resolve situations like writing-mode and -webkit-writing-mode.

  // The Filter is also expected to resolve similar difficulties with
  // direction-aware properties in the future, although this is not yet
  // implemented.
  class CORE_EXPORT Filter {
    STACK_ALLOCATED();

   public:
    // Attempts to add a given property/value to the Filter. If this returns
    // true, the Value may be applied to the ComputedStyle. If not, it means
    // that we have previously applied a Value with higher Priority, and the
    // current Value must be discarded.
    bool Add(const CSSProperty& property, const Value&);

   private:
    Priority& GetSlot(const CSSProperty&);

    Priority none_;
    Priority writing_mode_;
    Priority zoom_;
  };

  // Resolver is an object passed on a stack during Apply. Its most important
  // job is to detect cycles during Apply (in general, keep track of which
  // properties we're currently applying).
  class CORE_EXPORT Resolver {
    STACK_ALLOCATED();

   public:
    // TODO(crbug.com/985047): Probably use a HashMap for this.
    using NameStack = Vector<CSSPropertyName, 8>;

    // A 'locked' property is a property we are in the process of applying.
    // In other words, once a property is locked, locking it again would form
    // a cycle, and is therefore an error.
    bool IsLocked(const CSSProperty&) const;
    bool IsLocked(const CSSPropertyName&) const;

    // We do not allow substitution of animation-tainted values into
    // an animation-affecting property.
    //
    // https://drafts.csswg.org/css-variables/#animation-tainted
    bool AllowSubstitution(CSSVariableData*) const;

   private:
    friend class AutoLock;
    friend class StyleCascade;
    friend class TestCascadeResolver;

    Resolver(Animator& animator) : animator_(animator) {}
    // If the given property is already being applied, returns true.
    // The return value is the same value you would get from InCycle(), and
    // is just returned for convenience.
    //
    // When a cycle has been detected, the Resolver will *persist the cycle
    // state* (i.e. InCycle() will continue to return true) until we reach
    // the start of the cycle.
    //
    // The cycle state is cleared by ~AutoLock, once we have moved far enough
    // up the stack.
    bool DetectCycle(const CSSProperty&);
    // Returns true whenever the Resolver is in a cycle state.
    // This DOES NOT detect cycles; the caller must call DetectCycle first.
    bool InCycle() const;

    NameStack stack_;
    Animator& animator_;
    wtf_size_t cycle_depth_ = kNotFound;
    Filter filter_;
  };

  // Automatically locks and unlocks the given property. (See
  // Resolver::IsLocked).
  class CORE_EXPORT AutoLock {
    STACK_ALLOCATED();

   public:
    AutoLock(const CSSProperty&, Resolver&);
    AutoLock(const CSSPropertyName&, Resolver&);
    ~AutoLock();

   private:
    Resolver& resolver_;
  };

  // Animator & CSSPendingInterpolationValue
  //
  // Blink's way of applying animations poses some difficulty for StyleCascade,
  // as much of the code that applies the animation effects completely bypasses
  // StyleBuilder; it sets the values on ComputedStyle directly. This prevents
  // those values from participating properly in the cascade.
  //
  // At the same time, we don't want to actually create CSSValues for the
  // animation effects, as this is (yet another?) unnecessary conversion, and
  // it produces unwanted GC pressure. To solve this problem, the cascading
  // and application aspects of interpolations are handled *separately*.
  //
  // CSSPendingInterpolationValue represents the cascading aspect of an
  // interpolation: this means that, once we know that an interpolation is
  // active for a given property, we add a CSSPendingInterpolationValue to the
  // cascade (with the appropriate Priority). Apply-time, we then ask the
  // Animator (see StyleAnimator) to actually apply the interpolated value
  // using the interpolation infrastructure.
  class CORE_EXPORT Animator {
   public:
    virtual void Apply(const CSSProperty&,
                       const CSSPendingInterpolationValue&,
                       Resolver&) = 0;
  };

  // Applying a CSSPendingInterpolationValue may involve resolving values,
  // since we may be applying a keyframe from e.g. "color: var(--x)" to
  // "color: var(--y)". Hence that code needs an entry point to the resolving
  // process.
  //
  // TODO(crbug.com/985023): This function has an associated const
  // violation, which isn't great. (This vilation was not introduced with
  // StyleCascade, however).
  //
  // See documentation the other Resolve* functions for what resolve means.
  const CSSValue* Resolve(const CSSPropertyName&, const CSSValue&, Resolver&);

 private:
  friend class TestCascade;

  // The maximum number of tokens that may be produced by a var()
  // reference.
  //
  // https://drafts.csswg.org/css-variables/#long-variables
  static const size_t kMaxSubstitutionTokens = 16384;

  // Applies kHighPropertyPriority properties.
  //
  // In theory, it would be possible for each property/value that contains
  // em/ch/etc to dynamically apply font-size (and related properties), but
  // in practice, it is very inconvenient to detect these dependencies. Hence,
  // we apply font-affecting properties (among others) before all the others.
  void ApplyHighPriority(Resolver&);

  // Apply a single property (including any dependencies).
  void Apply(const CSSPropertyName&);
  void Apply(const CSSPropertyName&, Resolver&);
  void Apply(const CSSProperty&, Resolver&);

  // True if the cascade currently holds the provided value for a given
  // property. Note that the value is compared by address.
  bool HasValue(const CSSPropertyName&, const CSSValue*) const;

  // Get current cascaded value for the specified property.
  const CSSValue* GetValue(const CSSPropertyName&) const;

  // If there is a cascaded value for the specified property, replace it
  // with the incoming value, maintaining the current cascade origin.
  // Has no effect if there is no cascaded value for the property.
  void ReplaceValue(const CSSPropertyName&, const CSSValue*);

  // Whether or not we are calculating the style for the root element.
  // We need to know this to detect cycles with 'rem' units.
  // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
  bool IsRootElement() const;

  // The TokenSequence class acts as a builder for CSSVariableData.
  //
  // However, actually building a CSSVariableData is optional; you can also
  // get a CSSParserTokenRange directly, which is useful when resolving a
  // CSSVariableData which won't ultimately end up in a CSSVariableData
  // (i.e. CSSVariableReferenceValue or CSSPendingSubstitutionValue).
  class TokenSequence {
    STACK_ALLOCATED();

   public:
    TokenSequence() = default;
    // Initialize a TokenSequence from a CSSVariableData, preparing the
    // TokenSequence for var() resolution.
    //
    // This copies everything except the tokens.
    explicit TokenSequence(const CSSVariableData*);

    bool IsAnimationTainted() const { return is_animation_tainted_; }
    CSSParserTokenRange TokenRange() const { return tokens_; }

    void Append(const TokenSequence&);
    void Append(const CSSVariableData*);
    void Append(const CSSParserToken&);

    scoped_refptr<CSSVariableData> BuildVariableData();

   private:
    Vector<CSSParserToken> tokens_;
    Vector<String> backing_strings_;
    // https://drafts.csswg.org/css-variables/#animation-tainted
    bool is_animation_tainted_ = false;
    // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
    bool has_font_units_ = false;
    bool has_root_font_units_ = false;

    // The base URL and charset are currently needed to calculate the computed
    // value of <url>-registered custom properties correctly.
    //
    // TODO(crbug.com/985013): Store CSSParserContext on
    // CSSCustomPropertyDeclaration and avoid this.
    //
    // https://drafts.css-houdini.org/css-properties-values-api-1/#relative-urls
    String base_url_;
    WTF::TextEncoding charset_;
  };

  // Resolving Values
  //
  // *Resolving* a value, means looking at the dependencies for a given
  // CSSValue, and ensuring that these dependencies are satisfied. The result
  // of a Resolve call is either the same CSSValue (e.g. if there were no
  // dependencies), or a new CSSValue with the dependencies resolved.
  //
  // For example, consider the following properties:
  //
  //  --x: 10px;
  //  --y: var(--x);
  //  width: var(--y);
  //
  // Here, to resolve 'width', the computed value of --y must be known. In
  // other words, we must first Apply '--y'. Hence, resolving 'width' will
  // Apply '--y' as a side-effect. (This process would then continue to '--x').

  const CSSValue* Resolve(const CSSProperty&, const CSSValue&, Resolver&);
  const CSSValue* ResolveCustomProperty(const CSSProperty&,
                                        const CSSCustomPropertyDeclaration&,
                                        Resolver&);
  const CSSValue* ResolveVariableReference(const CSSProperty&,
                                           const CSSVariableReferenceValue&,
                                           Resolver&);
  const CSSValue* ResolvePendingSubstitution(const CSSProperty&,
                                             const CSSPendingSubstitutionValue&,
                                             Resolver&);

  scoped_refptr<CSSVariableData> ResolveVariableData(CSSVariableData*,
                                                     Resolver&);

  // The Resolve*Into functions either resolve dependencies, append to the
  // TokenSequence accordingly, and return true; or it returns false when
  // the TokenSequence is "invalid at computed-value time" [1]. This happens
  // when there was a reference to an invalid/missing custom property, or when a
  // cycle was detected.
  //
  // [1] https://drafts.csswg.org/css-variables/#invalid-at-computed-value-time

  bool ResolveTokensInto(CSSParserTokenRange, Resolver&, TokenSequence&);
  bool ResolveVarInto(CSSParserTokenRange, Resolver&, TokenSequence&);
  bool ResolveEnvInto(CSSParserTokenRange, Resolver&, TokenSequence&);

  CSSVariableData* GetVariableData(const CustomProperty&) const;
  CSSVariableData* GetEnvironmentVariable(const AtomicString&) const;
  const CSSParserContext* GetParserContext(const CSSVariableReferenceValue&);

  // Detects if the given property/data depends on the font-size property
  // of the Element we're calculating the style for.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
  bool HasFontSizeDependency(const CustomProperty&, CSSVariableData*) const;
  // The fallback must match the syntax of the custom property, otherwise the
  // the declaration is "invalid at computed-value time".'
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
  bool ValidateFallback(const CustomProperty&, CSSParserTokenRange) const;
  // Marks the CustomProperty as referenced by something. Needed to avoid
  // animating these custom properties on the compositor, and to disable the
  // matched properties cache in some cases.
  void MarkReferenced(const CustomProperty&);

  StyleResolverState& state_;
  HeapHashMap<CSSPropertyName, Value> cascade_;
  uint16_t order_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_CASCADE_H_
