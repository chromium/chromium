// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include <vector>

#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_appearance_auto_base_select_value_pair.h"
#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/document_style_sheet_collection.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using css_test_helpers::ParseDeclarationBlock;
using css_test_helpers::RegisterProperty;
using Origin = CascadeOrigin;
using Priority = CascadePriority;
using UnitType = CSSPrimitiveValue::UnitType;

class TestCascadeResolver {
  STACK_ALLOCATED();

 public:
  explicit TestCascadeResolver(uint8_t generation = 0)
      : resolver_(CascadeFilter(), generation) {}
  bool InCycle() const { return resolver_.InCycle(); }
  bool DetectCycle(const CSSProperty& property) {
    return resolver_.DetectCycle(property);
  }
  wtf_size_t CycleStart() const { return resolver_.cycle_start_; }
  wtf_size_t CycleEnd() const { return resolver_.cycle_end_; }
  uint8_t GetGeneration() { return resolver_.generation_; }
  CascadeResolver& InnerResolver() { return resolver_; }
  const CSSProperty* CurrentProperty() const {
    return resolver_.CurrentProperty();
  }

 private:
  friend class TestCascadeAutoLock;

  CascadeResolver resolver_;
};

struct AddOptions {
  CascadeOrigin origin = CascadeOrigin::kAuthor;
  unsigned link_match_type = CSSSelector::kMatchAll;
  uint16_t layer_order = CascadeLayerMap::kImplicitOuterLayerOrder;
  bool is_inline_style = false;
  bool is_try_style = false;
  bool is_try_tactics_style = false;
};

class TestCascade {
  STACK_ALLOCATED();

 public:
  explicit TestCascade(Document& document,
                       Element* target = nullptr,
                       const StyleRecalcContext* style_recalc_context = nullptr)
      : state_(document,
               target ? *target : *document.body(),
               style_recalc_context),
        cascade_(InitState(state_, nullptr)) {}

  TestCascade(Document& document,
              const ComputedStyle* parent_style,
              Element* target = nullptr)
      : state_(document, target ? *target : *document.body()),
        cascade_(InitState(state_, parent_style)) {}

  const ComputedStyle* TakeStyle() { return state_.TakeStyle(); }

  StyleResolverState& State() { return state_; }
  StyleCascade& InnerCascade() { return cascade_; }

  //  Note that because of how MatchResult works, declarations must be added
  //  in "origin order", i.e. UserAgent first, then User, then Author.

  void Add(String block, AddOptions options = {}) {
    CSSParserMode mode = options.origin == CascadeOrigin::kUserAgent
                             ? kUASheetMode
                             : kHTMLStandardMode;
    Add(ParseDeclarationBlock(block, mode), options);
  }

  void Add(String block, CascadeOrigin origin) {
    Add(block, {.origin = origin});
  }

  void Add(String name, String value, CascadeOrigin origin = Origin::kAuthor) {
    Add(name + ":" + value, origin);
  }

  void Add(const CSSPropertyValueSet* set, AddOptions options = {}) {
    DCHECK_LE(options.origin, CascadeOrigin::kAuthor)
        << "Animations not supported";
    DCHECK_LE(current_origin_, options.origin)
        << "Please add declarations in order";
    EnsureAtLeast(options.origin);
    cascade_.MutableMatchResult().AddMatchedProperties(
        set,
        {
            .link_match_type = static_cast<uint8_t>(options.link_match_type),
            .is_inline_style = options.is_inline_style,
            .is_try_style = options.is_try_style,
            .origin = options.origin,
            .layer_order = options.layer_order,
            .is_try_tactics_style = options.is_try_tactics_style,
        });
  }

  void Apply(CascadeFilter filter = CascadeFilter()) {
    EnsureAtLeast(CascadeOrigin::kAnimation);
    cascade_.Apply(filter);
  }

  void ApplySingle(const CSSProperty& property) {
    EnsureAtLeast(CascadeOrigin::kAnimation);
    cascade_.AnalyzeIfNeeded();
    TestCascadeResolver resolver(++cascade_.generation_);
    cascade_.LookupAndApply(property, resolver.InnerResolver());
  }

  void AnalyzeIfNeeded() { cascade_.AnalyzeIfNeeded(); }

  const CSSValue* Resolve(const CSSProperty& property,
                          const CSSValue& value,
                          CascadeOrigin& origin) {
    TestCascadeResolver resolver;
    return cascade_.Resolve(property, value, CascadePriority(origin), origin,
                            resolver.InnerResolver());
  }

  static const CSSValue* StaticResolve(StyleResolverState& state,
                                       String name,
                                       String value) {
    const CSSPropertyValueSet* set =
        ParseDeclarationBlock(name + ":" + value, kHTMLStandardMode);
    DCHECK(set);
    DCHECK(set->PropertyCount());
    CSSPropertyValueSet::PropertyReference reference = set->PropertyAt(0);
    return StyleCascade::Resolve(state, reference.Name(), reference.Value());
  }

  std::unique_ptr<CSSBitset> GetImportantSet() {
    return cascade_.GetImportantSet();
  }

  String ComputedValue(String name) const {
    CSSPropertyRef ref(name, GetDocument());
    DCHECK(ref.IsValid());
    const LayoutObject* layout_object = nullptr;
    bool allow_visited_style = false;
    CSSValuePhase value_phase = CSSValuePhase::kResolvedValue;
    const ComputedStyle* style = state_.StyleBuilder().CloneStyle();
    const CSSValue* value = ref.GetProperty().CSSValueFromComputedStyle(
        *style, layout_object, allow_visited_style, value_phase);
    return value ? value->CssText() : g_null_atom;
  }

  CascadePriority GetPriority(String name) {
    return GetPriority(
        *CSSPropertyName::From(GetDocument().GetExecutionContext(), name));
  }

  CascadePriority* FindPriority(CSSPropertyName name) {
    return cascade_.map_.Find(name);
  }

  CascadePriority GetPriority(CSSPropertyName name) {
    CascadePriority* c = FindPriority(name);
    return c ? *c : CascadePriority();
  }

  CascadeOrigin GetOrigin(String name) { return GetPriority(name).GetOrigin(); }

  void AddInterpolations() {
    state_.StyleBuilder().SetBaseData(
        StyleBaseData::Create(state_.StyleBuilder().CloneStyle(), nullptr));

    CalculateInterpolationUpdate();

    // Add to cascade:
    const auto& update = state_.AnimationUpdate();
    if (update.IsEmpty()) {
      return;
    }

    cascade_.AddInterpolations(&update.ActiveInterpolationsForAnimations(),
                               CascadeOrigin::kAnimation);
    cascade_.AddInterpolations(&update.ActiveInterpolationsForTransitions(),
                               CascadeOrigin::kTransition);
  }

  void Reset() {
    cascade_.Reset();
    current_origin_ = CascadeOrigin::kUserAgent;
  }

  bool NeedsMatchResultAnalyze() const {
    return cascade_.needs_match_result_analyze_;
  }
  bool NeedsInterpolationsAnalyze() const {
    return cascade_.needs_interpolations_analyze_;
  }
  bool DependsOnCascadeAffectingProperty() const {
    return cascade_.depends_on_cascade_affecting_property_;
  }
  bool InlineStyleLostCascade() const { return cascade_.InlineStyleLost(); }

  HeapHashMap<CSSPropertyName, Member<const CSSValue>> GetCascadedValues()
      const {
    return cascade_.GetCascadedValues();
  }

 private:
  Document& GetDocument() const { return state_.GetDocument(); }
  Element* Body() const { return GetDocument().body(); }

  static StyleResolverState& InitState(StyleResolverState& state,
                                       const ComputedStyle* parent_style) {
    state.GetDocument().GetStyleEngine().UpdateViewportSize();
    if (parent_style) {
      state.CreateNewStyle(*InitialStyle(state.GetDocument()), *parent_style);
      state.SetParentStyle(parent_style);
    } else {
      state.SetStyle(*InitialStyle(state.GetDocument()));
      state.SetParentStyle(InitialStyle(state.GetDocument()));
    }
    state.SetOldStyle(state.GetElement().GetComputedStyle());
    return state;
  }

  static const ComputedStyle* InitialStyle(Document& document) {
    return document.GetStyleResolver().InitialStyleForElement();
  }

