// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include <vector>

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet_init.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_animator.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using css_test_helpers::RegisterProperty;
using cssvalue::CSSPendingInterpolationValue;
using Origin = StyleCascade::Origin;
using Priority = StyleCascade::Priority;
using UnitType = CSSPrimitiveValue::UnitType;

enum class AnimationTainted { kYes, kNo };

class TestCascade {
  STACK_ALLOCATED();

 public:
  TestCascade(Document& document, Element* target = nullptr)
      : state_(document, target ? *target : *document.body()),
        cascade_(InitState(state_)) {}

  scoped_refptr<ComputedStyle> TakeStyle() { return state_.TakeStyle(); }

  StyleResolverState& State() { return state_; }
  StyleCascade& InnerCascade() { return cascade_; }

  void InheritFrom(scoped_refptr<ComputedStyle> parent) {
    state_.SetParentStyle(parent);
    state_.StyleRef().InheritFrom(*parent);
  }

  void Add(String name,
           String value,
           Priority priority = Origin::kAuthor,
           AnimationTainted animation_tainted = AnimationTainted::kNo) {
    return Add(*CSSPropertyName::From(name), value, priority,
               animation_tainted);
  }

  void Add(const CSSPropertyName& name,
           String value,
           Priority priority = Origin::kAuthor,
           AnimationTainted animation_tainted = AnimationTainted::kNo) {
    HeapVector<CSSPropertyValue, 256> values =
        ParseValues(name, value, animation_tainted);

    for (const CSSPropertyValue& v : values)
      Add(v.Name(), v.Value(), priority);
  }

  void Add(String name,
           const CSSValue* value,
           Priority priority = Origin::kAuthor) {
    Add(*CSSPropertyName::From(name), value, priority);
  }

  void Add(const CSSPropertyName& name,
           const CSSValue* value,
           Priority priority = Origin::kAuthor) {
    DCHECK(CSSPropertyRef(name, GetDocument()).GetProperty().IsLonghand());
    cascade_.Add(name, value, priority);
  }

  void Apply(const CSSPropertyName& name) { cascade_.Apply(name); }
  void Apply(String name) { Apply(*CSSPropertyName::From(name)); }
  void Apply() { cascade_.Apply(); }
  void Apply(StyleCascade::Animator& animator) { cascade_.Apply(animator); }

  HeapVector<CSSPropertyValue, 256> ParseValues(
      const CSSPropertyName& name,
      String value,
      AnimationTainted animation_tainted) {
    CSSTokenizer tokenizer(value);
    auto tokens = tokenizer.TokenizeToEOF();
    auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    context->SetMode(kUASheetMode);  // Allows -internal variables.

    HeapVector<CSSPropertyValue, 256> parsed_properties;

    bool is_animation_tainted = animation_tainted == AnimationTainted::kYes;
    if (name.Id() == CSSPropertyID::kVariable) {
      // TODO(andruud): Make CSSPropertyParser::ParseValue handle custom props.
      const CSSValue* decl = CSSVariableParser::ParseDeclarationValue(
          name.ToAtomicString(), tokens, is_animation_tainted, *context);
      DCHECK(decl);
      parsed_properties.emplace_back(GetCSSPropertyVariable(), *decl);
      return parsed_properties;
    }

    const bool important = false;

    bool ok = CSSPropertyParser::ParseValue(name.Id(), important, tokens,
                                            context, parsed_properties,
                                            StyleRule::RuleType::kStyle);
    DCHECK(ok);

    return parsed_properties;
  }

  String ComputedValue(String name) const {
    CSSPropertyRef ref(name, GetDocument());
    DCHECK(ref.IsValid());
    const LayoutObject* layout_object = nullptr;
    bool allow_visited_style = false;
    const CSSValue* value = ref.GetProperty().CSSValueFromComputedStyle(
        *state_.Style(), layout_object, allow_visited_style);
    return value ? value->CssText() : g_null_atom;
  }

  bool HasValue(String name, const CSSValue* value) {
    return cascade_.HasValue(*CSSPropertyName::From(name), value);
  }

  const CSSValue* GetCSSValue(String name) {
    return cascade_.GetValue(*CSSPropertyName::From(name));
  }

