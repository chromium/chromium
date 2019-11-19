// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_VARIABLE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_VARIABLE_RESOLVER_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CSSCustomPropertyDeclaration;
class CSSParserTokenRange;
class CSSVariableData;
class CSSVariableReferenceValue;
class CSSParserContext;
class PropertyRegistration;
class PropertyRegistry;
class StyleInheritedVariables;
class StyleNonInheritedVariables;
class StyleResolverState;

namespace cssvalue {

class CSSPendingSubstitutionValue;

}

class CORE_EXPORT CSSVariableResolver {
  STACK_ALLOCATED();

 public:
  explicit CSSVariableResolver(const StyleResolverState&);

  scoped_refptr<CSSVariableData> ResolveCustomPropertyAnimationKeyframe(
      const CSSCustomPropertyDeclaration& keyframe,
      bool& cycle_detected);

  void ResolveVariableDefinitions();

  // Shorthand properties are not supported.
  const CSSValue* ResolveVariableReferences(CSSPropertyID,
                                            const CSSValue&,
                                            bool disallow_animation_tainted);

  void ComputeRegisteredVariables();

 protected:
  // Called before looking up the value of some var()-reference to make it
  // possible to apply animated properties during variable resolution.
  virtual void ApplyAnimation(const AtomicString& name) {}

 private:
  // The maximum number of tokens that may be produced by a var()
  // reference.
  //
  // https://drafts.csswg.org/css-variables/#long-variables
  static const size_t kMaxSubstitutionTokens = 16384;

  struct Options {
    STACK_ALLOCATED();

   public:
    // Treat any references to animation-tainted custom properties as invalid.
    //
    // Custom properties used in @keyframe rules become 'animation-tainted'
    // (see https://drafts.csswg.org/css-variables/#syntax). References to
    // animation-tainted custom properties are not allowed for properties
    // that affect animations.
    bool disallow_animation_tainted = false;

    // Treat any references to registered custom properties with font-relative
    // units as invalid.
    //
    // This is used when resolving variable references for 'font-size', where
    // registered custom properties with font-relative units may not be used.
    //
    // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles-via-relative-units
    bool disallow_registered_font_units = false;

    // Treat any references to registered custom properties with
    // root-font-relative units as invalid.
    //
    // This is used when resolving variable references for 'font-size' on the
    // root element, where registered custom properties with root-font-relative
    // units may not be used.
    //
    // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles-via-relative-units
    bool disallow_registered_root_font_units = false;

    // Absolutize CSSVariableData during variable resolution.
    //
    // Absolutization is a process where the substitution tokens of a registered
    // custom property are "synthetically" created to represent the computed
    // value of the custom property. For instance, a <length>-registered custom
    // property may be specified with the value "10em". However, this property
    // should substitute into others as the computed value, hence an equivalent
    // token stream is needed. Assuming a font-size of 12px (for instance),
    // the absolutization process would produce a token stream of "120px".
    //
    // Absolutization must take place after high-priority properties have been
    // applied, to be able to resolve the relative units correctly. However,
    // registered custom properties must also be usable for the high-priority
    // properties themselves (e.g. color). When a high-priority property refers
    // to a custom property with an (inner) var()-reference, that custom
    // property is resolved "on the fly" with absolutize=false. This means that
    // the equivalent token stream for the computed value of that custom
    // property is not stored on ComputedStyle. Storing the token stream on
    // ComputedStyle can only be done with absolutize=true, otherwise we can
    // permanently end up with the wrong token stream if an unregistered
    // property references a registered property, for instance.
    bool absolutize = false;
  };

  struct Result {
    STACK_ALLOCATED();

   public:
    Vector<CSSParserToken> tokens;
    Vector<String> backing_strings;
    bool is_animation_tainted = false;
    bool has_font_units = false;
    bool has_root_font_units = false;
    bool absolutized = false;
  };