  void FinishOrigin() {
    switch (current_origin_) {
      case CascadeOrigin::kUserAgent:
        current_origin_ = CascadeOrigin::kUser;
        break;
      case CascadeOrigin::kUser:
        current_origin_ = CascadeOrigin::kAuthorPresentationalHint;
        break;
      case CascadeOrigin::kAuthorPresentationalHint:
        cascade_.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(
            GetDocument());
        current_origin_ = CascadeOrigin::kAuthor;
        break;
      case CascadeOrigin::kAuthor:
        current_origin_ = CascadeOrigin::kAnimation;
        break;
      case CascadeOrigin::kAnimation:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void EnsureAtLeast(CascadeOrigin origin) {
    while (current_origin_ < origin) {
      FinishOrigin();
    }
  }

  void CalculateInterpolationUpdate() {
    CSSAnimations::CalculateTransitionUpdate(
        state_.AnimationUpdate(), state_.GetElement(), state_.StyleBuilder(),
        state_.OldStyle(), true /* can_trigger_animations */);
    CSSAnimations::CalculateAnimationUpdate(
        state_.AnimationUpdate(), state_.GetElement(), state_.GetElement(),
        state_.StyleBuilder(), state_.ParentStyle(),
        &GetDocument().GetStyleResolver(), true /* can_trigger_animations */);
  }

  CascadeOrigin current_origin_ = CascadeOrigin::kUserAgent;
  StyleResolverState state_;
  StyleCascade cascade_;
};

class TestCascadeAutoLock {
  STACK_ALLOCATED();

 public:
  TestCascadeAutoLock(const CSSProperty& property,
                      TestCascadeResolver& resolver)
      : lock_(property, resolver.resolver_) {}

 private:
  CascadeResolver::AutoLock lock_;
};

class StyleCascadeTest : public PageTestBase {
 public:
  CSSStyleSheet* CreateSheet(const String& css_text) {
    auto* init = MakeGarbageCollected<CSSStyleSheetInit>();
    DummyExceptionStateForTesting exception_state;
    CSSStyleSheet* sheet =
        CSSStyleSheet::Create(GetDocument(), init, exception_state);
    sheet->replaceSync(css_text, exception_state);
    sheet->Contents()->EnsureRuleSet(
        MediaQueryEvaluator(GetDocument().GetFrame()));
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
    GetDocument()
        .GetStyleEngine()
        .GetDocumentStyleSheetCollection()
        .AppendActiveStyleSheet(active_sheets[0]);
  }

  Element* DocumentElement() const { return GetDocument().documentElement(); }

  void SetRootFont(String value) {
    DocumentElement()->SetInlineStyleProperty(CSSPropertyID::kFontSize, value);
    UpdateAllLifecyclePhasesForTest();
  }

  const MutableCSSPropertyValueSet* AnimationTaintedSet(const char* name,
                                                        String value) {
    CSSParserMode mode = kHTMLStandardMode;
    auto* set = MakeGarbageCollected<MutableCSSPropertyValueSet>(mode);
    set->ParseAndSetCustomProperty(AtomicString(name), value,
                                   /* important */ false,
                                   SecureContextMode::kSecureContext,
                                   /* context_style_sheet */ nullptr,
                                   /* is_animation_tainted */ true);
    return set;
  }

  const CSSPropertyValueSet* FlipRevertSet(String from_property,
                                           String to_property) {
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    set->SetProperty(PropertyName(from_property).Id(),
                     *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(
                         PropertyName(to_property).Id(), TryTacticTransform()));
    return set;
  }

  // Temporarily create a CSS Environment Variable.
  // https://drafts.csswg.org/css-env-1/
  class AutoEnv {
    STACK_ALLOCATED();

   public:
    AutoEnv(PageTestBase& test, const char* name, String value)
        : document_(&test.GetDocument()), name_(name) {
      EnsureEnvironmentVariables().SetVariable(name_, value);
    }
    ~AutoEnv() { EnsureEnvironmentVariables().RemoveVariable(name_); }

   private:
    DocumentStyleEnvironmentVariables& EnsureEnvironmentVariables() {
      return document_->GetStyleEngine().EnsureEnvironmentVariables();
    }

    Document* document_;
    AtomicString name_;
  };

  CSSPropertyName PropertyName(String name) {
    return *CSSPropertyName::From(GetDocument().GetExecutionContext(), name);
  }

  String CssText(const CSSValue* value) {
    if (!value) {
      return g_null_atom;
    }
    return value->CssText();
  }

  String CssTextAt(
      const HeapHashMap<CSSPropertyName, Member<const CSSValue>>& map,
      String name) {
    return CssText(map.at(PropertyName(name)));
  }
};

TEST_F(StyleCascadeTest, ApplySingle) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "1px", CascadeOrigin::kUserAgent);
  cascade.Add("width", "2px", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("2px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyImportance) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px !important", CascadeOrigin::kUserAgent);
  cascade.Add("width:2px", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyAll) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px", CascadeOrigin::kUserAgent);
  cascade.Add("height:1px", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("auto", cascade.ComputedValue("width"));
  EXPECT_EQ("auto", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, ApplyAllImportance) {
  TestCascade cascade(GetDocument());
  cascade.Add("opacity:0.5", CascadeOrigin::kUserAgent);
  cascade.Add("display:block !important", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("1", cascade.ComputedValue("opacity"));
  EXPECT_EQ("block", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, ApplyAllWithPhysicalLonghands) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px", CascadeOrigin::kUserAgent);
  cascade.Add("height:1px !important", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial", CascadeOrigin::kAuthor);
  cascade.Apply();
  EXPECT_EQ("auto", cascade.ComputedValue("width"));
  EXPECT_EQ("1px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, ApplyCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", " 10px ");
  cascade.Add("--y", "nope");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("nope", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, ApplyGenerations) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:10px");
  cascade.Add("width:20px");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("20px", cascade.ComputedValue("width"));

  cascade.State().StyleBuilder().SetWidth(Length::Auto());
  cascade.State().StyleBuilder().SetVariableData(AtomicString("--x"), nullptr,
                                                 true);
  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--x"));
  EXPECT_EQ("auto", cascade.ComputedValue("width"));

  // Apply again
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("20px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyCustomPropertyVar) {
  // Apply --x first.
  {
    TestCascade cascade(GetDocument());
    cascade.Add("--x", "yes and var(--y)");
    cascade.Add("--y", "no");
    cascade.Apply();

    EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
    EXPECT_EQ("no", cascade.ComputedValue("--y"));
  }

  // Apply --y first.
  {
    TestCascade cascade(GetDocument());
    cascade.Add("--y", "no");
    cascade.Add("--x", "yes and var(--y)");
    cascade.Apply();

    EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
    EXPECT_EQ("no", cascade.ComputedValue("--y"));
  }
}

TEST_F(StyleCascadeTest, InvalidVarReferenceCauseInvalidVariable) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "nope var(--y)");
  cascade.Apply();

  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, ApplyCustomPropertyFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "yes and var(--y,no)");
  cascade.Apply();

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
  cascade.Apply();

  EXPECT_EQ("one two three", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, VarReferenceInNormalProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Add("width", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, MultipleVarRefs) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "var(--y) bar var(--y)");
  cascade.Add("--y", "foo");
  cascade.Apply();

  EXPECT_EQ("foo bar foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyComputedValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Apply();

  EXPECT_EQ("96px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertySyntaxErrorCausesInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "10px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "#fefefe");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("10px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredPropertySubstitution) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();

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
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("5px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ApplyingPendingSubstitutionLast) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin-right", "5px");
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("2px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, PendingSubstitutionInLogicalShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin-inline:var(--x)");
  cascade.Add("--x:10px 20px");
  cascade.Add("direction:rtl");
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("margin-left"));
  EXPECT_EQ("10px", cascade.ComputedValue("margin-right"));
}

TEST_F(StyleCascadeTest, DetectCycleByName) {
  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver;

  // Two different CustomProperty instances with the same name:
  CustomProperty a1(AtomicString("--a"), GetDocument());
  CustomProperty a2(AtomicString("--a"), GetDocument());

  {
    TestCascadeAutoLock lock(a1, resolver);
    EXPECT_FALSE(resolver.InCycle());

    // This should still be detected as a cycle, even though it's not the same
    // CustomProperty instance.
    EXPECT_TRUE(resolver.DetectCycle(a2));
    EXPECT_TRUE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectCycle) {
  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());
  CustomProperty b(AtomicString("--b"), GetDocument());
  CustomProperty c(AtomicString("--c"), GetDocument());

  {
    TestCascadeAutoLock lock_a(a, resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      TestCascadeAutoLock lock_b(b, resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        TestCascadeAutoLock lock_c(c, resolver);
        EXPECT_FALSE(resolver.InCycle());

        EXPECT_TRUE(resolver.DetectCycle(a));
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
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());
  CustomProperty b(AtomicString("--b"), GetDocument());
  CustomProperty c(AtomicString("--c"), GetDocument());
  CustomProperty x(AtomicString("--x"), GetDocument());

  {
    TestCascadeAutoLock lock_a(a, resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      TestCascadeAutoLock lock_b(b, resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        TestCascadeAutoLock lock_c(c, resolver);
        EXPECT_FALSE(resolver.InCycle());

        EXPECT_FALSE(resolver.DetectCycle(x));
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
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());

  {
    TestCascadeAutoLock lock(a, resolver);
    EXPECT_FALSE(resolver.InCycle());

    EXPECT_TRUE(resolver.DetectCycle(a));
    EXPECT_TRUE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectMultiCycle) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());
  CustomProperty b(AtomicString("--b"), GetDocument());
  CustomProperty c(AtomicString("--c"), GetDocument());
  CustomProperty d(AtomicString("--d"), GetDocument());

  {
    AutoLock lock_a(a, resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock_b(b, resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        AutoLock lock_c(c, resolver);
        EXPECT_FALSE(resolver.InCycle());
        {
          AutoLock lock_d(d, resolver);
          EXPECT_FALSE(resolver.InCycle());

          // Cycle 1 (big cycle):
          EXPECT_TRUE(resolver.DetectCycle(b));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleStart());

          // Cycle 2 (small cycle):
          EXPECT_TRUE(resolver.DetectCycle(c));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleStart());
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
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());
  CustomProperty b(AtomicString("--b"), GetDocument());
  CustomProperty c(AtomicString("--c"), GetDocument());
  CustomProperty d(AtomicString("--d"), GetDocument());

  {
    AutoLock lock_a(a, resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock_b(b, resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        AutoLock lock_c(c, resolver);
        EXPECT_FALSE(resolver.InCycle());
        {
          AutoLock lock_d(d, resolver);
          EXPECT_FALSE(resolver.InCycle());

          // Cycle 1 (small cycle):
          EXPECT_TRUE(resolver.DetectCycle(c));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(2u, resolver.CycleStart());

          // Cycle 2 (big cycle):
          EXPECT_TRUE(resolver.DetectCycle(b));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleStart());
        }
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, CurrentProperty) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());
  CustomProperty b(AtomicString("--b"), GetDocument());
  CustomProperty c(AtomicString("--c"), GetDocument());

  EXPECT_FALSE(resolver.CurrentProperty());
  {
    AutoLock lock_a(a, resolver);
    EXPECT_EQ(&a, resolver.CurrentProperty());
    {
      AutoLock lock_b(b, resolver);
      EXPECT_EQ(&b, resolver.CurrentProperty());
      {
        AutoLock lock_c(c, resolver);
        EXPECT_EQ(&c, resolver.CurrentProperty());
      }
      EXPECT_EQ(&b, resolver.CurrentProperty());
    }
    EXPECT_EQ(&a, resolver.CurrentProperty());
  }
  EXPECT_FALSE(resolver.CurrentProperty());
}

TEST_F(StyleCascadeTest, CycleWithExtraEdge) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver;

  CustomProperty a(AtomicString("--a"), GetDocument());
  CustomProperty b(AtomicString("--b"), GetDocument());
  CustomProperty c(AtomicString("--c"), GetDocument());
  CustomProperty d(AtomicString("--d"), GetDocument());

  {
    AutoLock lock_a(a, resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock_b(b, resolver);
      EXPECT_FALSE(resolver.InCycle());

      {
        AutoLock lock_c(c, resolver);
        EXPECT_FALSE(resolver.InCycle());

        // Cycle:
        EXPECT_TRUE(resolver.DetectCycle(b));
        EXPECT_TRUE(resolver.InCycle());
        EXPECT_EQ(1u, resolver.CycleStart());
        EXPECT_EQ(3u, resolver.CycleEnd());
      }

      // ~AutoLock must shrink the in-cycle range:
      EXPECT_EQ(1u, resolver.CycleStart());
      EXPECT_EQ(2u, resolver.CycleEnd());

      {
        // We should not be in a cycle when locking a new property ...
        AutoLock lock_d(d, resolver);
        EXPECT_FALSE(resolver.InCycle());
        // AutoLock ctor does not affect in-cycle range:
        EXPECT_EQ(1u, resolver.CycleStart());
        EXPECT_EQ(2u, resolver.CycleEnd());
      }

      EXPECT_EQ(1u, resolver.CycleStart());
      EXPECT_EQ(2u, resolver.CycleEnd());

      // ... however we should be back InCycle when that AutoLock is destroyed.
      EXPECT_TRUE(resolver.InCycle());
    }

    // ~AutoLock should reduce cycle-end to equal cycle-start, hence we
    // are no longer in a cycle.
    EXPECT_EQ(kNotFound, resolver.CycleStart());
    EXPECT_EQ(kNotFound, resolver.CycleEnd());
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

  cascade.Reset();
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

  cascade.Reset();
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

  EXPECT_EQ("0px", cascade.ComputedValue("--a"));
  EXPECT_EQ("0px", cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, UniversalSyntaxCycle) {
  RegisterProperty(GetDocument(), "--a", "*", "foo", false);
  RegisterProperty(GetDocument(), "--b", "*", "bar", false);

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

  EXPECT_EQ("0px", cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, ReferencedRegisteredCycle) {
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

  EXPECT_EQ("0px", cascade.ComputedValue("--a"));
  EXPECT_EQ("0px", cascade.ComputedValue("--b"));
  EXPECT_EQ("0px", cascade.ComputedValue("--c"));
  EXPECT_EQ("0px", cascade.ComputedValue("--d"));
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
  cascade.Add("color:var(--a,green)");
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

  TestCascade cascade(GetDocument(), parent.TakeStyle());
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

  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, SubstitutingEmCycles) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Add("--y", "var(--x)");
  cascade.Add("--z", "var(--x,1px)");
  cascade.Apply();

  EXPECT_EQ("0px", cascade.ComputedValue("--y"));
  EXPECT_EQ("0px", cascade.ComputedValue("--z"));
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

  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInRootFontSizeNonCycle) {
  TestCascade cascade(GetDocument(), DocumentElement());
  cascade.Add("font-size", "initial");
  cascade.Apply();

  String expected = cascade.ComputedValue("font-size");

  cascade.Reset();
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

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  cascade.Add("--y", "foo");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
  EXPECT_EQ("foo", cascade.ComputedValue("--y"));

  cascade.Reset();
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

  TestCascade cascade(GetDocument(), parent.TakeStyle());

  EXPECT_EQ("foo", cascade.ComputedValue("--x"));

  cascade.Add("--x", "bar");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));

  cascade.Reset();
  cascade.Add("--x", "inherit");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, Unset) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));

  cascade.Add("--x", "bar");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));

  cascade.Reset();
  cascade.Add("--x", "unset");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertUA) {
  TestCascade cascade(GetDocument());
  cascade.Add("display:block", CascadeOrigin::kUserAgent);
  cascade.Add("display:revert", CascadeOrigin::kUserAgent);

  cascade.Add("display:block", CascadeOrigin::kUser);
  cascade.Add("display:revert", CascadeOrigin::kUser);

  cascade.Add("display:block", CascadeOrigin::kAuthor);
  cascade.Add("display:revert", CascadeOrigin::kAuthor);

  cascade.Apply();

  EXPECT_EQ("inline", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, RevertStandardProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("left:10px", CascadeOrigin::kUserAgent);
  cascade.Add("right:10px", CascadeOrigin::kUserAgent);

  cascade.Add("right:20px", CascadeOrigin::kUser);
  cascade.Add("right:revert", CascadeOrigin::kUser);
  cascade.Add("top:20px", CascadeOrigin::kUser);
  cascade.Add("bottom:20px", CascadeOrigin::kUser);

  cascade.Add("bottom:30px", CascadeOrigin::kAuthor);
  cascade.Add("bottom:revert", CascadeOrigin::kAuthor);
  cascade.Add("left:30px", CascadeOrigin::kAuthor);
  cascade.Add("left:revert", CascadeOrigin::kAuthor);
  cascade.Add("right:revert", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("top"));
  EXPECT_EQ("10px", cascade.ComputedValue("right"));
  EXPECT_EQ("20px", cascade.ComputedValue("bottom"));
  EXPECT_EQ("10px", cascade.ComputedValue("left"));
}

TEST_F(StyleCascadeTest, RevertCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:10px", CascadeOrigin::kUser);

  cascade.Add("--y:fail", CascadeOrigin::kAuthor);

  cascade.Add("--x:revert", CascadeOrigin::kAuthor);
  cascade.Add("--y:revert", CascadeOrigin::kAuthor);

  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_FALSE(cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RevertChain) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:10px", CascadeOrigin::kUserAgent);

  cascade.Add("width:revert", CascadeOrigin::kUser);
  cascade.Add("--x:revert", CascadeOrigin::kUser);

  cascade.Add("width:revert", CascadeOrigin::kAuthor);
  cascade.Add("--x:revert", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_FALSE(cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertFromAuthorToUA) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:10px", CascadeOrigin::kUserAgent);
  cascade.Add("height:10px", CascadeOrigin::kUserAgent);

  cascade.Add("width:20px", CascadeOrigin::kAuthor);
  cascade.Add("height:20px", CascadeOrigin::kAuthor);
  cascade.Add("width:revert", CascadeOrigin::kAuthor);
  cascade.Add("height:revert", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("10px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, RevertInitialFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:20px", CascadeOrigin::kAuthor);
  cascade.Add("width:revert", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("auto", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, RevertInheritedFallback) {
  TestCascade parent(GetDocument());
  parent.Add("color", "red");
  parent.Apply();

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("color"));

  cascade.Add("color:black", CascadeOrigin::kAuthor);
  cascade.Add("color:revert", CascadeOrigin::kAuthor);
  cascade.Apply();
  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("color"));
}

TEST_F(StyleCascadeTest, RevertRegistered) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x:20px", CascadeOrigin::kUser);
  cascade.Add("--x:100px", CascadeOrigin::kAuthor);
  cascade.Add("--x:revert", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertRegisteredInitialFallback) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x:20px", CascadeOrigin::kAuthor);
  cascade.Add("--x:revert", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertRegisteredInheritedFallback) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", true);

  TestCascade parent(GetDocument());
  parent.Add("--x", "1px");
  parent.Apply();

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  EXPECT_EQ("1px", cascade.ComputedValue("--x"));

  cascade.Add("--x:100px", CascadeOrigin::kAuthor);
  cascade.Add("--x:revert", CascadeOrigin::kAuthor);
  cascade.Apply();
  EXPECT_EQ("1px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertUASurrogate) {
  TestCascade cascade(GetDocument());

  // User-agent:

  // Only logical:
  cascade.Add("inline-size:10px", CascadeOrigin::kUserAgent);
  cascade.Add("min-inline-size:11px", CascadeOrigin::kUserAgent);
  // Only physical:
  cascade.Add("height:12px", CascadeOrigin::kUserAgent);
  cascade.Add("min-height:13px", CascadeOrigin::kUserAgent);
  // Physical first:
  cascade.Add("margin-left:14px", CascadeOrigin::kUserAgent);
  cascade.Add("padding-left:15px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-inline-start:16px", CascadeOrigin::kUserAgent);
  cascade.Add("padding-inline-start:17px", CascadeOrigin::kUserAgent);
  // Logical first:
  cascade.Add("margin-inline-end:18px", CascadeOrigin::kUserAgent);
  cascade.Add("padding-inline-end:19px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-right:20px", CascadeOrigin::kUserAgent);
  cascade.Add("padding-right:21px", CascadeOrigin::kUserAgent);

  // Author:

  cascade.Add("width:100px", CascadeOrigin::kAuthor);
  cascade.Add("height:101px", CascadeOrigin::kAuthor);
  cascade.Add("margin:102px", CascadeOrigin::kAuthor);
  cascade.Add("padding:103px", CascadeOrigin::kAuthor);
  cascade.Add("min-width:104px", CascadeOrigin::kAuthor);
  cascade.Add("min-height:105px", CascadeOrigin::kAuthor);
  // Revert via physical:
  cascade.Add("width:revert", CascadeOrigin::kAuthor);
  cascade.Add("height:revert", CascadeOrigin::kAuthor);
  cascade.Add("margin-left:revert", CascadeOrigin::kAuthor);
  cascade.Add("margin-right:revert", CascadeOrigin::kAuthor);
  // Revert via logical:
  cascade.Add("min-inline-size:revert", CascadeOrigin::kAuthor);
  cascade.Add("min-block-size:revert", CascadeOrigin::kAuthor);
  cascade.Add("padding-inline-start:revert", CascadeOrigin::kAuthor);
  cascade.Add("padding-inline-end:revert", CascadeOrigin::kAuthor);

  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("12px", cascade.ComputedValue("height"));
  EXPECT_EQ("11px", cascade.ComputedValue("min-width"));
  EXPECT_EQ("13px", cascade.ComputedValue("min-height"));
  EXPECT_EQ("102px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("20px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("102px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("16px", cascade.ComputedValue("margin-left"));
  EXPECT_EQ("103px", cascade.ComputedValue("padding-top"));
  EXPECT_EQ("21px", cascade.ComputedValue("padding-right"));
  EXPECT_EQ("103px", cascade.ComputedValue("padding-bottom"));
  EXPECT_EQ("17px", cascade.ComputedValue("padding-left"));

  EXPECT_EQ("10px", cascade.ComputedValue("inline-size"));
  EXPECT_EQ("12px", cascade.ComputedValue("block-size"));
  EXPECT_EQ("11px", cascade.ComputedValue("min-inline-size"));
  EXPECT_EQ("13px", cascade.ComputedValue("min-block-size"));
  EXPECT_EQ("102px", cascade.ComputedValue("margin-block-start"));
  EXPECT_EQ("20px", cascade.ComputedValue("margin-inline-end"));
  EXPECT_EQ("102px", cascade.ComputedValue("margin-block-end"));
  EXPECT_EQ("16px", cascade.ComputedValue("margin-inline-start"));
  EXPECT_EQ("103px", cascade.ComputedValue("padding-block-start"));
  EXPECT_EQ("21px", cascade.ComputedValue("padding-inline-end"));
  EXPECT_EQ("103px", cascade.ComputedValue("padding-block-end"));
  EXPECT_EQ("17px", cascade.ComputedValue("padding-inline-start"));
}

TEST_F(StyleCascadeTest, RevertWithImportantPhysical) {
  TestCascade cascade(GetDocument());
  cascade.Add("inline-size:10px", CascadeOrigin::kUserAgent);
  cascade.Add("block-size:11px", CascadeOrigin::kUserAgent);

  cascade.Add("width:100px", CascadeOrigin::kAuthor);
  cascade.Add("height:101px", CascadeOrigin::kAuthor);
  cascade.Add("width:revert !important", CascadeOrigin::kAuthor);
  cascade.Add("inline-size:101px", CascadeOrigin::kAuthor);
  cascade.Add("block-size:102px", CascadeOrigin::kAuthor);
  cascade.Add("height:revert !important", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("11px", cascade.ComputedValue("height"));
  EXPECT_EQ("10px", cascade.ComputedValue("inline-size"));
  EXPECT_EQ("11px", cascade.ComputedValue("block-size"));
}

TEST_F(StyleCascadeTest, RevertWithImportantLogical) {
  TestCascade cascade(GetDocument());
  cascade.Add("inline-size:10px", CascadeOrigin::kUserAgent);
  cascade.Add("block-size:11px", CascadeOrigin::kUserAgent);

  cascade.Add("inline-size:revert !important", CascadeOrigin::kAuthor);
  cascade.Add("width:100px", CascadeOrigin::kAuthor);
  cascade.Add("height:101px", CascadeOrigin::kAuthor);
  cascade.Add("block-size:revert !important", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("11px", cascade.ComputedValue("height"));
  EXPECT_EQ("10px", cascade.ComputedValue("inline-size"));
  EXPECT_EQ("11px", cascade.ComputedValue("block-size"));
}

TEST_F(StyleCascadeTest, RevertSurrogateChain) {
  TestCascade cascade(GetDocument());

  cascade.Add("inline-size:revert", CascadeOrigin::kUserAgent);
  cascade.Add("block-size:10px", CascadeOrigin::kUserAgent);
  cascade.Add("min-inline-size:11px", CascadeOrigin::kUserAgent);
  cascade.Add("min-block-size:12px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-inline:13px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-block:14px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-top:revert", CascadeOrigin::kUserAgent);
  cascade.Add("margin-left:15px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-bottom:16px", CascadeOrigin::kUserAgent);
  cascade.Add("margin-block-end:17px", CascadeOrigin::kUserAgent);

  cascade.Add("inline-size:101px", CascadeOrigin::kUser);
  cascade.Add("block-size:102px", CascadeOrigin::kUser);
  cascade.Add("width:revert", CascadeOrigin::kUser);
  cascade.Add("height:revert", CascadeOrigin::kUser);
  cascade.Add("min-inline-size:103px", CascadeOrigin::kUser);
  cascade.Add("min-block-size:104px", CascadeOrigin::kUser);
  cascade.Add("margin:105px", CascadeOrigin::kUser);
  cascade.Add("margin-block-start:revert", CascadeOrigin::kUser);
  cascade.Add("margin-inline-start:106px", CascadeOrigin::kUser);
  cascade.Add("margin-block-end:revert", CascadeOrigin::kUser);
  cascade.Add("margin-right:107px", CascadeOrigin::kUser);

  cascade.Add("inline-size:revert", CascadeOrigin::kAuthor);
  cascade.Add("block-size:revert", CascadeOrigin::kAuthor);
  cascade.Add("min-inline-size:revert", CascadeOrigin::kAuthor);
  cascade.Add("min-block-size:1001px", CascadeOrigin::kAuthor);
  cascade.Add("margin:1002px", CascadeOrigin::kAuthor);
  cascade.Add("margin-top:revert", CascadeOrigin::kAuthor);
  cascade.Add("margin-left:1003px", CascadeOrigin::kAuthor);
  cascade.Add("margin-bottom:1004px", CascadeOrigin::kAuthor);
  cascade.Add("margin-right:1005px", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("auto", cascade.ComputedValue("width"));
  EXPECT_EQ("10px", cascade.ComputedValue("height"));
  EXPECT_EQ("103px", cascade.ComputedValue("min-width"));
  EXPECT_EQ("1001px", cascade.ComputedValue("min-height"));
  EXPECT_EQ("0px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("1005px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("1004px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("1003px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, RevertInKeyframe) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { margin-left: 0px; }
        to { margin-left: revert; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("margin-left:100px", CascadeOrigin::kUserAgent);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("50px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, RevertToCustomPropertyInKeyframe) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 0px; }
        to { --x: revert; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("--x:100px", CascadeOrigin::kUser);
  cascade.Add("--x:1000px", CascadeOrigin::kAuthor);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("50px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertToCustomPropertyInKeyframeUnset) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "1000px", true);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 100px; --y: 100px; }
        to { --x: revert; --y: revert; }
     }
    )HTML");

  TestCascade parent(GetDocument());
  parent.Add("--y: 0px");
  parent.Apply();
  EXPECT_EQ("0px", parent.ComputedValue("--y"));

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  cascade.Add("--x:10000px", CascadeOrigin::kAuthor);
  cascade.Add("--y:10000px", CascadeOrigin::kAuthor);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("50px", cascade.ComputedValue("--x"));
  EXPECT_EQ("50px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RevertToCustomPropertyInKeyframeEmptyInherit) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", true);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 100px; }
        to { --x: revert; }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.Add("--x:10000px", CascadeOrigin::kAuthor);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("50px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertInKeyframeResponsive) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { margin-left: 0px; }
        to { margin-left: revert; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("--x:100px", CascadeOrigin::kUser);
  cascade.Add("margin-left:var(--x)", CascadeOrigin::kUser);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();
  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("50px", cascade.ComputedValue("margin-left"));

  cascade.Reset();
  cascade.Add("--x:100px", CascadeOrigin::kUser);
  cascade.Add("margin-left:var(--x)", CascadeOrigin::kUser);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Add("--x:80px", CascadeOrigin::kAuthor);
  cascade.Apply();
  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("40px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, RevertToCycleInKeyframe) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 100px; }
        to { --x: revert; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("--x:var(--y)", CascadeOrigin::kUser);
  cascade.Add("--y:var(--x)", CascadeOrigin::kUser);
  cascade.Add("--x:200px", CascadeOrigin::kAuthor);
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RevertCausesTransition) {
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade1(GetDocument());
  cascade1.Add("width:200px", CascadeOrigin::kUser);
  cascade1.Add("width:100px", CascadeOrigin::kAuthor);
  cascade1.Add("transition: width 1000s steps(2, end)", CascadeOrigin::kAuthor);
  cascade1.Apply();

  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with new color values.
  TestCascade cascade2(GetDocument());
  cascade2.Add("width:200px", CascadeOrigin::kUser);
  cascade2.Add("width:100px", CascadeOrigin::kAuthor);
  cascade2.Add("width:revert", CascadeOrigin::kAuthor);
  cascade2.Add("transition: width 1000s steps(2, start)",
               CascadeOrigin::kAuthor);
  cascade2.Apply();

  cascade2.AddInterpolations();
  cascade2.Apply();

  EXPECT_EQ("150px", cascade2.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, CSSWideKeywordsInFallbacks) {
  {
    TestCascade cascade(GetDocument());
    cascade.Add("display:var(--u,initial)");
    cascade.Add("margin:var(--u,initial)");
    cascade.Apply();
  }
  {
    TestCascade cascade(GetDocument());
    cascade.Add("display:var(--u,inherit)");
    cascade.Add("margin:var(--u,inherit)");
    cascade.Apply();
  }
  {
    TestCascade cascade(GetDocument());
    cascade.Add("display:var(--u,unset)");
    cascade.Add("margin:var(--u,unset)");
    cascade.Apply();
  }
  {
    TestCascade cascade(GetDocument());
    cascade.Add("display:var(--u,revert)");
    cascade.Add("margin:var(--u,revert)");
    cascade.Apply();
  }

  // TODO(crbug.com/1105782): Specs and WPT are currently in conflict
  // regarding the correct behavior here. For now this test just verifies
  // that we don't crash.
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
  EXPECT_EQ("13px", cascade.ComputedValue("--y"));
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
  RegisterProperty(GetDocument(), "--x", "*", std::nullopt, false);

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
  EXPECT_EQ("foo", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));

  cascade.Reset();
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

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));  // Note: inherit==false

  cascade.Reset();
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

  TestCascade cascade(GetDocument(), parent.TakeStyle());
  cascade.Add("--x", "2px");
  cascade.Add("--y", "2px");
  cascade.Apply();
  EXPECT_EQ("2px", cascade.ComputedValue("--x"));
  EXPECT_EQ("2px", cascade.ComputedValue("--y"));

  cascade.Reset();
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
  cascade.Add(AnimationTaintedSet("--x", "15px"));
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInStandardProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "15px"));
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInAnimationDelay) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "1s"));
  cascade.Add("animation-delay", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("1s", cascade.ComputedValue("--x"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-delay"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInAnimationProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "20s");
  cascade.Add("animation-duration", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("20s", cascade.ComputedValue("animation-duration"));

  cascade.Reset();
  cascade.Add(AnimationTaintedSet("--y", "20s"));
  cascade.Add("animation-duration", "var(--y)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--y"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, IndirectlyAnimationTainted) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "20s"));
  cascade.Add("--y", "var(--x)");
  cascade.Add("animation-duration", "var(--y)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("20s", cascade.ComputedValue("--y"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, AnimationTaintedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "20s"));
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

TEST_F(StyleCascadeTest, AnimationApplyFilter) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { color: white; background-color: white; }
        to { color: gray; background-color: gray; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation: test linear 10s -5s");
  cascade.Add("color:green");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("rgb(192, 192, 192)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, TransitionApplyFilter) {
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade1(GetDocument());
  cascade1.Add("background-color: white");
  cascade1.Add("color: white");
  cascade1.Add("transition: all steps(2, start) 100s");
  cascade1.Apply();

  // Set the old style on the element, so that the transition
  // update detects it.
  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with new color values.
  TestCascade cascade2(GetDocument());
  cascade2.Add("background-color: gray");
  cascade2.Add("color: gray");
  cascade2.Add("transition: all steps(2, start) 100s");
  cascade2.Apply();

  cascade2.AddInterpolations();
  cascade2.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("rgb(128, 128, 128)", cascade2.ComputedValue("color"));
  EXPECT_EQ("rgb(192, 192, 192)", cascade2.ComputedValue("background-color"));
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

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetPriority("--x").GetOrigin());
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

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetPriority("--x").GetOrigin());
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, TransitionCausesInterpolationValue) {
  UpdateAllLifecyclePhasesForTest();

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

  cascade2.AddInterpolations();
  cascade2.Apply();

  EXPECT_EQ(CascadeOrigin::kTransition,
            cascade2.GetPriority("--x").GetOrigin());
}

TEST_F(StyleCascadeTest, TransitionDetectedForChangedFontSize) {
  UpdateAllLifecyclePhasesForTest();

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

  cascade2.AddInterpolations();
  cascade2.Apply();

  EXPECT_EQ(CascadeOrigin::kTransition, cascade2.GetOrigin("--x"));
  EXPECT_EQ(CascadeOrigin::kTransition, cascade2.GetOrigin("width"));
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
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

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

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("width"));
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, AnimateLogicalProperty) {
  // We don't support smooth interpolation of css-logical properties yet,
  // so this test uses a paused animation at t=0.
  // TODO(crbug.com/865579): Support animations of css-logical properties

  AppendSheet(R"HTML(
     @keyframes test {
        from { margin-inline-start: 10px; }
        to { margin-inline-start: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("margin-left:1000px");
  cascade.Add("animation:test 1s linear paused");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-left"));
  EXPECT_EQ("10px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, AnimateLogicalPropertyWithLookup) {
  // We don't support smooth interpolation of css-logical properties yet,
  // so this test uses a paused animation at t=0.
  // TODO(crbug.com/865579): Support animations of css-logical properties

  AppendSheet(R"HTML(
     @keyframes test {
        from { margin-inline-start: 10px; }
        to { margin-inline-start: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("margin-left:1000px");
  cascade.Add("animation:test 1s linear paused");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.ApplySingle(GetCSSPropertyMarginLeft());

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-left"));
  EXPECT_EQ("10px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, AuthorImportantWinOverAnimations) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { width: 10px; height: 10px; }
        to { width: 20px; height: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Add("width:40px");
  cascade.Add("height:40px !important");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("width"));
  EXPECT_EQ(CascadeOrigin::kAuthor, cascade.GetOrigin("height"));

  EXPECT_EQ("15px", cascade.ComputedValue("width"));
  EXPECT_EQ("40px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, TransitionsWinOverAuthorImportant) {
  UpdateAllLifecyclePhasesForTest();

  // First, simulate an "old style".
  TestCascade cascade1(GetDocument());
  cascade1.Add("width:10px !important");
  cascade1.Add("height:10px !important");
  cascade1.Add("transition:all 1s");
  cascade1.Apply();

  // Set the old style on the element, so that the animation
  // update detects it.
  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with a new value for width/height.
  TestCascade cascade2(GetDocument());
  cascade2.Add("width:20px !important");
  cascade2.Add("height:20px !important");
  cascade2.Add("transition:all 1s");
  cascade2.Apply();

  cascade2.AddInterpolations();
  cascade2.Apply();

  EXPECT_EQ(CascadeOrigin::kTransition,
            cascade2.GetPriority("width").GetOrigin());
  EXPECT_EQ(CascadeOrigin::kTransition,
            cascade2.GetPriority("height").GetOrigin());
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
  cascade.Add("--x", "2em");
  cascade.Add("width", "10em");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

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
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

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

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-top"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-right"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-bottom"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-left"));

  EXPECT_EQ("15px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, AnimatedVisitedImportantOverride) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { background-color: rgb(100, 100, 100); }
        to { background-color: rgb(200, 200, 200); }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.State().StyleBuilder().SetInsideLink(EInsideLink::kInsideVisitedLink);

  cascade.Add(ParseDeclarationBlock("background-color:red !important"),
              {.link_match_type = CSSSelector::kMatchVisited});
  cascade.Add("animation-name:test");
  cascade.Add("animation-duration:10s");
  cascade.Add("animation-timing-function:linear");
  cascade.Add("animation-delay:-5s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();
  EXPECT_EQ("rgb(150, 150, 150)", cascade.ComputedValue("background-color"));

  const auto* style = cascade.TakeStyle();

  ComputedStyleBuilder builder(*style);
  builder.SetInsideLink(EInsideLink::kInsideVisitedLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color(255, 0, 0),
            style->VisitedDependentColor(GetCSSPropertyBackgroundColor()));

  builder = ComputedStyleBuilder(*style);
  builder.SetInsideLink(EInsideLink::kNotInsideLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color(150, 150, 150),
            style->VisitedDependentColor(GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleCascadeTest, AnimatedVisitedHighPrio) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { color: rgb(100, 100, 100); }
        to { color: rgb(200, 200, 200); }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.Add("color:red");
  cascade.Add("animation:test 10s -5s linear");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();
  EXPECT_EQ("rgb(150, 150, 150)", cascade.ComputedValue("color"));

  const auto* style = cascade.TakeStyle();

  ComputedStyleBuilder builder(*style);
  builder.SetInsideLink(EInsideLink::kInsideVisitedLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color(150, 150, 150),
            style->VisitedDependentColor(GetCSSPropertyColor()));

  builder = ComputedStyleBuilder(*style);
  builder.SetInsideLink(EInsideLink::kNotInsideLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color(150, 150, 150),
            style->VisitedDependentColor(GetCSSPropertyColor()));
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
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-top"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-right"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-bottom"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-left"));

  EXPECT_EQ("15px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ZoomCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Add("zoom:normal", CascadeOrigin::kUserAgent);
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomVsAll) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial");
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomReversedCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:normal", CascadeOrigin::kUserAgent);
  cascade.Add("zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Apply();

  EXPECT_EQ(2.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomImportant) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200% !important", CascadeOrigin::kUserAgent);
  cascade.Add("zoom:normal", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(2.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomExplicitDefault) {
  ScopedStandardizedBrowserZoomForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200%");
  cascade.Apply();

  // Since the zoom changed, there should be an explicit entry
  // in the cascade map with CascadeOrigin::kNone.
  CascadePriority* priority =
      cascade.FindPriority(CSSPropertyName(CSSPropertyID::kLineHeight));
  ASSERT_TRUE(priority);
  EXPECT_EQ(CascadeOrigin::kNone, priority->GetOrigin());
}

TEST_F(StyleCascadeTest, ZoomNoExplicitDefault) {
  ScopedStandardizedBrowserZoomForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Apply();

  // Since the zoom did not change, there should not be an entry in the map.
  CascadePriority* priority =
      cascade.FindPriority(CSSPropertyName(CSSPropertyID::kLineHeight));
  EXPECT_FALSE(priority);
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
  cascade.Add("writing-mode:vertical-lr !important", Origin::kAuthor);
  cascade.Add("-webkit-writing-mode:vertical-rl", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("vertical-lr", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-lr", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, RubyPositionCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("ruby-position", "over");
  cascade.Add("-webkit-ruby-position", "after");
  cascade.Apply();

  EXPECT_EQ("under", cascade.ComputedValue("ruby-position"));
  EXPECT_EQ("after", cascade.ComputedValue("-webkit-ruby-position"));
}

TEST_F(StyleCascadeTest, RubyPositionReverseCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-ruby-position", "after");
  cascade.Add("ruby-position", "over");
  cascade.Apply();

  EXPECT_EQ("over", cascade.ComputedValue("ruby-position"));
  EXPECT_EQ("before", cascade.ComputedValue("-webkit-ruby-position"));
}

TEST_F(StyleCascadeTest, RubyPositionSurrogateCanCascadeAsOriginal) {
  // Note: -webkit-ruby-position is defined as the surrogate, and ruby-position
  // is the original.
  ASSERT_FALSE(GetCSSPropertyRubyPosition().IsSurrogate());
  ASSERT_TRUE(GetCSSPropertyWebkitRubyPosition().IsSurrogate());

  const struct {
    CSSValueID specified;
    const char* webkit_expected;
    const char* unprefixed_expected;
  } tests[] = {
      {CSSValueID::kBefore, "before", "over"},
      {CSSValueID::kAfter, "after", "under"},
      {CSSValueID::kOver, "before", "over"},
      {CSSValueID::kUnder, "after", "under"},
  };

  for (const auto& test : tests) {
    TestCascade cascade(GetDocument());
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    set->SetProperty(CSSPropertyID::kWebkitRubyPosition,
                     *CSSIdentifierValue::Create(test.specified));
    cascade.Add(set);
    cascade.Apply();
    EXPECT_EQ(test.unprefixed_expected, cascade.ComputedValue("ruby-position"));
    EXPECT_EQ(test.webkit_expected,
              cascade.ComputedValue("-webkit-ruby-position"));
  }
}

TEST_F(StyleCascadeTest, TextOrientationPriority) {
  TestCascade cascade(GetDocument());
  cascade.Add("text-orientation:upright !important");
  cascade.Add("-webkit-text-orientation:sideways");
  cascade.Apply();

  EXPECT_EQ("upright", cascade.ComputedValue("text-orientation"));
  EXPECT_EQ("upright", cascade.ComputedValue("-webkit-text-orientation"));
}

TEST_F(StyleCascadeTest, TextOrientationRevert) {
  TestCascade cascade(GetDocument());
  cascade.Add("text-orientation:upright", CascadeOrigin::kUserAgent);
  cascade.Add("-webkit-text-orientation:mixed");
  cascade.Add("-webkit-text-orientation:revert");
  cascade.Apply();

  EXPECT_EQ("upright", cascade.ComputedValue("text-orientation"));
  EXPECT_EQ("upright", cascade.ComputedValue("-webkit-text-orientation"));
}

TEST_F(StyleCascadeTest, TextOrientationLegacyKeyword) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-text-orientation:vertical-right");
  cascade.Apply();

  EXPECT_EQ("mixed", cascade.ComputedValue("text-orientation"));
  EXPECT_EQ("vertical-right",
            cascade.ComputedValue("-webkit-text-orientation"));
}

TEST_F(StyleCascadeTest, WebkitBorderImageCascadeOrder) {
  String gradient1("linear-gradient(rgb(0, 0, 0), rgb(0, 128, 0))");
  String gradient2("linear-gradient(rgb(0, 0, 0), rgb(0, 200, 0))");

  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-border-image", gradient1 + " round 40 / 10px / 20px",
              Origin::kAuthor);
  cascade.Add("border-image-source", gradient2, Origin::kAuthor);
  cascade.Add("border-image-slice", "20", Origin::kAuthor);
  cascade.Add("border-image-width", "6px", Origin::kAuthor);
  cascade.Add("border-image-outset", "4px", Origin::kAuthor);
  cascade.Add("border-image-repeat", "space", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(gradient2, cascade.ComputedValue("border-image-source"));
  EXPECT_EQ("20", cascade.ComputedValue("border-image-slice"));
  EXPECT_EQ("6px", cascade.ComputedValue("border-image-width"));
  EXPECT_EQ("4px", cascade.ComputedValue("border-image-outset"));
  EXPECT_EQ("space", cascade.ComputedValue("border-image-repeat"));
}

TEST_F(StyleCascadeTest, WebkitBorderImageReverseCascadeOrder) {
  String gradient1("linear-gradient(rgb(0, 0, 0), rgb(0, 128, 0))");
  String gradient2("linear-gradient(rgb(0, 0, 0), rgb(0, 200, 0))");

  TestCascade cascade(GetDocument());
  cascade.Add("border-image-source", gradient2, Origin::kAuthor);
  cascade.Add("border-image-slice", "20", Origin::kAuthor);
  cascade.Add("border-image-width", "6px", Origin::kAuthor);
  cascade.Add("border-image-outset", "4px", Origin::kAuthor);
  cascade.Add("border-image-repeat", "space", Origin::kAuthor);
  cascade.Add("-webkit-border-image", gradient1 + " round 40 / 10px / 20px",
              Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(gradient1, cascade.ComputedValue("border-image-source"));
  EXPECT_EQ("40 fill", cascade.ComputedValue("border-image-slice"));
  EXPECT_EQ("10px", cascade.ComputedValue("border-image-width"));
  EXPECT_EQ("20px", cascade.ComputedValue("border-image-outset"));
  EXPECT_EQ("round", cascade.ComputedValue("border-image-repeat"));
}

TEST_F(StyleCascadeTest, WebkitBorderImageMixedOrder) {
  String gradient1("linear-gradient(rgb(0, 0, 0), rgb(0, 128, 0))");
  String gradient2("linear-gradient(rgb(0, 0, 0), rgb(0, 200, 0))");

  TestCascade cascade(GetDocument());
  cascade.Add("border-image-source", gradient2, Origin::kAuthor);
  cascade.Add("border-image-width", "6px", Origin::kAuthor);
  cascade.Add("-webkit-border-image", gradient1 + " round 40 / 10px / 20px",
              Origin::kAuthor);
  cascade.Add("border-image-slice", "20", Origin::kAuthor);
  cascade.Add("border-image-outset", "4px", Origin::kAuthor);
  cascade.Add("border-image-repeat", "space", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(gradient1, cascade.ComputedValue("border-image-source"));
  EXPECT_EQ("20", cascade.ComputedValue("border-image-slice"));
  EXPECT_EQ("10px", cascade.ComputedValue("border-image-width"));
  EXPECT_EQ("4px", cascade.ComputedValue("border-image-outset"));
  EXPECT_EQ("space", cascade.ComputedValue("border-image-repeat"));
}

TEST_F(StyleCascadeTest, WebkitPerspectiveOriginCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-perspective-origin-x:10px");
  cascade.Add("-webkit-perspective-origin-y:20px");
  cascade.Add("perspective-origin:30px 40px");
  cascade.Apply();

  EXPECT_EQ("30px 40px", cascade.ComputedValue("perspective-origin"));

  // The -webkit-perspective-origin-x/y properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-y"));
}

TEST_F(StyleCascadeTest, WebkitPerspectiveOriginReverseCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("perspective-origin:30px 40px");
  cascade.Add("-webkit-perspective-origin-x:10px");
  cascade.Add("-webkit-perspective-origin-y:20px");
  cascade.Apply();

  EXPECT_EQ("10px 20px", cascade.ComputedValue("perspective-origin"));

  // The -webkit-perspective-origin-x/y properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-y"));
}

TEST_F(StyleCascadeTest, WebkitPerspectiveOriginMixedCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-perspective-origin-x:10px");
  cascade.Add("perspective-origin:30px 40px");
  cascade.Add("-webkit-perspective-origin-y:20px");
  cascade.Apply();

  EXPECT_EQ("30px 20px", cascade.ComputedValue("perspective-origin"));

  // The -webkit-perspective-origin-x/y properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-y"));
}

TEST_F(StyleCascadeTest, WebkitPerspectiveOriginRevert) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-perspective-origin-x:10px");
  cascade.Add("perspective-origin:30px 40px");
  cascade.Add("-webkit-perspective-origin-y:20px");
  cascade.Apply();

  EXPECT_EQ("30px 20px", cascade.ComputedValue("perspective-origin"));

  // The -webkit-perspective-origin-x/y properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-perspective-origin-y"));
}

TEST_F(StyleCascadeTest, WebkitTransformOriginCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-transform-origin-x:10px");
  cascade.Add("-webkit-transform-origin-y:20px");
  cascade.Add("-webkit-transform-origin-z:30px");
  cascade.Add("transform-origin:40px 50px 60px");
  cascade.Apply();

  EXPECT_EQ("40px 50px 60px", cascade.ComputedValue("transform-origin"));

  // The -webkit-transform-origin-x/y/z properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-y"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-z"));
}

TEST_F(StyleCascadeTest, WebkitTransformOriginReverseCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("transform-origin:40px 50px 60px");
  cascade.Add("-webkit-transform-origin-x:10px");
  cascade.Add("-webkit-transform-origin-y:20px");
  cascade.Add("-webkit-transform-origin-z:30px");
  cascade.Apply();

  EXPECT_EQ("10px 20px 30px", cascade.ComputedValue("transform-origin"));

  // The -webkit-transform-origin-x/y/z properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-y"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-z"));
}

TEST_F(StyleCascadeTest, WebkitTransformOriginMixedCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-transform-origin-x:10px");
  cascade.Add("transform-origin:40px 50px 60px");
  cascade.Add("-webkit-transform-origin-y:20px");
  cascade.Add("-webkit-transform-origin-z:30px");
  cascade.Apply();

  EXPECT_EQ("40px 20px 30px", cascade.ComputedValue("transform-origin"));

  // The -webkit-transform-origin-x/y/z properties are not "computable".
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-x"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-y"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("-webkit-transform-origin-z"));
}

TEST_F(StyleCascadeTest, VerticalAlignBaselineSource) {
  TestCascade cascade(GetDocument());
  cascade.Add("vertical-align", "top");
  cascade.Add("baseline-source", "first");
  cascade.Apply();

  EXPECT_EQ("top", cascade.ComputedValue("vertical-align"));
  EXPECT_EQ("first", cascade.ComputedValue("baseline-source"));
}

TEST_F(StyleCascadeTest, VerticalAlignBaselineSourceReversed) {
  TestCascade cascade(GetDocument());
  cascade.Add("baseline-source", "first");
  cascade.Add("vertical-align", "top");
  cascade.Apply();

  EXPECT_EQ("top", cascade.ComputedValue("vertical-align"));
  EXPECT_EQ("auto", cascade.ComputedValue("baseline-source"));
}

TEST_F(StyleCascadeTest, WebkitBoxDecorationBreakOverlap) {
  ScopedBoxDecorationBreakForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-box-decoration-break", "slice");
  cascade.Add("box-decoration-break", "clone");
  cascade.Apply();

  EXPECT_EQ("clone", cascade.ComputedValue("box-decoration-break"));
  EXPECT_EQ("clone", cascade.ComputedValue("-webkit-box-decoration-break"));
}

TEST_F(StyleCascadeTest, WebkitBoxDecorationBreakOverlapReverse) {
  ScopedBoxDecorationBreakForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Add("box-decoration-break", "slice");
  cascade.Add("-webkit-box-decoration-break", "clone");
  cascade.Apply();

  EXPECT_EQ("clone", cascade.ComputedValue("box-decoration-break"));
  EXPECT_EQ("clone", cascade.ComputedValue("-webkit-box-decoration-break"));
}

TEST_F(StyleCascadeTest, InitialDirection) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin-inline-start:10px");
  cascade.Add("margin-inline-end:20px");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("margin-left"));
  EXPECT_EQ("20px", cascade.ComputedValue("margin-right"));
}

TEST_F(StyleCascadeTest, NonInitialDirection) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin-inline-start:10px");
  cascade.Add("margin-inline-end:20px");
  cascade.Add("direction:rtl");
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("margin-left"));
  EXPECT_EQ("10px", cascade.ComputedValue("margin-right"));
}

TEST_F(StyleCascadeTest, InitialWritingMode) {
  TestCascade cascade(GetDocument());
  cascade.Add("inline-size:10px");
  cascade.Add("block-size:20px");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("20px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, NonInitialWritingMode) {
  TestCascade cascade(GetDocument());
  cascade.Add("inline-size:10px");
  cascade.Add("block-size:20px");
  cascade.Add("writing-mode:vertical-lr");
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("width"));
  EXPECT_EQ("10px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, InitialTextSizeAdjust) {
  GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
  ScopedTextSizeAdjustImprovementsForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size:10px");
  cascade.Add("line-height:20px");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("font-size"));
  EXPECT_EQ("20px", cascade.ComputedValue("line-height"));
}

TEST_F(StyleCascadeTest, NonInitialTextSizeAdjust) {
  GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
  ScopedTextSizeAdjustImprovementsForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size:10px");
  cascade.Add("line-height:20px");
  cascade.Add("text-size-adjust:200%");
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("font-size"));
  EXPECT_EQ("40px", cascade.ComputedValue("line-height"));
}

TEST_F(StyleCascadeTest, DoesNotDependOnCascadeAffectingProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:10px");
  cascade.Add("height:20px");
  cascade.Apply();

  EXPECT_FALSE(cascade.DependsOnCascadeAffectingProperty());
  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("20px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, DependsOnCascadeAffectingProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("inline-size:10px");
  cascade.Add("height:20px");
  cascade.Apply();

  EXPECT_TRUE(cascade.DependsOnCascadeAffectingProperty());
  EXPECT_EQ("10px", cascade.ComputedValue("width"));
  EXPECT_EQ("20px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, ResetDependsOnCascadeAffectingPropertyFlag) {
  TestCascade cascade(GetDocument());
  cascade.Add("inline-size:10px");
  cascade.Add("height:20px");
  cascade.Apply();

  EXPECT_TRUE(cascade.DependsOnCascadeAffectingProperty());
  cascade.Reset();
  EXPECT_FALSE(cascade.DependsOnCascadeAffectingProperty());
}

TEST_F(StyleCascadeTest, MarkReferenced) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("width", "var(--x)");
  cascade.Apply();

  const auto& registry = GetDocument().EnsurePropertyRegistry();

  EXPECT_TRUE(registry.WasReferenced(AtomicString("--x")));
  EXPECT_FALSE(registry.WasReferenced(AtomicString("--y")));
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceLonghand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1px");
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1px");
  cascade.Add("margin", "var(--x)");
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceLonghandMissingVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceShorthandMissingVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "var(--x)");
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, NoMarkHasVariableReferenceInherited) {
  TestCascade cascade(GetDocument());
  cascade.Add("color", "var(--x)");
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, NoMarkHasVariableReferenceWithoutVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "1px");
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, InternalVisitedColorLonghand) {
  TestCascade cascade(GetDocument());
  cascade.Add("color:green");
  cascade.Add("color:red", {.origin = CascadeOrigin::kAuthor,
                            .link_match_type = CSSSelector::kMatchVisited});

  cascade.State().StyleBuilder().SetInsideLink(EInsideLink::kInsideVisitedLink);
  cascade.Apply();

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));

  Color red(255, 0, 0);
  const css_longhand::Color& color = GetCSSPropertyColor();
  EXPECT_EQ(red, cascade.TakeStyle()->VisitedDependentColor(color));
}

TEST_F(StyleCascadeTest, VarInInternalVisitedColorShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:red");
  cascade.Add("outline:medium solid var(--x)",
              {.link_match_type = CSSSelector::kMatchVisited});
  cascade.Add("outline-color:green",
              {.link_match_type = CSSSelector::kMatchLink});

  cascade.State().StyleBuilder().SetInsideLink(EInsideLink::kInsideVisitedLink);
  cascade.Apply();

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("outline-color"));

  Color red(255, 0, 0);
  const css_longhand::OutlineColor& outline_color =
      GetCSSPropertyOutlineColor();
  EXPECT_EQ(red, cascade.TakeStyle()->VisitedDependentColor(outline_color));
}

TEST_F(StyleCascadeTest, ApplyWithFilter) {
  TestCascade cascade(GetDocument());
  cascade.Add("color", "blue", Origin::kAuthor);
  cascade.Add("background-color", "green", Origin::kAuthor);
  cascade.Add("display", "inline", Origin::kAuthor);
  cascade.Apply();

  cascade.Reset();
  cascade.Add("color", "green", Origin::kAuthor);
  cascade.Add("background-color", "red", Origin::kAuthor);
  cascade.Add("display", "block", Origin::kAuthor);
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, false));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("background-color"));
  EXPECT_EQ("inline", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, FilterWebkitBorderImage) {
  TestCascade cascade(GetDocument());
  cascade.Add("border-image:linear-gradient(green, red) 1 / 2 / 3 round",
              Origin::kAuthor);
  cascade.Add(
      "-webkit-border-image:linear-gradient(green, red) 4 / 5 / 6 round",
      Origin::kAuthor);
  cascade.Apply(CascadeFilter(CSSProperty::kLegacyOverlapping, true));
  EXPECT_EQ("linear-gradient(rgb(0, 128, 0), rgb(255, 0, 0)) 1 / 2 / 3 round",
            cascade.ComputedValue("-webkit-border-image"));
}

TEST_F(StyleCascadeTest, FilterPerspectiveOrigin) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-perspective-origin-x:10px");
  cascade.Add("-webkit-perspective-origin-y:20px");
  cascade.Add("perspective-origin:30px 40px");
  cascade.Apply(CascadeFilter(CSSProperty::kLegacyOverlapping, false));
  EXPECT_EQ("10px 20px", cascade.ComputedValue("perspective-origin"));
}

TEST_F(StyleCascadeTest, FilterTransformOrigin) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-transform-origin-x:10px");
  cascade.Add("-webkit-transform-origin-y:20px");
  cascade.Add("-webkit-transform-origin-z:30px");
  cascade.Add("transform-origin:40px 50px 60px");
  cascade.Apply(CascadeFilter(CSSProperty::kLegacyOverlapping, false));
  EXPECT_EQ("10px 20px 30px", cascade.ComputedValue("transform-origin"));
}

TEST_F(StyleCascadeTest, HasAuthorBackground) {
  Vector<String> properties = {"background-attachment", "background-clip",
                               "background-image",      "background-origin",
                               "background-position-x", "background-position-y",
                               "background-size"};

  for (String property : properties) {
    TestCascade cascade(GetDocument());
    cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
    cascade.Add(property, "unset", Origin::kAuthor);
    cascade.Apply();
    EXPECT_TRUE(cascade.TakeStyle()->HasAuthorBackground());
  }
}

TEST_F(StyleCascadeTest, HasAuthorBorder) {
  Vector<String> properties = {
      "border-top-color",          "border-right-color",
      "border-bottom-color",       "border-left-color",
      "border-top-style",          "border-right-style",
      "border-bottom-style",       "border-left-style",
      "border-top-width",          "border-right-width",
      "border-bottom-width",       "border-left-width",
      "border-top-left-radius",    "border-top-right-radius",
      "border-bottom-left-radius", "border-bottom-right-radius",
      "border-image-source",       "border-image-slice",
      "border-image-width",        "border-image-outset",
      "border-image-repeat"};

  for (String property : properties) {
    TestCascade cascade(GetDocument());
    cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
    cascade.Add(property, "unset", Origin::kAuthor);
    cascade.Apply();
    EXPECT_TRUE(cascade.TakeStyle()->HasAuthorBorder());
  }
}

TEST_F(StyleCascadeTest, HasAuthorBorderLogical) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
  cascade.Add("border-block-start-color", "red", Origin::kUserAgent);
  cascade.Add("border-block-start-color", "green", Origin::kAuthor);
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasAuthorBorder());
}

TEST_F(StyleCascadeTest, NoAuthorBackgroundOrBorder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
  cascade.Add("background-color", "red", Origin::kUserAgent);
  cascade.Add("border-left-color", "green", Origin::kUserAgent);
  cascade.Add("background-clip", "padding-box", Origin::kUser);
  cascade.Add("border-right-color", "green", Origin::kUser);
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasAuthorBackground());
  EXPECT_FALSE(style->HasAuthorBorder());
}

TEST_F(StyleCascadeTest, AuthorBackgroundRevert) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
  cascade.Add("background-color", "red", Origin::kUserAgent);
  cascade.Add("background-color", "revert", Origin::kAuthor);
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasAuthorBackground());
}

TEST_F(StyleCascadeTest, AuthorBorderRevert) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
  cascade.Add("border-top-color", "red", Origin::kUserAgent);
  cascade.Add("border-top-color", "revert", Origin::kAuthor);
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasAuthorBorder());
}

TEST_F(StyleCascadeTest, AuthorBorderRevertLogical) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
  cascade.Add("border-block-start-color", "red", Origin::kUserAgent);
  cascade.Add("border-block-start-color", "revert", Origin::kAuthor);
  cascade.Apply();
  const auto* style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasAuthorBorder());
}

TEST_F(StyleCascadeTest, AnalyzeMatchResult) {
  auto ua = CascadeOrigin::kUserAgent;
  auto author = CascadeOrigin::kAuthor;

  TestCascade cascade(GetDocument());
  cascade.Add("display:none;left:5px", ua);
  cascade.Add("font-size:1px !important", ua);
  cascade.Add("display:block;color:red", author);
  cascade.Add("font-size:3px", author);
  cascade.Apply();

  EXPECT_EQ(cascade.GetPriority("display").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("left").GetOrigin(), ua);
  EXPECT_EQ(cascade.GetPriority("color").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("font-size").GetOrigin(), ua);
}

TEST_F(StyleCascadeTest, AnalyzeMatchResultAll) {
  auto ua = CascadeOrigin::kUserAgent;
  auto author = CascadeOrigin::kAuthor;

  TestCascade cascade(GetDocument());
  cascade.Add("display:block", ua);
  cascade.Add("font-size:1px !important", ua);
  cascade.Add("all:unset", author);
  cascade.Apply();

  EXPECT_EQ(cascade.GetPriority("display").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("font-size").GetOrigin(), ua);

  // Random sample from another property affected by 'all'.
  EXPECT_EQ(cascade.GetPriority("color").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("color"), cascade.GetPriority("display"));
}

TEST_F(StyleCascadeTest, AnalyzeFlagsClean) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { top: 0px; }
        to { top: 10px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("bottom:10px");
  cascade.Add("animation:test linear 1000s -500s");
  cascade.Apply();
  EXPECT_FALSE(cascade.NeedsMatchResultAnalyze());
  EXPECT_FALSE(cascade.NeedsInterpolationsAnalyze());

  cascade.AddInterpolations();
  cascade.Apply();
  EXPECT_FALSE(cascade.NeedsMatchResultAnalyze());
  EXPECT_FALSE(cascade.NeedsInterpolationsAnalyze());
}

TEST_F(StyleCascadeTest, ApplyMatchResultFilter) {
  TestCascade cascade(GetDocument());
  cascade.Add("display:block");
  cascade.Add("color:green");
  cascade.Add("font-size:3px");
  cascade.Apply();

  cascade.Reset();
  cascade.Add("display:inline");
  cascade.Add("color:red");
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("inline", cascade.ComputedValue("display"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("3px", cascade.ComputedValue("font-size"));
}

TEST_F(StyleCascadeTest, ApplyMatchResultAllFilter) {
  TestCascade cascade(GetDocument());
  cascade.Add("color:green");
  cascade.Add("display:block");
  cascade.Apply();

  cascade.Reset();
  cascade.Add("all:unset");
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("inline", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, MarkHasReferenceLonghand) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:red");
  cascade.Add("background-color:var(--x)");
  cascade.Apply();

  EXPECT_TRUE(cascade.State()
                  .StyleBuilder()
                  .HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasReferenceShorthand) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:red");
  cascade.Add("background:var(--x)");
  cascade.Apply();

  EXPECT_TRUE(cascade.State()
                  .StyleBuilder()
                  .HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, NoMarkHasReferenceForInherited) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:red");
  cascade.Add("--y:caption");
  cascade.Add("color:var(--x)");
  cascade.Add("font:var(--y)");
  cascade.Apply();

  EXPECT_FALSE(cascade.State()
                   .StyleBuilder()
                   .HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, Reset) {
  TestCascade cascade(GetDocument());

  EXPECT_EQ(CascadePriority(), cascade.GetPriority("color"));
  EXPECT_EQ(CascadePriority(), cascade.GetPriority("--x"));

  cascade.Add("color:red");
  cascade.Add("--x:red");
  cascade.Apply();  // generation=1
  cascade.Apply();  // generation=2

  EXPECT_EQ(2u, cascade.GetPriority("color").GetGeneration());
  EXPECT_EQ(2u, cascade.GetPriority("--x").GetGeneration());

  cascade.Reset();

  EXPECT_EQ(CascadePriority(), cascade.GetPriority("color"));
  EXPECT_EQ(CascadePriority(), cascade.GetPriority("--x"));
}

TEST_F(StyleCascadeTest, GetImportantSetEmpty) {
  TestCascade cascade(GetDocument());
  cascade.Add("color:red");
  cascade.Add("width:1px");
  cascade.Add("--x:green");
  EXPECT_FALSE(cascade.GetImportantSet());
}

TEST_F(StyleCascadeTest, GetImportantSetSingle) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px !important");
  ASSERT_TRUE(cascade.GetImportantSet());
  EXPECT_EQ(CSSBitset({CSSPropertyID::kWidth}), *cascade.GetImportantSet());
}

TEST_F(StyleCascadeTest, GetImportantSetMany) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px !important");
  cascade.Add("height:1px !important");
  cascade.Add("top:1px !important");
  ASSERT_TRUE(cascade.GetImportantSet());
  EXPECT_EQ(CSSBitset({CSSPropertyID::kWidth, CSSPropertyID::kHeight,
                       CSSPropertyID::kTop}),
            *cascade.GetImportantSet());
}

TEST_F(StyleCascadeTest, RootColorNotModifiedByEmptyCascade) {
  TestCascade cascade(GetDocument(), GetDocument().documentElement());
  cascade.Add("color:red");
  cascade.Apply();

  cascade.Reset();
  cascade.Add("display:block");
  cascade.Apply();  // Should not affect 'color'.

  const auto* style = cascade.TakeStyle();

  ComputedStyleBuilder builder(*style);
  builder.SetInsideLink(EInsideLink::kInsideVisitedLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color(255, 0, 0),
            style->VisitedDependentColor(GetCSSPropertyColor()));

  builder = ComputedStyleBuilder(*style);
  builder.SetInsideLink(EInsideLink::kNotInsideLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color(255, 0, 0),
            style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleCascadeTest, InitialColor) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  TestCascade cascade(GetDocument(), GetDocument().documentElement());
  cascade.Add("color-scheme:dark");

  // CSSInitialColorValue is not reachable via a string, hence we must
  // create the CSSPropertyValueSet that contains it manually.
  auto* set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  set->SetProperty(CSSPropertyID::kColor, *CSSInitialColorValue::Create());
  cascade.Add(set);

  cascade.Apply();

  const auto* style = cascade.TakeStyle();

  ComputedStyleBuilder builder(*style);
  builder.SetInsideLink(EInsideLink::kInsideVisitedLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color::kWhite, style->VisitedDependentColor(GetCSSPropertyColor()));

  builder = ComputedStyleBuilder(*style);
  builder.SetInsideLink(EInsideLink::kNotInsideLink);
  style = builder.TakeStyle();
  EXPECT_EQ(Color::kWhite, style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleCascadeTest, MaxVariableBytes) {
  StringBuilder builder;
  for (size_t i = 0; i < CSSVariableData::kMaxVariableBytes; ++i) {
    builder.Append(':');  // <colon-token>
  }

  String at_limit = builder.ToString();
  String above_limit = builder.ToString() + ":";

  TestCascade cascade(GetDocument());
  cascade.Add("--at-limit", at_limit);
  cascade.Add("--above-limit", above_limit);
  cascade.Add("--at-limit-reference", "var(--at-limit)");
  cascade.Add("--above-limit-reference", "var(--above-limit)");
  cascade.Add("--at-limit-reference-fallback",
              "var(--unknown,var(--at-limit))");
  cascade.Add("--above-limit-reference-fallback",
              "var(--unknown,var(--above-limit))");
  cascade.Apply();

  EXPECT_EQ(at_limit, cascade.ComputedValue("--at-limit"));
  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--above-limit"));
  EXPECT_EQ(at_limit, cascade.ComputedValue("--at-limit-reference"));
  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--above-limit-reference"));
  EXPECT_EQ(at_limit, cascade.ComputedValue("--at-limit-reference-fallback"));
  EXPECT_EQ(g_null_atom,
            cascade.ComputedValue("--above-limit-reference-fallback"));
}

TEST_F(StyleCascadeTest, UnicodeEscapeInCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "\"\\65e5\\672c\"");
  cascade.Add("content", "var(--a)");
  cascade.Apply();

  EXPECT_EQ(String::FromUTF8("\"日本\""), cascade.ComputedValue("content"));
}

TEST_F(StyleCascadeTest, GetCascadedValues) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:1px", CascadeOrigin::kUserAgent);
  cascade.Add("right:2px", CascadeOrigin::kUserAgent);
  cascade.Add("bottom:3px", CascadeOrigin::kUserAgent);
  cascade.Add("left:4px !important", CascadeOrigin::kUserAgent);
  cascade.Add("width:5px", CascadeOrigin::kUserAgent);

  cascade.Add("top:10px", CascadeOrigin::kUser);
  cascade.Add("right:20px", CascadeOrigin::kUser);
  cascade.Add("bottom:30px !important", CascadeOrigin::kUser);
  cascade.Add("left:40px", CascadeOrigin::kUser);
  cascade.Add("height:60px", CascadeOrigin::kUser);
  cascade.Add("height:61px", CascadeOrigin::kUser);
  cascade.Add("--x:70px", CascadeOrigin::kUser);
  cascade.Add("--y:80px !important", CascadeOrigin::kUser);

  cascade.Add("top:100px", CascadeOrigin::kAuthor);
  cascade.Add("right:201px !important", CascadeOrigin::kAuthor);
  cascade.Add("right:200px", CascadeOrigin::kAuthor);
  cascade.Add("bottom:300px", CascadeOrigin::kAuthor);
  cascade.Add("left:400px", CascadeOrigin::kAuthor);
  cascade.Add("--x:700px", CascadeOrigin::kAuthor);
  cascade.Add("--y:800px", CascadeOrigin::kAuthor);

  cascade.Apply();

  auto map = cascade.GetCascadedValues();
  EXPECT_EQ(8u, map.size());

  EXPECT_EQ("100px", CssTextAt(map, "top"));
  EXPECT_EQ("201px", CssTextAt(map, "right"));
  EXPECT_EQ("30px", CssTextAt(map, "bottom"));
  EXPECT_EQ("4px", CssTextAt(map, "left"));
  EXPECT_EQ("5px", CssTextAt(map, "width"));
  EXPECT_EQ("61px", CssTextAt(map, "height"));
  EXPECT_EQ("700px", CssTextAt(map, "--x"));
  EXPECT_EQ("80px", CssTextAt(map, "--y"));
}

TEST_F(StyleCascadeTest, GetCascadedValuesCssWide) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:initial");
  cascade.Add("right:inherit");
  cascade.Add("bottom:unset");
  cascade.Add("left:revert");
  cascade.Apply();