  const String GetValue(String name) {
    const CSSValue* value = GetCSSValue(name);
    // Per spec, CSSPendingSubstitutionValue serializes as an empty string,
    // but for testing purposes it's nice to see the actual value.
    if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value))
      return v->ShorthandValue()->CssText();
    if (DynamicTo<CSSPendingInterpolationValue>(value))
      return "<interpolation>";
    return value ? value->CssText() : g_null_atom;
  }

  CSSAnimationUpdate& CalculateTransitionUpdate() {
    CSSAnimations::CalculateTransitionUpdate(
        state_.AnimationUpdate(), CSSAnimations::PropertyPass::kCustom,
        &state_.GetElement(), *state_.Style());
    CSSAnimations::CalculateTransitionUpdate(
        state_.AnimationUpdate(), CSSAnimations::PropertyPass::kStandard,
        &state_.GetElement(), *state_.Style());
    return state_.AnimationUpdate();
  }

  CSSAnimationUpdate& CalculateAnimationUpdate() {
    CSSAnimations::CalculateAnimationUpdate(
        state_.AnimationUpdate(), &state_.GetElement(), state_.GetElement(),
        *state_.Style(), state_.ParentStyle(),
        &GetDocument().EnsureStyleResolver());
    return state_.AnimationUpdate();
  }

  void AddAnimations() {
    auto& update = CalculateAnimationUpdate();
    using Type = CSSPendingInterpolationValue::Type;

    for (const auto& entry : update.ActiveInterpolationsForCustomAnimations()) {
      auto name = entry.key.GetCSSPropertyName();
      auto* v = CSSPendingInterpolationValue::Create(Type::kCSSProperty);
      Add(name.ToAtomicString(), v);
    }
    for (const auto& entry :
         update.ActiveInterpolationsForStandardAnimations()) {
      auto name = entry.key.GetCSSPropertyName();
      auto* v = CSSPendingInterpolationValue::Create(Type::kCSSProperty);
      Add(name.ToAtomicString(), v);
    }
  }

  void AddTransitions() {
    auto& update = CalculateTransitionUpdate();
    using Type = CSSPendingInterpolationValue::Type;

    for (const auto& entry :
         update.ActiveInterpolationsForCustomTransitions()) {
      auto name = entry.key.GetCSSPropertyName();
      auto* v = CSSPendingInterpolationValue::Create(Type::kCSSProperty);
      Add(name.ToAtomicString(), v);
    }
    for (const auto& entry :
         update.ActiveInterpolationsForStandardTransitions()) {
      auto name = entry.key.GetCSSPropertyName();
      auto* v = CSSPendingInterpolationValue::Create(Type::kCSSProperty);
      Add(name.ToAtomicString(), v);
    }
  }

 private:
  Document& GetDocument() const { return state_.GetDocument(); }
  Element* Body() const { return GetDocument().body(); }

  static StyleResolverState& InitState(StyleResolverState& state) {
    state.SetStyle(InitialStyle(state.GetDocument()));
    state.SetParentStyle(InitialStyle(state.GetDocument()));
    return state;
  }

  static scoped_refptr<ComputedStyle> InitialStyle(Document& document) {
    return StyleResolver::InitialStyleForElement(document);
  }

  StyleResolverState state_;
  StyleCascade cascade_;
};

class TestCascadeResolver {
  STACK_ALLOCATED();

 public:
  TestCascadeResolver(Document& document, StyleAnimator& animator)
      : document_(&document), resolver_(animator) {}
  bool InCycle() const { return resolver_.InCycle(); }
  bool DetectCycle(String name) {
    CSSPropertyRef ref(name, *document_);
    DCHECK(ref.IsValid());
    const CSSProperty& property = ref.GetProperty();
    return resolver_.DetectCycle(property);
  }
  wtf_size_t CycleDepth() const { return resolver_.cycle_depth_; }

 private:
  friend class TestCascadeAutoLock;

  Member<Document> document_;
  StyleCascade::Resolver resolver_;
};

class TestCascadeAutoLock {
  STACK_ALLOCATED();

 public:
  TestCascadeAutoLock(const CSSPropertyName& name,
                      TestCascadeResolver& resolver)
      : lock_(name, resolver.resolver_) {}

 private:
  StyleCascade::AutoLock lock_;
};

class StyleCascadeTest : public PageTestBase, private ScopedCSSCascadeForTest {
 public:
  StyleCascadeTest() : ScopedCSSCascadeForTest(true) {}

  CSSStyleSheet* CreateSheet(const String& css_text) {
    auto* init = MakeGarbageCollected<CSSStyleSheetInit>();
    DummyExceptionStateForTesting exception_state;
    CSSStyleSheet* sheet =
        CSSStyleSheet::Create(GetDocument(), init, exception_state);
    sheet->replaceSync(css_text, exception_state);
    sheet->Contents()->EnsureRuleSet(MediaQueryEvaluator(),
                                     kRuleHasNoSpecialState);
    return sheet;
  }

  void AppendSheet(const String& css_text) {
    CSSStyleSheet* sheet = CreateSheet(css_text);
    ASSERT_TRUE(sheet);

    Element* body = GetDocument().body();
    ASSERT_TRUE(body->IsInTreeScope());
    TreeScope& tree_scope = body->GetTreeScope();
    ScopedStyleResolver& scoped_resolver =
        tree_scope.EnsureScopedStyleResolver();
    ActiveStyleSheetVector active_sheets;
    active_sheets.push_back(
        std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));
    scoped_resolver.AppendActiveStyleSheets(0, active_sheets);
  }

  Element* DocumentElement() const { return GetDocument().documentElement(); }

  void SetRootFont(String value) {
    DocumentElement()->SetInlineStyleProperty(CSSPropertyID::kFontSize, value);
    UpdateAllLifecyclePhasesForTest();
  }

  Priority AuthorPriority(uint16_t tree_order, uint16_t cascade_order) {
    return Priority(Origin::kAuthor, tree_order)
        .WithCascadeOrder(cascade_order);
  }

  Priority ImportantAuthorPriority(uint16_t tree_order,
                                   uint16_t cascade_order) {
    return Priority(Origin::kImportantAuthor, tree_order)
        .WithCascadeOrder(cascade_order);
  }

  // Temporarily create a CSS Environment Variable.
  // https://drafts.csswg.org/css-env-1/
  class AutoEnv {
    STACK_ALLOCATED();

   public:
    AutoEnv(PageTestBase& test, AtomicString name, String value)
        : document_(&test.GetDocument()), name_(name) {
      EnsureEnvironmentVariables().SetVariable(name, value);
    }
    ~AutoEnv() { EnsureEnvironmentVariables().RemoveVariable(name_); }

   private:
    DocumentStyleEnvironmentVariables& EnsureEnvironmentVariables() {
      return document_->GetStyleEngine().EnsureEnvironmentVariables();
    }

    Member<Document> document_;
    AtomicString name_;
  };
};