  const CSSValue* ResolvePendingSubstitutions(
      CSSPropertyID,
      const cssvalue::CSSPendingSubstitutionValue&,
      const Options&);
  const CSSValue* ResolveVariableReferences(CSSPropertyID,
                                            const CSSVariableReferenceValue&,
                                            const Options&);

  // These return false if we encounter a reference to an invalid variable with
  // no fallback.

  // Resolves a range which may contain var() or env() references.
  bool ResolveTokenRange(CSSParserTokenRange, const Options&, Result&);

  // Return value for ResolveFallback.
  enum class Fallback {
    // Fallback not present.
    kNone,
    // Fallback present, but resolution failed (i.e. invalid variables
    // referenced), or the result did not match the syntax registered for
    // the referenced variable (if applicable).
    kFail,
    // Fallback present, resolution succeeded, and syntax matched (if
    // applicable).
    kSuccess
  };

  // Resolves the fallback (if present) of a var() or env() reference, starting
  // from the comma.
  Fallback ResolveFallback(CSSParserTokenRange,
                           const Options&,
                           const PropertyRegistration*,
                           Result&);
  // Resolves the contents of a var() or env() reference.
  bool ResolveVariableReference(CSSParserTokenRange,
                                const Options&,
                                bool is_env_variable,
                                Result&);

  // These return null if the custom property is invalid.

  // Returns the CSSVariableData for an environment variable.
  scoped_refptr<CSSVariableData> ValueForEnvironmentVariable(
      const AtomicString& name);
  // Returns the CSSVariableData for a custom property, resolving and storing it
  // if necessary. If a cycle via font-relative units was discovered, the
  // unit_cycle flag is set to true.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
  scoped_refptr<CSSVariableData> ValueForCustomProperty(AtomicString name,
                                                        const Options&,
                                                        bool& unit_cycle);
  // Resolves the CSSVariableData from a custom property declaration.
  scoped_refptr<CSSVariableData> ResolveCustomProperty(AtomicString name,
                                                       const CSSVariableData&,
                                                       const Options&,
                                                       bool& cycle_detected);
  // Like ResolveCustomProperty, but returns the incoming CSSVariableData if
  // no resolution is needed.
  scoped_refptr<CSSVariableData> ResolveCustomPropertyIfNeeded(
      AtomicString name,
      CSSVariableData*,
      const Options&,
      bool& cycle_detected);

  bool IsDisallowedByFontUnitFlags(const CSSVariableData&,
                                   const Options&,
                                   const PropertyRegistration*);

  bool IsDisallowedByAnimationTaintedFlag(const CSSVariableData&,
                                          const Options&);

  // The following utilities get/set variables on either StyleInheritedVariables
  // or StyleNonInheritedVariables, according to their PropertyRegistration.

  CSSVariableData* GetVariableData(const AtomicString& name,
                                   const PropertyRegistration*);
  const CSSValue* GetVariableValue(const AtomicString& name,
                                   const PropertyRegistration&);
  void SetVariableData(const AtomicString& name,
                       const PropertyRegistration*,
                       scoped_refptr<CSSVariableData>);
  void SetVariableValue(const AtomicString& name,
                        const PropertyRegistration&,
                        const CSSValue*);
  void SetInvalidVariable(const AtomicString& name,
                          const PropertyRegistration*);

  const CSSParserContext* GetParserContext(
      const CSSVariableReferenceValue&) const;

  const StyleResolverState& state_;
  StyleInheritedVariables* inherited_variables_;
  StyleNonInheritedVariables* non_inherited_variables_;
  Member<const PropertyRegistry> registry_;
  HashSet<AtomicString> variables_seen_;
  // Resolution doesn't finish when a cycle is detected. Fallbacks still
  // need to be tracked for additional cycles, and invalidation only
  // applies back to cycle starts.
  HashSet<AtomicString> cycle_start_points_;

  friend class CSSVariableResolverTest;
};

}  // namespace blink

#endif  // CSSVariableResolver