  auto map = cascade.GetCascadedValues();
  EXPECT_EQ(4u, map.size());

  EXPECT_EQ("initial", CssTextAt(map, "top"));
  EXPECT_EQ("inherit", CssTextAt(map, "right"));
  EXPECT_EQ("unset", CssTextAt(map, "bottom"));
  EXPECT_EQ("revert", CssTextAt(map, "left"));
}

TEST_F(StyleCascadeTest, GetCascadedValuesLogical) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin-inline-start:1px");
  cascade.Add("margin-inline-end:2px");
  cascade.Apply();

  auto map = cascade.GetCascadedValues();
  EXPECT_EQ(2u, map.size());

  EXPECT_EQ("1px", CssTextAt(map, "margin-left"));
  EXPECT_EQ("2px", CssTextAt(map, "margin-right"));
}

TEST_F(StyleCascadeTest, GetCascadedValuesInterpolated) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 100px; width: 100px; }
        to { --x: 200px; width: 200px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name: test");
  cascade.Add("animation-timing-function: linear");
  cascade.Add("animation-duration: 10s");
  cascade.Add("animation-delay: -5s");
  cascade.Apply();

  cascade.AddInterpolations();
  cascade.Apply();

  // Verify that effect values from the animation did apply:
  EXPECT_EQ("200px", cascade.ComputedValue("--x"));
  EXPECT_EQ("150px", cascade.ComputedValue("width"));

  // However, we don't currently support returning interpolated vales from
  // GetCascadedValues:
  auto map = cascade.GetCascadedValues();
  EXPECT_EQ(4u, map.size());

  EXPECT_EQ("test", CssTextAt(map, "animation-name"));
  EXPECT_EQ("linear", CssTextAt(map, "animation-timing-function"));
  EXPECT_EQ("10s", CssTextAt(map, "animation-duration"));
  EXPECT_EQ("-5s", CssTextAt(map, "animation-delay"));
}