TEST_F(StyleCascadeTest, OriginImportance) {
  EXPECT_EQ(Origin::kImportantUserAgent,
            Priority(Origin::kUserAgent).AddImportance().GetOrigin());
  EXPECT_EQ(Origin::kImportantUser,
            Priority(Origin::kUser).AddImportance().GetOrigin());
  EXPECT_EQ(Origin::kImportantAuthor,
            Priority(Origin::kAuthor).AddImportance().GetOrigin());
}

TEST_F(StyleCascadeTest, PriorityOrigin) {
  std::vector<Priority> priorities = {
      Origin::kTransition,    Origin::kImportantUserAgent,
      Origin::kImportantUser, Origin::kImportantAuthor,
      Origin::kAnimation,     Origin::kAuthor,
      Origin::kUser,          Origin::kUserAgent,
      Origin::kNone};

  for (size_t i = 0; i < priorities.size(); ++i) {
    for (size_t j = i; j < priorities.size(); ++j)
      EXPECT_GE(priorities[i], priorities[j]);
  }

  EXPECT_FALSE(Priority(Origin::kUser) >= Priority(Origin::kAuthor));
}

TEST_F(StyleCascadeTest, PriorityHasOrigin) {
  EXPECT_TRUE(Priority(Origin::kTransition).HasOrigin());
  EXPECT_TRUE(Priority(Origin::kAuthor).HasOrigin());
  EXPECT_FALSE(Priority(Origin::kNone).HasOrigin());
}

TEST_F(StyleCascadeTest, PriorityTreeOrder) {
  Origin origin = Origin::kAuthor;
  EXPECT_GE(Priority(origin, 0), Priority(origin, 1));
  EXPECT_GE(Priority(origin, 6), Priority(origin, 7));
  EXPECT_GE(Priority(origin, 42), Priority(origin, 42));
  EXPECT_FALSE(Priority(origin, 8) >= Priority(origin, 1));
}

TEST_F(StyleCascadeTest, PriorityTreeOrderImportant) {
  Origin origin = Origin::kImportantAuthor;
  EXPECT_GE(Priority(origin, 1), Priority(origin, 0));
  EXPECT_GE(Priority(origin, 7), Priority(origin, 6));
  EXPECT_GE(Priority(origin, 42), Priority(origin, 42));
  EXPECT_FALSE(Priority(origin, 1) >= Priority(origin, 8));
}

TEST_F(StyleCascadeTest, PriorityTreeOrderDifferentOrigin) {
  // Tree order does not matter if the origin is different.
  Origin author = Origin::kAuthor;
  Origin transition = Origin::kTransition;
  EXPECT_GE(Priority(transition, 42), Priority(author, 1));
  EXPECT_GE(Priority(transition, 1), Priority(author, 1));
}

TEST_F(StyleCascadeTest, PriorityCascadeOrder) {
  // AuthorPriority(tree_order, cascade_order)
  EXPECT_GE(AuthorPriority(0, 0), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(0, 2), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(0, 0xFFFF), AuthorPriority(0, 0xFFFE));
  EXPECT_FALSE(AuthorPriority(0, 2) >= AuthorPriority(0, 3));
}

TEST_F(StyleCascadeTest, PriorityCascadeOrderAndTreeOrder) {
  // AuthorPriority(tree_order, cascade_order)
  EXPECT_GE(AuthorPriority(0, 0), AuthorPriority(1, 0));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(1, 1));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(1, 3));
  EXPECT_GE(AuthorPriority(0, 2), AuthorPriority(1, 0xFFFE));
}

TEST_F(StyleCascadeTest, PriorityCascadeOrderAndOrigin) {
  // [Important]AuthorPriority(tree_order, cascade_order)
  EXPECT_GE(ImportantAuthorPriority(0, 0), AuthorPriority(0, 0));
  EXPECT_GE(ImportantAuthorPriority(0, 1), AuthorPriority(0, 1));
  EXPECT_GE(ImportantAuthorPriority(0, 1), AuthorPriority(0, 3));
  EXPECT_GE(ImportantAuthorPriority(0, 2), AuthorPriority(0, 0xFFFE));
}