TEST_F(StyleCascadeTest, GetCascadedValuesWithExplicitDefaults) {
  ScopedStandardizedBrowserZoomForTest scoped_feature(true);

  TestCascade cascade(GetDocument());
  cascade.Add("top:100px");
  cascade.Add("zoom:200%");  // Causes explicit defaults.
  cascade.Apply();

  // Any explicit defaults (StyleCascade::AddExplicitDefaults) should not
  // be visible via GetCascadedValues.

  auto map = cascade.GetCascadedValues();
  EXPECT_EQ(2u, map.size());

  EXPECT_EQ("100px", CssTextAt(map, "top"));
  EXPECT_EQ("200%", CssTextAt(map, "zoom"));
}

TEST_F(StyleCascadeTest, StaticResolveNoVar) {
  // We don't need this object, but it's an easy way of setting
  // up a StyleResolverState.
  TestCascade cascade(GetDocument());

  EXPECT_EQ("thing", CssText(TestCascade::StaticResolve(cascade.State(), "--x",
                                                        "thing")));
  EXPECT_EQ("red", CssText(TestCascade::StaticResolve(cascade.State(), "color",
                                                      "red")));
  EXPECT_EQ("10px", CssText(TestCascade::StaticResolve(cascade.State(), "width",
                                                       "10px")));
  EXPECT_EQ("10em", CssText(TestCascade::StaticResolve(cascade.State(), "width",
                                                       "10em")));
  EXPECT_EQ("calc(1% + 1em)", CssText(TestCascade::StaticResolve(
                                  cascade.State(), "width", "calc(1% + 1em)")));
}

TEST_F(StyleCascadeTest, StaticResolveVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:foo");
  cascade.Apply();

  EXPECT_EQ("foo", CssText(TestCascade::StaticResolve(cascade.State(), "--y",
                                                      "var(--x)")));
  EXPECT_EQ("foo bar", CssText(TestCascade::StaticResolve(
                           cascade.State(), "--y", "var(--x) bar")));
  EXPECT_EQ("bar", CssText(TestCascade::StaticResolve(cascade.State(), "--y",
                                                      "var(--unknown,bar)")));
  EXPECT_EQ("unset", CssText(TestCascade::StaticResolve(cascade.State(), "--y",
                                                        "var(--unknown)")));
}

TEST_F(StyleCascadeTest, StaticResolveRegisteredVar) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x:100px");
  cascade.Apply();

  EXPECT_EQ("100px", CssText(TestCascade::StaticResolve(cascade.State(), "--y",
                                                        "var(--x)")));
  EXPECT_EQ("100px", CssText(TestCascade::StaticResolve(cascade.State(), "--z",
                                                        "var(--x)")));

  EXPECT_EQ("50px", CssText(TestCascade::StaticResolve(
                        cascade.State(), "--y", "var(--unknown, 50px)")));
  EXPECT_EQ("50px", CssText(TestCascade::StaticResolve(
                        cascade.State(), "--z", "var(--unknown, 50px)")));

  EXPECT_EQ("unset", CssText(TestCascade::StaticResolve(cascade.State(), "--y",
                                                        "var(--unknown)")));
  EXPECT_EQ("unset", CssText(TestCascade::StaticResolve(cascade.State(), "--z",
                                                        "var(--unknown)")));

  // StyleCacade::Resolve does not actually compute values, just eliminate
  // var() references.
  EXPECT_EQ("calc(5em + 100px)",
            CssText(TestCascade::StaticResolve(cascade.State(), "--y",
                                               "calc(5em + var(--x))")));
  EXPECT_EQ("calc(5em + 100px)",
            CssText(TestCascade::StaticResolve(cascade.State(), "--z",
                                               "calc(5em + var(--x))")));
}