TEST_F(StyleCascadeTest, ApplySingle) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "2px", Origin::kAuthor);
  cascade.Add("width", "1px", Origin::kUser);
  cascade.Apply("width");

  EXPECT_EQ("2px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", " 10px ");
  cascade.Add("--y", "nope");
  cascade.Apply("--x");
  cascade.Apply("--y");

  EXPECT_EQ(" 10px ", cascade.ComputedValue("--x"));
  EXPECT_EQ("nope", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, Copy) {
  StyleResolverState state(GetDocument(), *GetDocument().body());

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Add("width", "20px");

  // Take snapshot of the cascade, pointing to the same StyleResolverState.
  StyleCascade snapshot(cascade.InnerCascade());

  cascade.Add("--x", "0px");
  cascade.Add("width", "1px");
  cascade.Apply();

  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
  EXPECT_EQ("1px", cascade.ComputedValue("width"));

  snapshot.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("20px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyCustomPropertyVar) {
  // Apply --x first.
  {
    TestCascade cascade(GetDocument());
    cascade.Add("--x", "yes and var(--y)");
    cascade.Add("--y", "no");
    cascade.Apply("--x");
    cascade.Apply("--y");

    EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
    EXPECT_EQ("no", cascade.ComputedValue("--y"));
  }

  // Apply --y first.
  {
    TestCascade cascade(GetDocument());
    cascade.Add("--x", "yes and var(--y)");
    cascade.Add("--y", "no");
    cascade.Apply("--y");
    cascade.Apply("--x");

    EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
    EXPECT_EQ("no", cascade.ComputedValue("--y"));
  }
}

TEST_F(StyleCascadeTest, InvalidVarReferenceCauseInvalidVariable) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "nope var(--y)");
  cascade.Apply("--x");

  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, ApplyCustomPropertyFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "yes and var(--y,no)");
  cascade.Apply("--x");

  EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyFallback) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "var(--y,10px)");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyFallbackValidation) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Add("--y", "var(--x,red)");  // Fallback must be valid <length>.
  cascade.Add("--z", "var(--y,pass)");
  cascade.Apply();

  EXPECT_EQ("pass", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, VarInFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "one var(--z,two var(--y))");
  cascade.Add("--y", "three");
  cascade.Apply("--x");

  EXPECT_EQ("one two three", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, VarReferenceInNormalProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Add("width", "var(--x)");
  cascade.Apply("width");

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, MultipleVarRefs) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "var(--y) bar var(--y)");
  cascade.Add("--y", "foo");
  cascade.Apply("--x");

  EXPECT_EQ("foo bar foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyComputedValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Apply("--x");

  EXPECT_EQ("96px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertySyntaxErrorCausesInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "10px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "#fefefe");
  cascade.Add("--y", "var(--x)");
  cascade.Apply("--x");
  cascade.Apply("--y");

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("10px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredPropertySubstitution) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Add("--y", "var(--x)");
  cascade.Apply("--y");

  EXPECT_EQ("96px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyChain) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--z", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Add("--y", "var(--x)");
  cascade.Add("--z", "calc(var(--y) + 1in)");
  cascade.Apply();

  EXPECT_EQ("96px", cascade.ComputedValue("--x"));
  EXPECT_EQ("96px", cascade.ComputedValue("--y"));
  EXPECT_EQ("192px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, BasicShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px 2px 3px 4px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("2px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, BasicVarShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("2px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ApplyingPendingSubstitutionFirst) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Add("margin-right", "5px");

  // Apply one of the pending substitution values first. This should not
  // overwrite margin-right's 5px.
  cascade.Apply("margin-left");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("5px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ApplyingPendingSubstitutionLast) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Add("margin-right", "5px");

  // Apply margin-right before the others. Applying the pending substitution
  // afterwards should not overwrite margin-right's 5px.
  cascade.Apply("margin-right");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("5px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ApplyingPendingSubstitutionModifiesCascade) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Add("margin-right", "5px");

  // We expect the pending substitution value for all the shorthands,
  // except margin-right.
  EXPECT_EQ("1px var(--x) 3px 4px", cascade.GetValue("margin-top"));
  EXPECT_EQ("5px", cascade.GetValue("margin-right"));
  EXPECT_EQ("1px var(--x) 3px 4px", cascade.GetValue("margin-bottom"));
  EXPECT_EQ("1px var(--x) 3px 4px", cascade.GetValue("margin-left"));

  // Apply a pending substitution value should modify the cascade for other
  // longhands with the same pending substitution value.
  cascade.Apply("margin-left");

  EXPECT_EQ("1px", cascade.GetValue("margin-top"));
  EXPECT_EQ("5px", cascade.GetValue("margin-right"));
  EXPECT_EQ("3px", cascade.GetValue("margin-bottom"));
  EXPECT_FALSE(cascade.GetValue("margin-left"));
}

TEST_F(StyleCascadeTest, ResolverDetectCycle) {
  TestCascade cascade(GetDocument());
  StyleAnimator animator(cascade.State(), cascade.InnerCascade());
  TestCascadeResolver resolver(GetDocument(), animator);

  {
    TestCascadeAutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      TestCascadeAutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        TestCascadeAutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());

        EXPECT_TRUE(resolver.DetectCycle("--a"));
        EXPECT_TRUE(resolver.InCycle());
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_TRUE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectNoCycle) {
  TestCascade cascade(GetDocument());
  StyleAnimator animator(cascade.State(), cascade.InnerCascade());
  TestCascadeResolver resolver(GetDocument(), animator);

  {
    TestCascadeAutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      TestCascadeAutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        TestCascadeAutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());

        EXPECT_FALSE(resolver.DetectCycle("--x"));
        EXPECT_FALSE(resolver.InCycle());
      }
      EXPECT_FALSE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectCycleSelf) {
  TestCascade cascade(GetDocument());
  StyleAnimator animator(cascade.State(), cascade.InnerCascade());
  TestCascadeResolver resolver(GetDocument(), animator);

  {
    TestCascadeAutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());

    EXPECT_TRUE(resolver.DetectCycle("--a"));
    EXPECT_TRUE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectMultiCycle) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  StyleAnimator animator(cascade.State(), cascade.InnerCascade());
  TestCascadeResolver resolver(GetDocument(), animator);

  {
    AutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        AutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());
        {
          AutoLock lock(CSSPropertyName("--d"), resolver);
          EXPECT_FALSE(resolver.InCycle());

          // Cycle 1 (big cycle):
          EXPECT_TRUE(resolver.DetectCycle("--b"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleDepth());

          // Cycle 2 (small cycle):
          EXPECT_TRUE(resolver.DetectCycle("--c"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleDepth());
        }
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectMultiCycleReverse) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  StyleAnimator animator(cascade.State(), cascade.InnerCascade());
  TestCascadeResolver resolver(GetDocument(), animator);

  {
    AutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        AutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());
        {
          AutoLock lock(CSSPropertyName("--d"), resolver);
          EXPECT_FALSE(resolver.InCycle());

          // Cycle 1 (small cycle):
          EXPECT_TRUE(resolver.DetectCycle("--c"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(2u, resolver.CycleDepth());

          // Cycle 2 (big cycle):
          EXPECT_TRUE(resolver.DetectCycle("--b"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleDepth());
        }
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, BasicCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "foo");
  cascade.Add("--b", "bar");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--a"));
  EXPECT_EQ("bar", cascade.ComputedValue("--b"));

  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, SelfCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "foo");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--a"));

  cascade.Add("--a", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
}

TEST_F(StyleCascadeTest, SelfCycleInFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--x, var(--a))");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
}

TEST_F(StyleCascadeTest, SelfCycleInUnusedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b, var(--a))");
  cascade.Add("--b", "10px");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_EQ("10px", cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, LongCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--c)");
  cascade.Add("--c", "var(--d)");
  cascade.Add("--d", "var(--e)");
  cascade.Add("--e", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_FALSE(cascade.ComputedValue("--d"));
  EXPECT_FALSE(cascade.ComputedValue("--e"));
}

TEST_F(StyleCascadeTest, PartialCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("--c", "bar var(--d) var(--a)");
  cascade.Add("--d", "foo");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_EQ("foo", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, VarCycleViaFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--x, var(--a))");
  cascade.Add("--c", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
}

TEST_F(StyleCascadeTest, FallbackTriggeredByCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("--c", "var(--a,foo)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("foo", cascade.ComputedValue("--c"));
}

TEST_F(StyleCascadeTest, RegisteredCycle) {
  RegisterProperty(GetDocument(), "--a", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--b", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, PartiallyRegisteredCycle) {
  RegisterProperty(GetDocument(), "--a", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, FallbackTriggeredByRegisteredCycle) {
  RegisterProperty(GetDocument(), "--a", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--b", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  // References to cycle:
  cascade.Add("--c", "var(--a,1px)");
  cascade.Add("--d", "var(--b,2px)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("1px", cascade.ComputedValue("--c"));
  EXPECT_EQ("2px", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, CycleStillInvalidWithFallback) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b,red)");
  cascade.Add("--b", "var(--a,red)");
  // References to cycle:
  cascade.Add("--c", "var(--a,green)");
  cascade.Add("--d", "var(--b,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("green", cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, CycleInFallbackStillInvalid) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b,red)");
  cascade.Add("--b", "var(--x,var(--a))");
  // References to cycle:
  cascade.Add("--c", "var(--a,green)");
  cascade.Add("--d", "var(--b,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("green", cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, CycleMultiple) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--c, red)");
  cascade.Add("--b", "var(--c, red)");
  cascade.Add("--c", "var(--a, blue) var(--b, blue)");
  // References to cycle:
  cascade.Add("--d", "var(--a,green)");
  cascade.Add("--e", "var(--b,green)");
  cascade.Add("--f", "var(--c,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
  EXPECT_EQ("green", cascade.ComputedValue("--e"));
  EXPECT_EQ("green", cascade.ComputedValue("--f"));
}

TEST_F(StyleCascadeTest, CycleMultipleFallback) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b, red)");
  cascade.Add("--b", "var(--a, var(--c, red))");
  cascade.Add("--c", "var(--b, red)");
  // References to cycle:
  cascade.Add("--d", "var(--a,green)");
  cascade.Add("--e", "var(--b,green)");
  cascade.Add("--f", "var(--c,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
  EXPECT_EQ("green", cascade.ComputedValue("--e"));
  EXPECT_EQ("green", cascade.ComputedValue("--f"));
}

TEST_F(StyleCascadeTest, CycleMultipleUnusedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "red");
  // Cycle:
  cascade.Add("--b", "var(--c, red)");
  cascade.Add("--c", "var(--a, var(--b, red) var(--d, red))");
  cascade.Add("--d", "var(--c, red)");
  // References to cycle:
  cascade.Add("--e", "var(--b,green)");
  cascade.Add("--f", "var(--c,green)");
  cascade.Add("--g", "var(--d,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_FALSE(cascade.ComputedValue("--d"));
  EXPECT_EQ("green", cascade.ComputedValue("--e"));
  EXPECT_EQ("green", cascade.ComputedValue("--f"));
  EXPECT_EQ("green", cascade.ComputedValue("--g"));
}

TEST_F(StyleCascadeTest, CycleReferencedFromStandardProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("color", "var(--a,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
}

TEST_F(StyleCascadeTest, CycleReferencedFromShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("background", "var(--a,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, EmUnit) {
  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "10px");
  cascade.Add("width", "10em");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, EmUnitCustomProperty) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "10px");
  cascade.Add("--x", "10em");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, EmUnitNonCycle) {
  TestCascade parent(GetDocument());
  parent.Add("font-size", "10px");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Apply();

  // Note: Only registered properties can have cycles with font-size.
  EXPECT_EQ("100px", cascade.ComputedValue("font-size"));
}

TEST_F(StyleCascadeTest, EmUnitCycle) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, SubstitutingEmCycles) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Add("--y", "var(--x)");
  cascade.Add("--z", "var(--x,1px)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--y"));
  EXPECT_EQ("1px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, RemUnit) {
  SetRootFont("10px");
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade(GetDocument());
  cascade.Add("width", "10rem");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, RemUnitCustomProperty) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  SetRootFont("10px");
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10rem");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInFontSize) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  SetRootFont("10px");
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "1rem");
  cascade.Add("--x", "10rem");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInRootFontSizeCycle) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument(), DocumentElement());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "1rem");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInRootFontSizeNonCycle) {
  TestCascade cascade(GetDocument(), DocumentElement());
  cascade.Add("font-size", "initial");
  cascade.Apply();

  String expected = cascade.ComputedValue("font-size");

  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "1rem");
  cascade.Apply();

  // Note: Only registered properties can have cycles with font-size.
  EXPECT_EQ("1rem", cascade.ComputedValue("--x"));
  EXPECT_EQ(expected, cascade.ComputedValue("font-size"));
}

TEST_F(StyleCascadeTest, Initial) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Add("--y", "foo");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
  EXPECT_EQ("foo", cascade.ComputedValue("--y"));

  cascade.Add("--x", "initial");
  cascade.Add("--y", "initial");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--x"));
  EXPECT_FALSE(cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, Inherit) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());

  EXPECT_EQ("foo", cascade.ComputedValue("--x"));

  cascade.Add("--x", "bar");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));

  cascade.Add("--x", "inherit");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, Unset) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));

  cascade.Add("--x", "bar");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));

  cascade.Add("--x", "unset");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredImplicitInitialValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "13px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--y", " var(--x) ");
  cascade.Apply();
  EXPECT_EQ("13px", cascade.ComputedValue("--x"));
  EXPECT_EQ(" 13px ", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredUniversal) {
  RegisterProperty(GetDocument(), "--x", "*", "foo", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "bar");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));
  EXPECT_EQ("bar", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredUniversalInvalid) {
  RegisterProperty(GetDocument(), "--x", "*", g_null_atom, false);

  TestCascade cascade(GetDocument());
  cascade.Add("--y", " var(--x) ");
  cascade.Apply();
  EXPECT_FALSE(cascade.ComputedValue("--x"));
  EXPECT_FALSE(cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredUniversalInitial) {
  RegisterProperty(GetDocument(), "--x", "*", "foo", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--y", " var(--x) ");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
  EXPECT_EQ(" foo ", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));

  cascade.Add("--x", "initial");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
  EXPECT_EQ("0px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitInherit) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade parent(GetDocument());
  parent.Add("--x", "15px");
  parent.Apply();
  EXPECT_EQ("15px", parent.ComputedValue("--x"));

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));  // Note: inherit==false

  cascade.Add("--x", "inherit");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitUnset) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "0px", true);

  TestCascade parent(GetDocument());
  parent.Add("--x", "15px");
  parent.Add("--y", "15px");
  parent.Apply();
  EXPECT_EQ("15px", parent.ComputedValue("--x"));
  EXPECT_EQ("15px", parent.ComputedValue("--y"));

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Add("--x", "2px");
  cascade.Add("--y", "2px");
  cascade.Apply();
  EXPECT_EQ("2px", cascade.ComputedValue("--x"));
  EXPECT_EQ("2px", cascade.ComputedValue("--y"));

  cascade.Add("--x", "unset");
  cascade.Add("--y", "unset");
  cascade.Add("--z", "var(--x) var(--y)");
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
  EXPECT_EQ("0px 15px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "15px", Origin::kAuthor, AnimationTainted::kYes);
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInStandardProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "15px", Origin::kAuthor, AnimationTainted::kYes);
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInAnimationProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "20s");
  cascade.Add("animation-duration", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("20s", cascade.ComputedValue("animation-duration"));

  cascade.Add("--y", "20s", Origin::kAuthor, AnimationTainted::kYes);
  cascade.Add("animation-duration", "var(--y)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--y"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, IndirectlyAnimationTainted) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "20s", Origin::kAuthor, AnimationTainted::kYes);
  cascade.Add("--y", "var(--x)");
  cascade.Add("animation-duration", "var(--y)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("20s", cascade.ComputedValue("--y"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, AnimationTaintedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "20s", Origin::kAuthor, AnimationTainted::kYes);
  cascade.Add("animation-duration", "var(--x,1s)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("1s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, EnvMissingNestedVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "rgb(0, 0, 0)");
  cascade.Add("background-color", "env(missing, var(--x))");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("--x"));
  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, EnvMissingNestedVarFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(missing, var(--missing, blue))");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 255)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, EnvMissingFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(missing, blue)");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 255)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnv) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(test, blue)");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnvFallback) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(test, blue)");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnvInUnusedFallback) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "rgb(0, 0, 0)");
  cascade.Add("background-color", "var(--x, env(test))");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("--x"));
  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnvInUsedFallback) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "var(--missing, env(test))");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