TEST_F(StyleCascadeTest, RevertOrigin) {
  TestCascade cascade(GetDocument());

  cascade.Add("width", "1px", CascadeOrigin::kUserAgent);
  cascade.Add("height", "1px", CascadeOrigin::kUserAgent);
  cascade.Add("display", "block", CascadeOrigin::kUserAgent);
  cascade.Add("width", "2px", CascadeOrigin::kUser);
  cascade.Add("height", "revert", CascadeOrigin::kUser);
  cascade.Add("width", "revert", CascadeOrigin::kAuthor);
  cascade.Add("height", "revert", CascadeOrigin::kAuthor);
  cascade.Add("display", "revert", CascadeOrigin::kAuthor);
  cascade.Add("margin-left", "revert", CascadeOrigin::kAuthor);

  cascade.AnalyzeIfNeeded();

  CSSValue* revert_value = cssvalue::CSSRevertValue::Create();

  TestCascadeResolver resolver;

  CascadeOrigin origin = CascadeOrigin::kAuthor;
  const CSSValue* resolved_value =
      cascade.Resolve(GetCSSPropertyWidth(), *revert_value, origin);
  ASSERT_TRUE(resolved_value);
  EXPECT_EQ(CascadeOrigin::kUser, origin);
  EXPECT_EQ("2px", resolved_value->CssText());

  origin = CascadeOrigin::kAuthor;
  resolved_value =
      cascade.Resolve(GetCSSPropertyHeight(), *revert_value, origin);
  ASSERT_TRUE(resolved_value);
  EXPECT_EQ(CascadeOrigin::kUserAgent, origin);
  EXPECT_EQ("1px", resolved_value->CssText());

  origin = CascadeOrigin::kAuthor;
  resolved_value =
      cascade.Resolve(GetCSSPropertyDisplay(), *revert_value, origin);
  ASSERT_TRUE(resolved_value);
  EXPECT_EQ(CascadeOrigin::kUserAgent, origin);
  EXPECT_EQ("block", resolved_value->CssText());

  origin = CascadeOrigin::kAuthor;
  resolved_value =
      cascade.Resolve(GetCSSPropertyMarginLeft(), *revert_value, origin);
  ASSERT_TRUE(resolved_value);
  EXPECT_EQ(CascadeOrigin::kNone, origin);
  EXPECT_EQ("unset", resolved_value->CssText());
}

TEST_F(StyleCascadeTest, FlipRevertValue_Swap) {
  TestCascade cascade(GetDocument());

  cascade.Add("left:1px", {.layer_order = 1});
  cascade.Add("right:2px", {.layer_order = 1});
  cascade.Add("top:3px", {.layer_order = 1});
  cascade.Add("bottom:4px", {.layer_order = 1});

  // Revert left to right, and vice-versa.
  cascade.Add(FlipRevertSet("left", "right"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("right", "left"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("top", "bottom"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("bottom", "top"), {.layer_order = 2});

  cascade.Apply();

  EXPECT_EQ("2px", cascade.ComputedValue("left"));
  EXPECT_EQ("1px", cascade.ComputedValue("right"));
  EXPECT_EQ("4px", cascade.ComputedValue("top"));
  EXPECT_EQ("3px", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, FlipRevertValue_Chain) {
  TestCascade cascade(GetDocument());

  cascade.Add("left:1px", {.layer_order = 1});
  cascade.Add("right:2px", {.layer_order = 1});
  cascade.Add("top:3px", {.layer_order = 1});
  cascade.Add("bottom:4px", {.layer_order = 1});

  cascade.Add(FlipRevertSet("right", "left"), {.layer_order = 2});

  cascade.Add(FlipRevertSet("top", "right"), {.layer_order = 3});

  cascade.Add(FlipRevertSet("bottom", "top"), {.layer_order = 4});

  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("left"));
  EXPECT_EQ("1px", cascade.ComputedValue("right"));
  EXPECT_EQ("1px", cascade.ComputedValue("top"));
  EXPECT_EQ("1px", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, FlipRevertValue_Asymmetric) {
  TestCascade cascade(GetDocument());

  cascade.Add("left:1px", {.layer_order = 1});
  cascade.Add("right:2px", {.layer_order = 1});
  cascade.Add("top:3px", {.layer_order = 1});
  cascade.Add("bottom:4px", {.layer_order = 1});

  // Revert left to right, but not vice-versa.
  cascade.Add(FlipRevertSet("left", "right"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("top", "bottom"), {.layer_order = 2});

  cascade.Apply();

  EXPECT_EQ("2px", cascade.ComputedValue("left"));
  EXPECT_EQ("2px", cascade.ComputedValue("right"));
  EXPECT_EQ("4px", cascade.ComputedValue("top"));
  EXPECT_EQ("4px", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, FlipRevertValue_DifferentOrigins) {
  TestCascade cascade(GetDocument());

  cascade.Add("left:10px", {.origin = CascadeOrigin::kUser});

  // CascadeOrigin::kAuthor
  cascade.Add("right:2px", {.layer_order = 1});
  cascade.Add("top:3px", {.layer_order = 1});
  cascade.Add("bottom:4px", {.layer_order = 1});

  cascade.Add(FlipRevertSet("right", "left"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("bottom", "top"), {.layer_order = 2});

  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("left"));
  EXPECT_EQ("10px", cascade.ComputedValue("right"));
  EXPECT_EQ("3px", cascade.ComputedValue("top"));
  EXPECT_EQ("3px", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, FlipRevertValue_Overwritten) {
  TestCascade cascade(GetDocument());

  cascade.Add("left:1px", {.layer_order = 1});
  cascade.Add("right:2px", {.layer_order = 1});
  cascade.Add("top:3px", {.layer_order = 1});
  cascade.Add("bottom:4px", {.layer_order = 1});

  cascade.Add(FlipRevertSet("left", "right"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("right", "left"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("top", "bottom"), {.layer_order = 2});
  cascade.Add(FlipRevertSet("bottom", "top"), {.layer_order = 2});

  // Overwrite the CSSFlipRevertValues for left/top.
  cascade.Add("left:10px", {.layer_order = 3});
  cascade.Add("top:30px", {.layer_order = 3});

  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("left"));
  EXPECT_EQ("1px", cascade.ComputedValue("right"));
  EXPECT_EQ("30px", cascade.ComputedValue("top"));
  EXPECT_EQ("3px", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, InlineStyleWonCascade) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:1px", CascadeOrigin::kUserAgent);
  cascade.Add("top:2px",
              {.origin = CascadeOrigin::kAuthor, .is_inline_style = true});
  cascade.Apply();
  EXPECT_FALSE(cascade.InlineStyleLostCascade());
}

TEST_F(StyleCascadeTest, InlineStyleLostCascade) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:1px !important", {.origin = CascadeOrigin::kUserAgent});
  cascade.Add("top:2px",
              {.origin = CascadeOrigin::kAuthor, .is_inline_style = true});
  cascade.Apply();
  EXPECT_TRUE(cascade.InlineStyleLostCascade());
}

TEST_F(StyleCascadeTest, TryStyle) {
  TestCascade cascade(GetDocument());
  cascade.Add("position:absolute");
  cascade.Add("top:1px");
  cascade.Add("top:2px", {.is_inline_style = true});
  cascade.Add("top:3px", {.is_try_style = true});
  cascade.Apply();
  EXPECT_EQ("3px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, TryTacticsStyle) {
  TestCascade cascade(GetDocument());
  cascade.Add("position:absolute");
  cascade.Add("top:1px");
  cascade.Add("top:2px", {.is_try_style = true});
  cascade.Add("top:3px", {.is_try_tactics_style = true});
  cascade.Apply();
  EXPECT_EQ("3px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, TryTacticsStyleRevertLayer) {
  TestCascade cascade(GetDocument());
  cascade.Add("position:absolute");
  cascade.Add("top:1px");
  cascade.Add("top:2px", {.is_try_style = true});
  cascade.Add("top:revert-layer", {.is_try_tactics_style = true});
  cascade.Apply();
  EXPECT_EQ("2px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, TryTacticsStyleRevertTo) {
  TestCascade cascade(GetDocument());
  cascade.Add("position:absolute");
  cascade.Add("top:1px");
  cascade.Add("top:2px", {.is_try_style = true});
  cascade.Add("bottom:3px", {.is_try_style = true});
  cascade.Add(FlipRevertSet("bottom", "top"), {.is_try_tactics_style = true});
  cascade.Add(FlipRevertSet("top", "bottom"), {.is_try_tactics_style = true});
  cascade.Apply();
  EXPECT_EQ("3px", cascade.ComputedValue("top"));
  EXPECT_EQ("2px", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, RevertToAnchor) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:anchor(top, 10px)", {.origin = CascadeOrigin::kUser});
  cascade.Add("top:revert");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, RevertToAnchorInvalid) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:anchor(top)", {.origin = CascadeOrigin::kUser});
  cascade.Add("top:revert");
  cascade.Apply();
  EXPECT_EQ("auto", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, RevertLayerToAnchor) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:anchor(top, 10px)", {.layer_order = 1});
  cascade.Add("top:revert-layer", {.layer_order = 2});
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, RevertLayerToAnchorInvalid) {
  TestCascade cascade(GetDocument());
  cascade.Add("top:anchor(top)", {.layer_order = 1});
  cascade.Add("top:revert-layer", {.layer_order = 2});
  cascade.Apply();
  EXPECT_EQ("auto", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, VarInAnchor) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:top");
  cascade.Add("top:anchor(var(--x), 10px)");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, VarInAnchorInvalid) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:top");
  cascade.Add("top:anchor(var(--x))");
  cascade.Apply();
  EXPECT_EQ("auto", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, RevertToVarAnchor) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:top", {.origin = CascadeOrigin::kUser});
  cascade.Add("top:anchor(var(--x), 10px)", {.origin = CascadeOrigin::kUser});
  cascade.Add("top:revert");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("top"));
}

TEST_F(StyleCascadeTest, RevertToVarAnchorInvalid) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:top", {.origin = CascadeOrigin::kUser});
  cascade.Add("top:anchor(var(--x))", {.origin = CascadeOrigin::kUser});
  cascade.Add("top:revert");
  cascade.Apply();
  EXPECT_EQ("auto", cascade.ComputedValue("top"));
}

namespace {

// An AnchorEvaluator that responds to Mode::kTop only. This can be used to
// test what happens when a flip converts a top (valid) into a bottom
// (invalid).
class TopAnchorEvaluator : public AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const ScopedCSSName* position_anchor,
      const std::optional<PositionAreaOffsets>&) override {
    if (GetMode() == Mode::kTop) {
      return LayoutUnit(1);
    }
    return std::nullopt;
  }
  std::optional<PositionAreaOffsets> ComputePositionAreaOffsetsForLayout(
      const ScopedCSSName*,
      PositionArea) override {
    return PositionAreaOffsets();
  }
  std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder&) override {
    return std::nullopt;
  }
};

}  // namespace

TEST_F(StyleCascadeTest, FlipToAnchorInvalid) {
  TopAnchorEvaluator evaluator;
  StyleRecalcContext style_recalc_context;
  style_recalc_context.anchor_evaluator = &evaluator;

  TestCascade cascade(GetDocument(), /* element */ GetDocument().body(),
                      &style_recalc_context);
  cascade.Add("position:absolute");
  cascade.Add("top:anchor(top)");
  cascade.Add(FlipRevertSet("bottom", "top"), {.is_try_tactics_style = true});
  cascade.Add(FlipRevertSet("top", "bottom"), {.is_try_tactics_style = true});
  cascade.Apply();
  // Do not crash:
  EXPECT_EQ("auto", cascade.ComputedValue("top"));
  EXPECT_EQ("auto", cascade.ComputedValue("bottom"));
}

TEST_F(StyleCascadeTest, RevertInAppearanceAutoBaseSelectValue) {
  SetBodyInnerHTML("<select id=select></select>");
  Element* select = GetDocument().getElementById(AtomicString("select"));
  ASSERT_TRUE(select);

  // left:-internal-appearance-auto-base-select(revert, 2px)
  // (Not possible to create with the parser currently).
  const CSSValue* first = cssvalue::CSSRevertValue::Create();
  const CSSValue* second =
      css_test_helpers::ParseValue(GetDocument(), "<length>", "2px");
  auto* set = MakeGarbageCollected<MutableCSSPropertyValueSet>(kUASheetMode);
  set->SetProperty(CSSPropertyID::kLeft,
                   *MakeGarbageCollected<CSSAppearanceAutoBaseSelectValuePair>(
                       first, second));

  TestCascade cascade(GetDocument(), select);
  cascade.Add("left:300px", {.origin = CascadeOrigin::kUser});
  cascade.Add(set);
  cascade.Apply();
  EXPECT_EQ("300px", cascade.ComputedValue("left"));
}

TEST_F(StyleCascadeTest, EnvInAppearanceAutoBaseSelectValue) {
  SetBodyInnerHTML("<select id=select></select>");
  Element* select = GetDocument().getElementById(AtomicString("select"));
  ASSERT_TRUE(select);

  // UA styles don't use var(), but they could conceivably use env().
  const CSSPropertyValueSet* set = css_test_helpers::ParseDeclarationBlock(
      R"CSS(
      border-left-style: solid;
      border-left-width: -internal-appearance-auto-base-select(env(unknown, 7px), 42px);
    )CSS",
      kUASheetMode);

  TestCascade cascade(GetDocument(), select);
  cascade.Add(set);
  cascade.Apply();
  EXPECT_EQ("7px", cascade.ComputedValue("border-left-width"));
}

TEST_F(StyleCascadeTest, LhUnitCycle) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("line-height", "var(--x)");
  cascade.Add("--x", "10lh");
  cascade.Apply();

  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, SubstitutingLhCycles) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("line-height", "var(--x)");
  cascade.Add("--x", "10lh");
  cascade.Add("--y", "var(--x)");
  cascade.Add("--z", "var(--x,1px)");
  cascade.Apply();

  EXPECT_EQ("0px", cascade.ComputedValue("--y"));
  EXPECT_EQ("0px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, CSSFunctionTrivial) {
  AppendSheet(R"HTML(
     @function --foo(): color {
       @return red;
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("background-color", "--foo()");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, CSSFunctionWithArgument) {
  AppendSheet(R"HTML(
     @function --foo(--a: length): length {
       @return calc(arg(--a) * 2);
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("left", "--foo(10.00px)");
  cascade.Apply();

  EXPECT_EQ("20px", cascade.ComputedValue("left"));
}

TEST_F(StyleCascadeTest, CSSFunctionWithTwoArguments) {
  AppendSheet(R"HTML(
     @function --foo(--a: integer, --b: integer): integer {
       @return calc(arg(--a) * arg(--b));
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("z-index", "--foo(4, 6)");
  cascade.Apply();

  EXPECT_EQ("24", cascade.ComputedValue("z-index"));
}

TEST_F(StyleCascadeTest, CSSFunctionCallingOtherFunction) {
  AppendSheet(R"HTML(
     @function --foo(--a: length): length {
       @return calc(arg(--a) * 2);
     }
     @function --bar(--b: length): length {
       @return calc(--foo(arg(--b)) * 3);
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("left", "--bar(10.00px)");
  cascade.Apply();

  EXPECT_EQ("60px", cascade.ComputedValue("left"));
}

TEST_F(StyleCascadeTest, CSSFunctionReturnTypeCoercion) {
  AppendSheet(R"HTML(
     @function --returning-any(): any {
       @return var(--v);
     }
     @function --returning-length(): length {
       @return var(--v);
     }
     @function --returning-color(): color {
       @return var(--v);
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("--v", "10.00px");
  cascade.Add("--any", "--returning-any()");
  cascade.Add("--length", "--returning-length()");
  cascade.Add("--color", "--returning-color()");
  cascade.Apply();

  EXPECT_EQ("10.00px", cascade.ComputedValue("--any"));
  EXPECT_EQ("10px", cascade.ComputedValue("--length"));
  EXPECT_EQ(nullptr, cascade.ComputedValue("--color"));
}

TEST_F(StyleCascadeTest, CSSFunctionImplicitCalc) {
  AppendSheet(R"HTML(
     @function --foo(--x: number): number {
       @return arg(--x) * 2;
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("--result", "--foo(4 + 5)");
  cascade.Apply();

  EXPECT_EQ("18", cascade.ComputedValue("--result"));
}

TEST_F(StyleCascadeTest, AffectedByCSSFunction) {
  AppendSheet(R"HTML(
     @function --red(): color {
       @return red;
     }
    )HTML");

  {
    TestCascade cascade(GetDocument());
    cascade.Add("color", "--red()");
    cascade.Apply();
    EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("color"));
    EXPECT_TRUE(cascade.TakeStyle()->AffectedByCSSFunction());
  }
  {
    TestCascade cascade(GetDocument());
    cascade.Add("color", "red");
    cascade.Apply();
    EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("color"));
    EXPECT_FALSE(cascade.TakeStyle()->AffectedByCSSFunction());
  }
}

TEST_F(StyleCascadeTest, CSSFunctionDoesNotExistInShorthand) {
  for (bool enabled : {false, true}) {
    ScopedCSSFunctionsForTest scoped_feature(enabled);
    TestCascade cascade(GetDocument());

    cascade.Add("background", "--nonexistent()");
    cascade.Apply();

    EXPECT_EQ("rgba(0, 0, 0, 0)", cascade.ComputedValue("background-color"));
  }
}

}  // namespace blink