// An Animator that just records the name of all the properties
// applied.
class RecordingAnimator : public StyleCascade::Animator {
 public:
  void Apply(const CSSProperty& property,
             const CSSPendingInterpolationValue&,
             StyleCascade::Resolver& resolver) override {
    record.push_back(property.GetCSSPropertyName());
  }

  Vector<CSSPropertyName> record;
};

TEST_F(StyleCascadeTest, AnimatorCalledByPendingInterpolationValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  RecordingAnimator animator;

  using Type = CSSPendingInterpolationValue::Type;
  TestCascade cascade(GetDocument());
  cascade.Add("--x", CSSPendingInterpolationValue::Create(Type::kCSSProperty));
  cascade.Add("--y", CSSPendingInterpolationValue::Create(Type::kCSSProperty));

  cascade.Apply(animator);

  EXPECT_TRUE(animator.record.Contains(*CSSPropertyName::From("--x")));
  EXPECT_TRUE(animator.record.Contains(*CSSPropertyName::From("--y")));
}

TEST_F(StyleCascadeTest, PendingKeyframeAnimation) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 10px; }
        to { --x: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "1s");
  cascade.Apply();

  cascade.AddAnimations();

  EXPECT_EQ("<interpolation>", cascade.GetValue("--x"));
}

TEST_F(StyleCascadeTest, PendingKeyframeAnimationApply) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 10px; }
        to { --x: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.AddAnimations();

  EXPECT_EQ("<interpolation>", cascade.GetValue("--x"));
  StyleAnimator animator(cascade.State(), cascade.InnerCascade());
  cascade.Apply(animator);
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, TransitionCausesInterpolationValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  // First, simulate an "old style".
  TestCascade cascade1(GetDocument());
  cascade1.Add("--x", "10px");
  cascade1.Add("transition", "--x 1s");
  cascade1.Apply();

  // Set the old style on the element, so that the animation
  // update detects it.
  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with a new value for --x.
  TestCascade cascade2(GetDocument());
  cascade2.Add("--x", "20px");
  cascade2.Add("transition", "--x 1s");
  cascade2.Apply();

  // Detects transitions, and adds CSSPendingInterpolationValues
  // to the cascade, as appropriate.
  cascade2.AddTransitions();

  EXPECT_EQ("<interpolation>", cascade2.GetValue("--x"));
}

TEST_F(StyleCascadeTest, TransitionDetectedForChangedFontSize) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade1(GetDocument());
  cascade1.Add("font-size", "10px");
  cascade1.Add("--x", "10em");
  cascade1.Add("width", "10em");
  cascade1.Add("height", "10px");
  cascade1.Add("transition", "--x 1s, width 1s");
  cascade1.Apply();

  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  TestCascade cascade2(GetDocument());
  cascade2.Add("font-size", "20px");
  cascade2.Add("--x", "10em");
  cascade2.Add("width", "10em");
  cascade2.Add("height", "10px");
  cascade2.Add("transition", "--x 1s, width 1s");
  cascade2.Apply();

  cascade2.AddTransitions();

  EXPECT_EQ("<interpolation>", cascade2.GetValue("--x"));
  EXPECT_EQ("<interpolation>", cascade2.GetValue("width"));
  EXPECT_EQ("10px", cascade2.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, AnimatingVarReferences) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: var(--from); }
        to { --x: var(--to); }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  StyleAnimator animator(cascade.State(), cascade.InnerCascade());

  cascade.AddAnimations();
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  cascade.Add("--y", "var(--x)");
  cascade.Apply(animator);

  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, AnimateStandardProperty) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { width: 10px; }
        to { width: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  StyleAnimator animator(cascade.State(), cascade.InnerCascade());

  cascade.AddAnimations();
  EXPECT_EQ("<interpolation>", cascade.GetValue("width"));

  cascade.Apply(animator);
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, EmRespondsToAnimatedFontSize) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { font-size: 10px; }
        to { font-size: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  StyleAnimator animator(cascade.State(), cascade.InnerCascade());

  cascade.AddAnimations();
  cascade.Add("--x", "2em");
  cascade.Add("width", "10em");

  cascade.Apply(animator);
  EXPECT_EQ("30px", cascade.ComputedValue("--x"));
  EXPECT_EQ("150px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, AnimateStandardPropertyWithVar) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { width: var(--from); }
        to { width: var(--to); }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  StyleAnimator animator(cascade.State(), cascade.InnerCascade());

  cascade.AddAnimations();
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");

  cascade.Apply(animator);
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, AnimateStandardShorthand) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { margin: 10px; }
        to { margin: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  StyleAnimator animator(cascade.State(), cascade.InnerCascade());

  cascade.AddAnimations();
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-top"));
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-right"));
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-bottom"));
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-left"));

  cascade.Apply(animator);
  EXPECT_EQ("15px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, AnimatePendingSubstitutionValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { margin: var(--from); }
        to { margin: var(--to); }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  StyleAnimator animator(cascade.State(), cascade.InnerCascade());

  cascade.AddAnimations();
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-top"));
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-right"));
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-bottom"));
  EXPECT_EQ("<interpolation>", cascade.GetValue("margin-left"));

  cascade.Apply(animator);
  EXPECT_EQ("15px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ForeignObjectZoomVsEffectiveZoom) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <svg>
      <foreignObject id='foreign'></foreignObject>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* foreign_object = GetDocument().getElementById("foreign");
  ASSERT_TRUE(foreign_object);

  TestCascade cascade(GetDocument(), foreign_object);

  cascade.Add("zoom", "200%");
  // TODO(andruud): Can't use CSSPropertyName to get -internal properties
  // yet.
  cascade.Add(CSSPropertyName(CSSPropertyID::kInternalEffectiveZoom),
              "initial");
  cascade.Apply();

  // If both zoom and -internal-zoom exists in the cascade,
  // -internal-effective-zoom should win.
  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomCascadeOrder) {
  CSSPropertyName effective_zoom(CSSPropertyID::kInternalEffectiveZoom);
  TestCascade cascade(GetDocument());
  cascade.Add("zoom", "200%");
  cascade.Add(effective_zoom, "initial");
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomReversedCascadeOrder) {
  CSSPropertyName effective_zoom(CSSPropertyID::kInternalEffectiveZoom);
  TestCascade cascade(GetDocument());
  cascade.Add(effective_zoom, "initial");
  cascade.Add("zoom", "200%");
  cascade.Apply();

  EXPECT_EQ(2.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomPriority) {
  CSSPropertyName effective_zoom(CSSPropertyID::kInternalEffectiveZoom);
  TestCascade cascade(GetDocument());
  cascade.Add("zoom", "200%", Origin::kImportantAuthor);
  cascade.Add(effective_zoom, "initial");
  cascade.Apply();

  EXPECT_EQ(2.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, WritingModeCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("writing-mode", "vertical-lr");
  cascade.Add("-webkit-writing-mode", "vertical-rl");
  cascade.Apply();

  EXPECT_EQ("vertical-rl", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-rl", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, WritingModeReversedCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-writing-mode", "vertical-rl");
  cascade.Add("writing-mode", "vertical-lr");
  cascade.Apply();

  EXPECT_EQ("vertical-lr", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-lr", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, WritingModePriority) {
  TestCascade cascade(GetDocument());
  cascade.Add("writing-mode", "vertical-lr", Origin::kImportantAuthor);
  cascade.Add("-webkit-writing-mode", "vertical-rl", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("vertical-lr", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-lr", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, MarkReferenced) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("width", "var(--x)");
  cascade.Apply();

  const auto* registry = GetDocument().GetPropertyRegistry();
  ASSERT_TRUE(registry);

  EXPECT_TRUE(registry->WasReferenced("--x"));
  EXPECT_FALSE(registry->WasReferenced("--y"));
}

TEST_F(StyleCascadeTest, InternalVisitedColorLonghand) {
  CSSPropertyName visited_color(CSSPropertyID::kInternalVisitedColor);

  TestCascade cascade(GetDocument());
  cascade.Add(visited_color, "red");
  cascade.Add("color", "green");
  cascade.Apply();

  cascade.State().Style()->SetInsideLink(EInsideLink::kInsideVisitedLink);

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));

  Color red(255, 0, 0);
  const CSSProperty& color = GetCSSPropertyColor();
  EXPECT_EQ(red, cascade.TakeStyle()->VisitedDependentColor(color));
}

TEST_F(StyleCascadeTest, VarInInternalVisitedShorthand) {
  CSSPropertyName visited_outline_color(
      CSSPropertyID::kInternalVisitedOutlineColor);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "green");
  cascade.Add("outline", "medium solid var(--x)");

  // Copy pending substitution value from outline-color to
  // -internal-visited-outline-color, approximating StyleResolver's behavior
  // for :visited declarations.
  const CSSValue* pending_substitution = cascade.GetCSSValue("outline-color");
  ASSERT_TRUE(pending_substitution);
  cascade.Add(visited_outline_color, pending_substitution);
  cascade.Add("outline-color", "red");

  // Apply "outline-color" manually first, to ensure that
  // -internal-visited-outline-color is applied afterwards.
  cascade.Apply("outline-color");

  // When applying -internal-visited-outline-color, it should not modify
  // outline-color.
  cascade.Apply();

  cascade.State().Style()->SetInsideLink(EInsideLink::kInsideVisitedLink);

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("outline-color"));

  Color green(0, 128, 0);
  const CSSProperty& outline_color = GetCSSPropertyOutlineColor();
  EXPECT_EQ(green, cascade.TakeStyle()->VisitedDependentColor(outline_color));
}

}  // namespace blink
