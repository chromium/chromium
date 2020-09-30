/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/compositor_animations.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/mock_css_paint_image_generator.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/animation/compositor_color_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/geometry/float_box.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/skia/include/core/SkColor.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::Values;

namespace blink {

namespace {
// CSSPaintImageGenerator requires that CSSPaintImageGeneratorCreateFunction be
// a static method. As such, it cannot access a class member and so instead we
// store a pointer to the overriding generator globally.
MockCSSPaintImageGenerator* g_override_generator = nullptr;
CSSPaintImageGenerator* ProvideOverrideGenerator(
    const String&,
    const Document&,
    CSSPaintImageGenerator::Observer*) {
  return g_override_generator;
}
}  // namespace

using css_test_helpers::RegisterProperty;

class AnimationCompositorAnimationsTest : public PaintTestConfigurations,
                                          public RenderingTest {
 protected:
  scoped_refptr<TimingFunction> linear_timing_function_;
  scoped_refptr<TimingFunction> cubic_ease_timing_function_;
  scoped_refptr<TimingFunction> cubic_custom_timing_function_;
  scoped_refptr<TimingFunction> step_timing_function_;

  Timing timing_;
  CompositorAnimations::CompositorTiming compositor_timing_;
  Persistent<HeapVector<Member<StringKeyframe>>> keyframe_vector2_;
  Persistent<StringKeyframeEffectModel> keyframe_animation_effect2_;
  Persistent<HeapVector<Member<StringKeyframe>>> keyframe_vector5_;
  Persistent<StringKeyframeEffectModel> keyframe_animation_effect5_;

  Persistent<Element> element_;
  Persistent<Element> inline_;
  Persistent<DocumentTimeline> timeline_;

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
    linear_timing_function_ = LinearTimingFunction::Shared();
    cubic_ease_timing_function_ = CubicBezierTimingFunction::Preset(
        CubicBezierTimingFunction::EaseType::EASE);
    cubic_custom_timing_function_ =
        CubicBezierTimingFunction::Create(1, 2, 3, 4);
    step_timing_function_ =
        StepsTimingFunction::Create(1, StepsTimingFunction::StepPosition::END);

    timing_ = CreateCompositableTiming();
    compositor_timing_ = CompositorAnimations::CompositorTiming();
    // Make sure the CompositableTiming is really compositable, otherwise
    // most other tests will fail.
    ASSERT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));

    keyframe_vector2_ = CreateCompositableFloatKeyframeVector(2);
    keyframe_animation_effect2_ =
        MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector2_);

    keyframe_vector5_ = CreateCompositableFloatKeyframeVector(5);
    keyframe_animation_effect5_ =
        MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);

    GetAnimationClock().ResetTimeForTesting();

    timeline_ = GetDocument().Timeline();
    timeline_->ResetForTesting();

    // Using will-change ensures that this object will need paint properties.
    // Having an animation would normally ensure this but these tests don't
    // explicitly construct a full animation on the element.
    SetBodyInnerHTML(R"HTML(
        <div id='test' style='will-change: opacity,filter,transform; height:100px; background: green;'></div>
        <span id='inline' style='will-change: opacity,filter,transform;'>text</div>
    )HTML");
    element_ = GetDocument().getElementById("test");
    inline_ = GetDocument().getElementById("inline");

    helper_.Initialize(nullptr, nullptr, nullptr);
    base_url_ = "http://www.test.com/";
  }

 public:
  bool ConvertTimingForCompositor(const Timing& t,
                                  CompositorAnimations::CompositorTiming& out,
                                  double playback_rate = 1) {
    return CompositorAnimations::ConvertTimingForCompositor(
        t, base::TimeDelta(), out, playback_rate);
  }

  CompositorAnimations::FailureReasons CanStartEffectOnCompositor(
      const Timing& timing,
      const KeyframeEffectModelBase& effect) {
    // TODO(crbug.com/725385): Remove once compositor uses InterpolationTypes.
    auto style = GetDocument().GetStyleResolver().StyleForElement(element_);
    effect.SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);
    return CheckCanStartEffectOnCompositor(timing, *element_.Get(), nullptr,
                                           effect);
  }
  CompositorAnimations::FailureReasons CheckCanStartEffectOnCompositor(
      const Timing& timing,
      const Element& element,
      const Animation* animation,
      const EffectModel& effect_model,
      PropertyHandleSet* unsupported_properties = nullptr) {
    const PaintArtifactCompositor* paint_artifact_compositor =
        GetDocument().View()->GetPaintArtifactCompositor();
    return CompositorAnimations::CheckCanStartEffectOnCompositor(
        timing, element, animation, effect_model, paint_artifact_compositor, 1,
        unsupported_properties);
  }

  CompositorAnimations::FailureReasons CheckCanStartElementOnCompositor(
      const Element& element,
      const EffectModel& model) {
    return CompositorAnimations::CheckCanStartElementOnCompositor(element,
                                                                  model);
  }

  void GetAnimationOnCompositor(
      Timing& timing,
      StringKeyframeEffectModel& effect,
      Vector<std::unique_ptr<CompositorKeyframeModel>>& keyframe_models,
      double animation_playback_rate) {
    CompositorAnimations::GetAnimationOnCompositor(
        *element_, timing, 0, base::nullopt, base::TimeDelta(), effect,
        keyframe_models, animation_playback_rate);
  }

  CompositorAnimations::FailureReasons
  DuplicateSingleKeyframeAndTestIsCandidateOnResult(StringKeyframe* frame) {
    EXPECT_EQ(frame->CheckedOffset(), 0);
    StringKeyframeVector frames;
    Keyframe* second = frame->CloneWithOffset(1);

    frames.push_back(frame);
    frames.push_back(To<StringKeyframe>(second));
    return CanStartEffectOnCompositor(
        timing_, *MakeGarbageCollected<StringKeyframeEffectModel>(frames));
  }

  // -------------------------------------------------------------------

  Timing CreateCompositableTiming() {
    Timing timing;
    timing.start_delay = 0;
    timing.fill_mode = Timing::FillMode::NONE;
    timing.iteration_start = 0;
    timing.iteration_count = 1;
    timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);
    timing.direction = Timing::PlaybackDirection::NORMAL;
    timing.timing_function = linear_timing_function_;
    return timing;
  }

  StringKeyframe* CreateReplaceOpKeyframe(CSSPropertyID id,
                                          const String& value,
                                          double offset = 0) {
    auto* keyframe = MakeGarbageCollected<StringKeyframe>();
    keyframe->SetCSSPropertyValue(id, value,
                                  SecureContextMode::kInsecureContext, nullptr);
    keyframe->SetComposite(EffectModel::kCompositeReplace);
    keyframe->SetOffset(offset);
    keyframe->SetEasing(LinearTimingFunction::Shared());
    return keyframe;
  }

  StringKeyframe* CreateReplaceOpKeyframe(const String& property_name,
                                          const String& value,
                                          double offset = 0) {
    auto* keyframe = MakeGarbageCollected<StringKeyframe>();
    keyframe->SetCSSPropertyValue(
        AtomicString(property_name), value,
        GetDocument().GetExecutionContext()->GetSecureContextMode(),
        GetDocument().ElementSheet().Contents());
    keyframe->SetComposite(EffectModel::kCompositeReplace);
    keyframe->SetOffset(offset);
    keyframe->SetEasing(LinearTimingFunction::Shared());
    return keyframe;
  }

  StringKeyframe* CreateDefaultKeyframe(CSSPropertyID id,
                                        EffectModel::CompositeOperation op,
                                        double offset = 0) {
    String value = "0.1";
    if (id == CSSPropertyID::kTransform)
      value = "none";
    else if (id == CSSPropertyID::kColor)
      value = "red";

    StringKeyframe* keyframe = CreateReplaceOpKeyframe(id, value, offset);
    keyframe->SetComposite(op);
    return keyframe;
  }

  HeapVector<Member<StringKeyframe>>* CreateCompositableFloatKeyframeVector(
      size_t n) {
    Vector<double> values;
    for (size_t i = 0; i < n; i++) {
      values.push_back(static_cast<double>(i));
    }
    return CreateCompositableFloatKeyframeVector(values);
  }

  HeapVector<Member<StringKeyframe>>* CreateCompositableFloatKeyframeVector(
      Vector<double>& values) {
    HeapVector<Member<StringKeyframe>>* frames =
        MakeGarbageCollected<HeapVector<Member<StringKeyframe>>>();
    for (wtf_size_t i = 0; i < values.size(); i++) {
      double offset = 1.0 / (values.size() - 1) * i;
      String value = String::Number(values[i]);
      frames->push_back(
          CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, value, offset));
    }
    return frames;
  }

  void SetCustomProperty(const String& name, const String& value) {
    DummyExceptionStateForTesting exception_state;
    element_->style()->setProperty(GetDocument().GetExecutionContext(), name,
                                   value, g_empty_string, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_TRUE(element_->style()->getPropertyValue(name));
  }

  // This class exists to dodge the interlock between creating compositor
  // keyframe values iff we can animate them on the compositor, and hence can
  // start their animations on it. i.e. two far away switch statements have
  // matching non-default values, preventing us from testing the default.
  class MockStringKeyframe : public StringKeyframe {
   public:
    static StringKeyframe* Create(double offset) {
      return MakeGarbageCollected<MockStringKeyframe>(offset);
    }

    MockStringKeyframe(double offset)
        : StringKeyframe(),
          property_specific_(
              MakeGarbageCollected<MockPropertySpecificStringKeyframe>(
                  offset)) {
      SetOffset(offset);
    }

    Keyframe::PropertySpecificKeyframe* CreatePropertySpecificKeyframe(
        const PropertyHandle&,
        EffectModel::CompositeOperation,
        double) const final {
      return property_specific_;  // We know a shortcut.
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(property_specific_);
      StringKeyframe::Trace(visitor);
    }

   private:
    class MockPropertySpecificStringKeyframe : public PropertySpecificKeyframe {
     public:
      // Pretend to have a compositor keyframe value. Pick the offset for pure
      // convenience: it matters not what it is.
      MockPropertySpecificStringKeyframe(double offset)
          : PropertySpecificKeyframe(offset,
                                     LinearTimingFunction::Shared(),
                                     EffectModel::kCompositeReplace),
            compositor_keyframe_value_(
                MakeGarbageCollected<CompositorKeyframeDouble>(offset)) {}
      bool IsNeutral() const final { return true; }
      bool IsRevert() const final { return false; }
      PropertySpecificKeyframe* CloneWithOffset(double) const final {
        NOTREACHED();
        return nullptr;
      }
      bool PopulateCompositorKeyframeValue(
          const PropertyHandle&,
          Element&,
          const ComputedStyle& base_style,
          const ComputedStyle* parent_style) const final {
        return true;
      }
      const CompositorKeyframeValue* GetCompositorKeyframeValue() const final {
        return compositor_keyframe_value_;
      }
      PropertySpecificKeyframe* NeutralKeyframe(
          double,
          scoped_refptr<TimingFunction>) const final {
        NOTREACHED();
        return nullptr;
      }

      void Trace(Visitor* visitor) const override {
        visitor->Trace(compositor_keyframe_value_);
        PropertySpecificKeyframe::Trace(visitor);
      }

     private:
      Member<CompositorKeyframeDouble> compositor_keyframe_value_;
    };

    Member<PropertySpecificKeyframe> property_specific_;
  };

  StringKeyframe* CreateMockReplaceKeyframe(CSSPropertyID id,
                                            const String& value,
                                            double offset) {
    StringKeyframe* keyframe = MockStringKeyframe::Create(offset);
    keyframe->SetCSSPropertyValue(id, value,
                                  SecureContextMode::kInsecureContext, nullptr);
    keyframe->SetComposite(EffectModel::kCompositeReplace);
    keyframe->SetEasing(LinearTimingFunction::Shared());

    return keyframe;
  }

  StringKeyframe* CreateSVGKeyframe(const QualifiedName& name,
                                    const String& value,
                                    double offset) {
    auto* keyframe = MakeGarbageCollected<StringKeyframe>();
    keyframe->SetSVGAttributeValue(name, value);
    keyframe->SetComposite(EffectModel::kCompositeReplace);
    keyframe->SetOffset(offset);
    keyframe->SetEasing(LinearTimingFunction::Shared());

    return keyframe;
  }

  StringKeyframeEffectModel* CreateKeyframeEffectModel(
      StringKeyframe* from,
      StringKeyframe* to,
      StringKeyframe* c = nullptr,
      StringKeyframe* d = nullptr) {
    EXPECT_EQ(from->CheckedOffset(), 0);
    StringKeyframeVector frames;
    frames.push_back(from);
    EXPECT_LE(from->Offset(), to->Offset());
    frames.push_back(to);
    if (c) {
      EXPECT_LE(to->Offset(), c->Offset());
      frames.push_back(c);
    }
    if (d) {
      frames.push_back(d);
      EXPECT_LE(c->Offset(), d->Offset());
      EXPECT_EQ(d->CheckedOffset(), 1.0);
    } else {
      EXPECT_EQ(to->CheckedOffset(), 1.0);
    }
    if (!HasFatalFailure()) {
      return MakeGarbageCollected<StringKeyframeEffectModel>(frames);
    }
    return nullptr;
  }

  void SimulateFrame(double time) {
    GetAnimationClock().UpdateTime(base::TimeTicks() +
                                   base::TimeDelta::FromSecondsD(time));
    GetPendingAnimations().Update(nullptr, false);
    timeline_->ServiceAnimations(kTimingUpdateForAnimationFrame);
  }

  std::unique_ptr<CompositorKeyframeModel> ConvertToCompositorAnimation(
      StringKeyframeEffectModel& effect,
      double animation_playback_rate) {
    // As the compositor code only understands CompositorKeyframeValues, we must
    // snapshot the effect to make those available.
    // TODO(crbug.com/725385): Remove once compositor uses InterpolationTypes.
    auto style = GetDocument().GetStyleResolver().StyleForElement(element_);
    effect.SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

    Vector<std::unique_ptr<CompositorKeyframeModel>> result;
    GetAnimationOnCompositor(timing_, effect, result, animation_playback_rate);
    DCHECK_EQ(1U, result.size());
    return std::move(result[0]);
  }

  std::unique_ptr<CompositorKeyframeModel> ConvertToCompositorAnimation(
      StringKeyframeEffectModel& effect) {
    return ConvertToCompositorAnimation(effect, 1.0);
  }

  void ExpectKeyframeTimingFunctionCubic(
      const CompositorFloatKeyframe& keyframe,
      const CubicBezierTimingFunction::EaseType ease_type) {
    auto keyframe_timing_function = keyframe.GetTimingFunctionForTesting();
    DCHECK_EQ(keyframe_timing_function->GetType(),
              TimingFunction::Type::CUBIC_BEZIER);
    const auto& cubic_timing_function =
        To<CubicBezierTimingFunction>(*keyframe_timing_function);
    EXPECT_EQ(cubic_timing_function.GetEaseType(), ease_type);
  }

  void LoadTestData(const std::string& file_name) {
    String testing_path =
        test::BlinkRootDir() + "/renderer/core/animation/test_data/";
    WebURL url = url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing_path,
        WebString::FromUTF8(file_name));
    frame_test_helpers::LoadFrame(helper_.GetWebView()->MainFrameImpl(),
                                  base_url_ + file_name);
    ForceFullCompositingUpdate();
    url_test_helpers::RegisterMockedURLUnregister(url);
  }

  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

  void BeginFrame() {
    helper_.GetWebView()
        ->MainFrameViewWidget()
        ->SynchronouslyCompositeForTesting(base::TimeTicks::Now());
  }

  void ForceFullCompositingUpdate() {
    helper_.GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

 private:
  frame_test_helpers::WebViewHelper helper_;
  std::string base_url_;
};

class LayoutObjectProxy : public LayoutObject {
 public:
  static LayoutObjectProxy* Create(Node* node) {
    return new LayoutObjectProxy(node);
  }

  static void Dispose(LayoutObjectProxy* proxy) { proxy->Destroy(); }

  const char* GetName() const override { return nullptr; }
  void UpdateLayout() override {}
  FloatRect LocalBoundingBoxRectForAccessibility() const override {
    return FloatRect();
  }

  void EnsureIdForTestingProxy() {
    // We need Ids of proxies to be valid.
    EnsureIdForTesting();
  }

 private:
  explicit LayoutObjectProxy(Node* node) : LayoutObject(node) {}
};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

INSTANTIATE_PAINT_TEST_SUITE_P(AnimationCompositorAnimationsTest);

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorKeyframeMultipleCSSProperties) {
  StringKeyframe* keyframe_good_multiple = CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace);
  keyframe_good_multiple->SetCSSPropertyValue(
      CSSPropertyID::kTransform, "none", SecureContextMode::kInsecureContext,
      nullptr);
  EXPECT_EQ(
      DuplicateSingleKeyframeAndTestIsCandidateOnResult(keyframe_good_multiple),
      CompositorAnimations::kNoFailure);

  StringKeyframe* keyframe_bad_multiple_id = CreateDefaultKeyframe(
      CSSPropertyID::kColor, EffectModel::kCompositeReplace);
  keyframe_bad_multiple_id->SetCSSPropertyValue(
      CSSPropertyID::kOpacity, "0.1", SecureContextMode::kInsecureContext,
      nullptr);
  EXPECT_TRUE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(
                  keyframe_bad_multiple_id) &
              CompositorAnimations::kUnsupportedCSSProperty);
}

TEST_P(AnimationCompositorAnimationsTest,
       IsNotCandidateForCompositorAnimationTransformDependsOnBoxSize) {
  // Absolute transforms can be animated on the compositor.
  String transform = "translateX(2px) translateY(2px)";
  StringKeyframe* good_keyframe =
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, transform);
  EXPECT_EQ(DuplicateSingleKeyframeAndTestIsCandidateOnResult(good_keyframe),
            CompositorAnimations::kNoFailure);

  // Transforms that rely on the box size, such as percent calculations, cannot
  // be animated on the compositor (as the box size may change).
  String transform2 = "translateX(50%) translateY(2px)";
  StringKeyframe* bad_keyframe =
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, transform2);
  EXPECT_TRUE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(bad_keyframe) &
              CompositorAnimations::kTransformRelatedPropertyDependsOnBoxSize);

  // Similarly, calc transforms cannot be animated on the compositor.
  String transform3 = "translateX(calc(100% + (0.5 * 100px)))";
  StringKeyframe* bad_keyframe2 =
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, transform3);
  EXPECT_TRUE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(bad_keyframe2) &
              CompositorAnimations::kTransformRelatedPropertyDependsOnBoxSize);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorKeyframeEffectModel) {
  StringKeyframeVector frames_same;
  frames_same.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kColor, EffectModel::kCompositeReplace, 0.0));
  frames_same.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kColor, EffectModel::kCompositeReplace, 1.0));
  EXPECT_TRUE(CanStartEffectOnCompositor(
                  timing_, *MakeGarbageCollected<StringKeyframeEffectModel>(
                               frames_same)) &
              CompositorAnimations::kUnsupportedCSSProperty);

  StringKeyframeVector frames_mixed_properties;
  auto* keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kColor, "red",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kColor, "green",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  EXPECT_TRUE(CanStartEffectOnCompositor(
                  timing_, *MakeGarbageCollected<StringKeyframeEffectModel>(
                               frames_mixed_properties)) &
              CompositorAnimations::kUnsupportedCSSProperty);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorCustomCssProperty) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RegisterProperty(GetDocument(), "--foo", "<number>", "0", false);
  RegisterProperty(GetDocument(), "--bar", "<length>", "10px", false);
  RegisterProperty(GetDocument(), "--loo", "<color>", "rgb(0, 0, 0)", false);
  RegisterProperty(GetDocument(), "--x", "<number>", "0", false);
  RegisterProperty(GetDocument(), "--y", "<number>", "200", false);
  RegisterProperty(GetDocument(), "--z", "<number>", "200", false);
  SetCustomProperty("--foo", "10");
  SetCustomProperty("--bar", "10px");
  SetCustomProperty("--loo", "rgb(0, 255, 0)");
  SetCustomProperty("--x", "5");

  UpdateAllLifecyclePhasesForTest();
  auto style = GetDocument().GetStyleResolver().StyleForElement(element_);
  EXPECT_TRUE(style->NonInheritedVariables());
  EXPECT_TRUE(style->NonInheritedVariables()
                  ->GetData(AtomicString("--foo"))
                  .value_or(nullptr));
  EXPECT_TRUE(style->NonInheritedVariables()
                  ->GetData(AtomicString("--bar"))
                  .value_or(nullptr));
  EXPECT_TRUE(style->NonInheritedVariables()
                  ->GetData(AtomicString("--loo"))
                  .value_or(nullptr));
  EXPECT_TRUE(style->NonInheritedVariables()
                  ->GetData(AtomicString("--x"))
                  .value_or(nullptr));
  EXPECT_TRUE(style->GetVariableData("--y"));
  EXPECT_TRUE(style->GetVariableData("--z"));

  NiceMock<MockCSSPaintImageGenerator>* mock_generator =
      MakeGarbageCollected<NiceMock<MockCSSPaintImageGenerator>>();
  base::AutoReset<MockCSSPaintImageGenerator*> scoped_override_generator(
      &g_override_generator, mock_generator);
  base::AutoReset<CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction>
      scoped_create_function(
          CSSPaintImageGenerator::GetCreateFunctionForTesting(),
          ProvideOverrideGenerator);

  mock_generator->AddCustomProperty("--foo");
  mock_generator->AddCustomProperty("--bar");
  mock_generator->AddCustomProperty("--loo");
  mock_generator->AddCustomProperty("--y");
  mock_generator->AddCustomProperty("--z");
  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("foopainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  paint_value->CreateGeneratorForTesting(GetDocument());
  StyleGeneratedImage* style_image =
      MakeGarbageCollected<StyleGeneratedImage>(*paint_value);
  style->AddPaintImage(style_image);
  element_->GetLayoutObject()->SetStyle(style);
  // The image is added for testing off-thread paint worklet supporting
  // custom property animation case. The style doesn't have a real
  // PaintImage, so we cannot call UpdateAllLifecyclePhasesForTest. But the
  // PaintArtifactCompositor requires NeedsUpdate to be false.
  // In the real world when a PaintImage does exist in the style, the life
  // cycle will be updated automatically and we don't have to worry about
  // this.
  auto* paint_artifact_compositor =
      GetDocument().View()->GetPaintArtifactCompositor();
  paint_artifact_compositor->ClearNeedsUpdateForTesting();

  ON_CALL(*mock_generator, IsImageGeneratorReady()).WillByDefault(Return(true));
  StringKeyframe* keyframe = CreateReplaceOpKeyframe("--foo", "10");
  EXPECT_EQ(DuplicateSingleKeyframeAndTestIsCandidateOnResult(keyframe),
            CompositorAnimations::kNoFailure);

  // Color-valued properties are supported
  StringKeyframe* color_keyframe =
      CreateReplaceOpKeyframe("--loo", "rgb(0, 255, 0)");
  EXPECT_EQ(DuplicateSingleKeyframeAndTestIsCandidateOnResult(color_keyframe),
            CompositorAnimations::kNoFailure);

  // Length-valued properties are not compositable.
  StringKeyframe* non_animatable_keyframe =
      CreateReplaceOpKeyframe("--bar", "10px");
  EXPECT_TRUE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(
                  non_animatable_keyframe) &
              CompositorAnimations::kUnsupportedCSSProperty);

  // Cannot composite due to side effect.
  SetCustomProperty("opacity", "var(--foo)");
  EXPECT_TRUE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(keyframe) &
              CompositorAnimations::kUnsupportedCSSProperty);

  // Cannot composite because "--x" is not used by the paint worklet.
  StringKeyframe* non_used_keyframe = CreateReplaceOpKeyframe("--x", "5");
  EXPECT_EQ(
      DuplicateSingleKeyframeAndTestIsCandidateOnResult(non_used_keyframe),
      CompositorAnimations::kUnsupportedCSSProperty);

  // Implicitly initial values are supported.
  StringKeyframe* y_keyframe = CreateReplaceOpKeyframe("--y", "1000");
  EXPECT_EQ(DuplicateSingleKeyframeAndTestIsCandidateOnResult(y_keyframe),
            CompositorAnimations::kNoFailure);

  // Implicitly initial values are not supported when the property
  // has been referenced.
  SetCustomProperty("opacity", "var(--z)");
  StringKeyframe* z_keyframe = CreateReplaceOpKeyframe("--z", "1000");
  EXPECT_EQ(DuplicateSingleKeyframeAndTestIsCandidateOnResult(z_keyframe),
            CompositorAnimations::kUnsupportedCSSProperty);
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorStartDelay) {
  const double play_forward = 1;
  const double play_reverse = -1;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(20);

  timing_.start_delay = 2.0;
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_forward));
  EXPECT_DOUBLE_EQ(-2.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_reverse));
  EXPECT_DOUBLE_EQ(0.0, compositor_timing_.scaled_time_offset.InSecondsF());

  timing_.start_delay = -2.0;
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_forward));
  EXPECT_DOUBLE_EQ(2.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_reverse));
  EXPECT_DOUBLE_EQ(0.0, compositor_timing_.scaled_time_offset.InSecondsF());
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationStart) {
  timing_.iteration_start = 2.2;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationCount) {
  timing_.iteration_count = 5.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(5, compositor_timing_.adjusted_iteration_count);

  timing_.iteration_count = 5.5;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(5.5, compositor_timing_.adjusted_iteration_count);

  timing_.iteration_count = std::numeric_limits<double>::infinity();
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            compositor_timing_.adjusted_iteration_count);

  timing_.iteration_count = std::numeric_limits<double>::infinity();
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(5);
  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            compositor_timing_.adjusted_iteration_count);
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationsAndStartDelay) {
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(5);

  timing_.start_delay = 6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(-6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_DOUBLE_EQ(4.0, compositor_timing_.adjusted_iteration_count);

  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_DOUBLE_EQ(4.0, compositor_timing_.adjusted_iteration_count);

  timing_.start_delay = 21.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
}

TEST_P(AnimationCompositorAnimationsTest, ConvertTimingForCompositorDirection) {
  timing_.direction = Timing::PlaybackDirection::NORMAL;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction, Timing::PlaybackDirection::NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);

  timing_.direction = Timing::PlaybackDirection::REVERSE;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction, Timing::PlaybackDirection::REVERSE);
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorDirectionIterationsAndStartDelay) {
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(5);
  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(5);
  timing_.start_delay = -11.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(11.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(5);
  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(5);
  timing_.start_delay = -11.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(11.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionLinear) {
  timing_.timing_function = linear_timing_function_;
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionCubic) {
  timing_.timing_function = cubic_ease_timing_function_;
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);

  timing_.timing_function = cubic_custom_timing_function_;
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionSteps) {
  timing_.timing_function = step_timing_function_;
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionChainedLinear) {
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorNonLinearTimingFunctionOnFirstOrLastFrame) {
  keyframe_vector2_->at(0)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_animation_effect2_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector2_);

  keyframe_vector5_->at(3)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_animation_effect5_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);

  timing_.timing_function = cubic_ease_timing_function_;
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);

  timing_.timing_function = cubic_custom_timing_function_;
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorEffectOpacity) {
  // Check that we got something effectively different.
  StringKeyframeVector key_frames;
  key_frames.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 0.0));
  key_frames.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 1.0));
  KeyframeEffectModelBase* animation_effect =
      MakeGarbageCollected<StringKeyframeEffectModel>(key_frames);

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);

  // The first animation for opacity is ok to run on compositor.
  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_, animation_effect, timing);
  Animation* animation = timeline_->Play(keyframe_effect1);
  auto style = ComputedStyle::Create();
  animation_effect->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(),
                                                              *style, nullptr);

  // Now we can check that we are set up correctly.
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing, *element_.Get(), animation,
                                            *animation_effect),
            CompositorAnimations::kNoFailure);

  // Timings have to be convertible for compositor.
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing, *element_.Get(), animation,
                                            *animation_effect),
            CompositorAnimations::kNoFailure);
  timing.end_delay = 1.0;
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing, *element_.Get(),
                                              animation, *animation_effect) &
              CompositorAnimations::kEffectHasUnsupportedTimingParameters);
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing, *element_.Get(),
                                              animation, *animation_effect) &
              (CompositorAnimations::kTargetHasInvalidCompositingState |
               CompositorAnimations::kEffectHasUnsupportedTimingParameters));
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorEffectInvalid) {
  auto style = ComputedStyle::Create();

  // Check that we notice the value is not animatable correctly.
  const CSSProperty& target_property1(GetCSSPropertyOutlineStyle());
  PropertyHandle target_property1h(target_property1);
  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(target_property1.PropertyID(), "dotted", 0),
      CreateReplaceOpKeyframe(target_property1.PropertyID(), "dashed", 1.0));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  const auto& keyframes1 =
      *effect1->GetPropertySpecificKeyframes(target_property1h);
  EXPECT_EQ(2u, keyframes1.size());
  EXPECT_FALSE(keyframes1[0]->GetCompositorKeyframeValue());
  EXPECT_EQ(1u, effect1->Properties().size());
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                              animation1, *effect1) &
              CompositorAnimations::kUnsupportedCSSProperty);

  // Check that we notice transform is not animatable correctly on an inline.
  const CSSProperty& target_property2(GetCSSPropertyScale());
  PropertyHandle target_property2h(target_property2);
  StringKeyframeEffectModel* effect2 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(target_property2.PropertyID(), "1", 0),
      CreateReplaceOpKeyframe(target_property2.PropertyID(), "3", 1.0));

  auto* keyframe_effect2 =
      MakeGarbageCollected<KeyframeEffect>(inline_.Get(), effect2, timing_);

  Animation* animation2 = timeline_->Play(keyframe_effect2);
  effect2->SnapshotAllCompositorKeyframesIfNecessary(*inline_.Get(), *style,
                                                     nullptr);

  const auto& keyframes2 =
      *effect2->GetPropertySpecificKeyframes(target_property2h);
  EXPECT_EQ(2u, keyframes2.size());
  EXPECT_TRUE(keyframes2[0]->GetCompositorKeyframeValue());
  EXPECT_EQ(1u, effect2->Properties().size());
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(),
                                              animation2, *effect2) &
              CompositorAnimations::
                  kTransformRelatedPropertyCannotBeAcceleratedOnTarget);

  // Check that we notice the Property is not animatable correctly.
  // These ones claim to have animatable values, but we can't composite
  // the property. We also don't know the ID domain.
  const CSSProperty& target_property3(GetCSSPropertyWidth());
  PropertyHandle target_property3h(target_property3);
  StringKeyframeEffectModel* effect3 = CreateKeyframeEffectModel(
      CreateMockReplaceKeyframe(target_property3.PropertyID(), "10px", 0.0),
      CreateMockReplaceKeyframe(target_property3.PropertyID(), "20px", 1.0));

  auto* keyframe_effect3 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect3, timing_);

  Animation* animation3 = timeline_->Play(keyframe_effect3);
  effect3->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  const auto& keyframes3 =
      *effect3->GetPropertySpecificKeyframes(target_property3h);
  EXPECT_EQ(2u, keyframes3.size());
  EXPECT_TRUE(keyframes3[0]->GetCompositorKeyframeValue());
  EXPECT_EQ(1u, effect3->Properties().size());
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                              animation3, *effect3) &
              CompositorAnimations::kUnsupportedCSSProperty);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorEffectFilter) {
  // TODO(https://crbug.com/960953): Create a filter effect node when
  // will-change: filter is specified so that filter effects can be tested
  // without compositing changes.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  // Filter Properties use a different ID namespace
  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "none", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "sepia(50%)", 1.0));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  auto style = ComputedStyle::Create();
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  // Now we can check that we are set up correctly.
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                            animation1, *effect1),
            CompositorAnimations::kNoFailure);

  // Filters that affect neighboring pixels can't be composited.
  StringKeyframeEffectModel* effect2 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "none", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "blur(10px)", 1.0));

  auto* keyframe_effect2 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect2, timing_);

  Animation* animation2 = timeline_->Play(keyframe_effect2);
  effect2->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                              animation2, *effect2) &
              CompositorAnimations::kFilterRelatedPropertyMayMovePixels);

  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                              animation2, *effect2) &
              (CompositorAnimations::kFilterRelatedPropertyMayMovePixels |
               CompositorAnimations::kTargetHasInvalidCompositingState));
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorEffectTransform) {
  auto style = ComputedStyle::Create();

  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "none", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "rotate(45deg)", 1.0));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  // Check if our layout object is not TransformApplicable
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(),
                                              animation1, *effect1) &
              CompositorAnimations::
                  kTransformRelatedPropertyCannotBeAcceleratedOnTarget);

  StringKeyframeEffectModel* effect2 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateX(-45px)",
                              0),
      CreateReplaceOpKeyframe(CSSPropertyID::kRotate, "none", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateX(45px)",
                              1.0),
      CreateReplaceOpKeyframe(CSSPropertyID::kRotate, "45deg", 1.0));

  auto* keyframe_effect2 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect2, timing_);

  Animation* animation2 = timeline_->Play(keyframe_effect2);
  effect2->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                              animation2, *effect2) &
              CompositorAnimations::kMultipleTransformAnimationsOnSameTarget);
}

TEST_P(AnimationCompositorAnimationsTest,
       CheckCanStartEffectOnCompositorUnsupportedCSSProperties) {
  auto style = ComputedStyle::Create();

  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "1", 1));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  // Make sure supported properties do not register a failure
  PropertyHandleSet unsupported_properties1;
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(), animation1,
                                            *effect1, &unsupported_properties1),
            CompositorAnimations::kNoFailure);
  EXPECT_TRUE(unsupported_properties1.IsEmpty());

  StringKeyframeEffectModel* effect2 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kHeight, "100px", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kHeight, "200px", 1));

  auto* keyframe_effect2 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect2, timing_);

  Animation* animation2 = timeline_->Play(keyframe_effect2);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  // Make sure unsupported properties are reported
  PropertyHandleSet unsupported_properties2;
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(),
                                              animation2, *effect2,
                                              &unsupported_properties2) &
              CompositorAnimations::kUnsupportedCSSProperty);
  EXPECT_EQ(unsupported_properties2.size(), 1U);
  EXPECT_EQ(
      unsupported_properties2.begin()->GetCSSPropertyName().ToAtomicString(),
      "height");

  StringKeyframeEffectModel* effect3 =
      MakeGarbageCollected<StringKeyframeEffectModel>(StringKeyframeVector({
          CreateReplaceOpKeyframe(CSSPropertyID::kHeight, "100px", 0),
          CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0", 0),
          CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateY(0)",
                                  0),
          CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "grayscale(50%)", 0),
          CreateReplaceOpKeyframe(CSSPropertyID::kHeight, "200px", 1),
          CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "1", 1),
          CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateY(50px)",
                                  1),
          CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "grayscale(100%)", 1),
      }));

  auto* keyframe_effect3 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect3, timing_);

  Animation* animation3 = timeline_->Play(keyframe_effect3);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

  // Make sure only the unsupported properties are reported
  PropertyHandleSet unsupported_properties3;
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(),
                                              animation3, *effect3,
                                              &unsupported_properties3) &
              CompositorAnimations::kUnsupportedCSSProperty);
  EXPECT_EQ(unsupported_properties3.size(), 1U);
  EXPECT_EQ(
      unsupported_properties3.begin()->GetCSSPropertyName().ToAtomicString(),
      "height");
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionChainedCubicMatchingOffsets) {
  keyframe_vector2_->at(0)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_animation_effect2_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector2_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);

  keyframe_vector2_->at(0)->SetEasing(cubic_custom_timing_function_.get());
  keyframe_animation_effect2_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector2_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);

  keyframe_vector5_->at(0)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_vector5_->at(1)->SetEasing(cubic_custom_timing_function_.get());
  keyframe_vector5_->at(2)->SetEasing(cubic_custom_timing_function_.get());
  keyframe_vector5_->at(3)->SetEasing(cubic_custom_timing_function_.get());
  keyframe_animation_effect5_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionMixedGood) {
  keyframe_vector5_->at(0)->SetEasing(linear_timing_function_.get());
  keyframe_vector5_->at(1)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_vector5_->at(2)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_vector5_->at(3)->SetEasing(linear_timing_function_.get());
  keyframe_animation_effect5_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionWithStepOrFrameOkay) {
  keyframe_vector2_->at(0)->SetEasing(step_timing_function_.get());
  keyframe_animation_effect2_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector2_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_),
            CompositorAnimations::kNoFailure);

  keyframe_vector5_->at(0)->SetEasing(step_timing_function_.get());
  keyframe_vector5_->at(1)->SetEasing(linear_timing_function_.get());
  keyframe_vector5_->at(2)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_animation_effect5_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);

  keyframe_vector5_->at(1)->SetEasing(step_timing_function_.get());
  keyframe_vector5_->at(2)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_vector5_->at(3)->SetEasing(linear_timing_function_.get());
  keyframe_animation_effect5_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);

  keyframe_vector5_->at(0)->SetEasing(linear_timing_function_.get());
  keyframe_vector5_->at(2)->SetEasing(cubic_ease_timing_function_.get());
  keyframe_vector5_->at(3)->SetEasing(step_timing_function_.get());
  keyframe_animation_effect5_ =
      MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest, CanStartEffectOnCompositorBasic) {
  StringKeyframeVector basic_frames_vector;
  basic_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 0.0));
  basic_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 1.0));

  StringKeyframeVector non_basic_frames_vector;
  non_basic_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 0.0));
  non_basic_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 0.5));
  non_basic_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 1.0));

  basic_frames_vector[0]->SetEasing(linear_timing_function_.get());
  auto* basic_frames =
      MakeGarbageCollected<StringKeyframeEffectModel>(basic_frames_vector);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *basic_frames),
            CompositorAnimations::kNoFailure);

  basic_frames_vector[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  basic_frames =
      MakeGarbageCollected<StringKeyframeEffectModel>(basic_frames_vector);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *basic_frames),
            CompositorAnimations::kNoFailure);

  non_basic_frames_vector[0]->SetEasing(linear_timing_function_.get());
  non_basic_frames_vector[1]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  auto* non_basic_frames =
      MakeGarbageCollected<StringKeyframeEffectModel>(non_basic_frames_vector);
  EXPECT_EQ(CanStartEffectOnCompositor(timing_, *non_basic_frames),
            CompositorAnimations::kNoFailure);

  StringKeyframeVector non_allowed_frames_vector;
  non_allowed_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeAdd, 0.1));
  non_allowed_frames_vector.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeAdd, 0.25));
  auto* non_allowed_frames = MakeGarbageCollected<StringKeyframeEffectModel>(
      non_allowed_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *non_allowed_frames) &
              CompositorAnimations::kEffectHasNonReplaceCompositeMode);

  // Set SVGAttribute keeps a pointer to this thing for the lifespan of
  // the Keyframe.  This is ugly but sufficient to work around it.
  QualifiedName fake_name("prefix", "local", "uri");

  StringKeyframeVector non_css_frames_vector;
  non_css_frames_vector.push_back(CreateSVGKeyframe(fake_name, "cargo", 0.0));
  non_css_frames_vector.push_back(CreateSVGKeyframe(fake_name, "cargo", 1.0));
  auto* non_css_frames =
      MakeGarbageCollected<StringKeyframeEffectModel>(non_css_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *non_css_frames) &
              CompositorAnimations::kAnimationAffectsNonCSSProperties);
  EXPECT_TRUE(non_css_frames->HasNonVariableProperty());
  // NB: Important that non_css_frames_vector goes away and cleans up
  // before fake_name.
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_P(AnimationCompositorAnimationsTest, CreateSimpleOpacityAnimation) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(1.0, keyframe_model->Iterations());
  EXPECT_EQ(0, keyframe_model->TimeOffset());
  EXPECT_EQ(CompositorKeyframeModel::Direction::NORMAL,
            keyframe_model->GetDirection());
  EXPECT_EQ(1.0, keyframe_model->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time());
  EXPECT_EQ(0.5f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationDuration) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  const AnimationTimeDelta kDuration = AnimationTimeDelta::FromSecondsD(10);
  timing_.iteration_duration = kDuration;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(kDuration, keyframes[1]->Time() * kDuration);
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateMultipleKeyframeOpacityAnimationLinear) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.0", 0.25),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.25", 0.5),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.iteration_count = 5;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect, 2.0);
  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(5.0, keyframe_model->Iterations());
  EXPECT_EQ(0, keyframe_model->TimeOffset());
  EXPECT_EQ(CompositorKeyframeModel::Direction::ALTERNATE_NORMAL,
            keyframe_model->GetDirection());
  EXPECT_EQ(2.0, keyframe_model->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(0.25, keyframes[1]->Time());
  EXPECT_EQ(0, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(0.5, keyframes[2]->Time());
  EXPECT_EQ(0.25f, keyframes[2]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[2]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[3]->Time());
  EXPECT_EQ(0.5f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[3]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationStartDelay) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  const double kStartDelay = 3.25;

  timing_.iteration_count = 5.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(1.75);
  timing_.start_delay = kStartDelay;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);

  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(5.0, keyframe_model->Iterations());
  EXPECT_EQ(-kStartDelay, keyframe_model->TimeOffset());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(1.75,
            keyframes[1]->Time() * timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.5f, keyframes[1]->Value());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateMultipleKeyframeOpacityAnimationChained) {
  // KeyframeEffect to convert
  StringKeyframeVector frames;
  frames.push_back(CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0));
  frames.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.0", 0.25));
  frames.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.35", 0.5));
  frames.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));
  frames[0]->SetEasing(cubic_ease_timing_function_.get());
  frames[1]->SetEasing(linear_timing_function_.get());
  frames[2]->SetEasing(cubic_custom_timing_function_.get());
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(frames);

  timing_.timing_function = linear_timing_function_.get();
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(2);
  timing_.iteration_count = 10;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(10.0, keyframe_model->Iterations());
  EXPECT_EQ(0, keyframe_model->TimeOffset());
  EXPECT_EQ(CompositorKeyframeModel::Direction::ALTERNATE_NORMAL,
            keyframe_model->GetDirection());
  EXPECT_EQ(1.0, keyframe_model->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time() * timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  ExpectKeyframeTimingFunctionCubic(*keyframes[0],
                                    CubicBezierTimingFunction::EaseType::EASE);

  EXPECT_EQ(0.5,
            keyframes[1]->Time() * timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0,
            keyframes[2]->Time() * timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.35f, keyframes[2]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[2], CubicBezierTimingFunction::EaseType::CUSTOM);

  EXPECT_EQ(2.0,
            keyframes[3]->Time() * timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.5f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[3]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest, CreateReversedOpacityAnimation) {
  scoped_refptr<TimingFunction> cubic_easy_flip_timing_function =
      CubicBezierTimingFunction::Create(0.0, 0.0, 0.0, 1.0);

  // KeyframeEffect to convert
  StringKeyframeVector frames;
  frames.push_back(CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0));
  frames.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.0", 0.25));
  frames.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.25", 0.5));
  frames.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));
  frames[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  frames[1]->SetEasing(linear_timing_function_.get());
  frames[2]->SetEasing(cubic_easy_flip_timing_function.get());
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(frames);

  timing_.timing_function = linear_timing_function_.get();
  timing_.iteration_count = 10;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(10.0, keyframe_model->Iterations());
  EXPECT_EQ(0, keyframe_model->TimeOffset());
  EXPECT_EQ(CompositorKeyframeModel::Direction::ALTERNATE_REVERSE,
            keyframe_model->GetDirection());
  EXPECT_EQ(1.0, keyframe_model->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(keyframed_float_curve->GetTimingFunctionForTesting()->GetType(),
            TimingFunction::Type::LINEAR);

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[0], CubicBezierTimingFunction::EaseType::EASE_IN);

  EXPECT_EQ(0.25, keyframes[1]->Time());
  EXPECT_EQ(0, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(0.5, keyframes[2]->Time());
  EXPECT_EQ(0.25f, keyframes[2]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[2], CubicBezierTimingFunction::EaseType::CUSTOM);

  EXPECT_EQ(1.0, keyframes[3]->Time());
  EXPECT_EQ(0.5f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[3]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateReversedOpacityAnimationNegativeStartDelay) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  const double kNegativeStartDelay = -3;

  timing_.iteration_count = 5.0;
  timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(1.5);
  timing_.start_delay = kNegativeStartDelay;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(5.0, keyframe_model->Iterations());
  EXPECT_EQ(-kNegativeStartDelay, keyframe_model->TimeOffset());
  EXPECT_EQ(CompositorKeyframeModel::Direction::ALTERNATE_REVERSE,
            keyframe_model->GetDirection());
  EXPECT_EQ(1.0, keyframe_model->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationFillModeNone) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.fill_mode = Timing::FillMode::NONE;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorKeyframeModel::FillMode::NONE,
            keyframe_model->GetFillMode());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationFillModeAuto) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.fill_mode = Timing::FillMode::AUTO;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::OPACITY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(1.0, keyframe_model->Iterations());
  EXPECT_EQ(0, keyframe_model->TimeOffset());
  EXPECT_EQ(CompositorKeyframeModel::Direction::NORMAL,
            keyframe_model->GetDirection());
  EXPECT_EQ(1.0, keyframe_model->PlaybackRate());
  EXPECT_EQ(CompositorKeyframeModel::FillMode::NONE,
            keyframe_model->GetFillMode());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationWithTimingFunction) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.timing_function = cubic_custom_timing_function_;

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  auto curve_timing_function =
      keyframed_float_curve->GetTimingFunctionForTesting();
  EXPECT_EQ(curve_timing_function->GetType(),
            TimingFunction::Type::CUBIC_BEZIER);
  const auto& cubic_timing_function =
      To<CubicBezierTimingFunction>(*curve_timing_function);
  EXPECT_EQ(cubic_timing_function.GetEaseType(),
            CubicBezierTimingFunction::EaseType::CUSTOM);
  EXPECT_EQ(cubic_timing_function.X1(), 1.0);
  EXPECT_EQ(cubic_timing_function.Y1(), 2.0);
  EXPECT_EQ(cubic_timing_function.X2(), 3.0);
  EXPECT_EQ(cubic_timing_function.Y2(), 4.0);

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time());
  EXPECT_EQ(0.5f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateCustomFloatPropertyAnimationWithNonAsciiName) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);

  String property_name = "--東京都";
  RegisterProperty(GetDocument(), property_name, "<number>", "0", false);
  SetCustomProperty(property_name, "10");

  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(property_name, "10", 0),
      CreateReplaceOpKeyframe(property_name, "20", 1.0));

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::CSS_CUSTOM_PROPERTY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(keyframe_model->GetCustomPropertyNameForTesting(),
            property_name.Utf8().data());
  EXPECT_FALSE(effect->HasNonVariableProperty());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleCustomFloatPropertyAnimation) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);

  RegisterProperty(GetDocument(), "--foo", "<number>", "0", false);
  SetCustomProperty("--foo", "10");

  StringKeyframeEffectModel* effect =
      CreateKeyframeEffectModel(CreateReplaceOpKeyframe("--foo", "10", 0),
                                CreateReplaceOpKeyframe("--foo", "20", 1.0));

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::CSS_CUSTOM_PROPERTY,
            keyframe_model->TargetProperty());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      keyframe_model->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(10, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time());
  EXPECT_EQ(20, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleCustomColorPropertyAnimation) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);

  RegisterProperty(GetDocument(), "--foo", "<color>", "rgb(0, 0, 0)", false);
  SetCustomProperty("--foo", "rgb(0, 0, 0)");

  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe("--foo", "rgb(0, 0, 0)", 0),
      CreateReplaceOpKeyframe("--foo", "rgb(0, 255, 0)", 1.0));

  std::unique_ptr<CompositorKeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(compositor_target_property::CSS_CUSTOM_PROPERTY,
            keyframe_model->TargetProperty());

  std::unique_ptr<CompositorColorAnimationCurve> keyframed_color_curve =
      keyframe_model->ColorCurveForTesting();

  CompositorColorAnimationCurve::Keyframes keyframes =
      keyframed_color_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time());
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());
}

TEST_P(AnimationCompositorAnimationsTest, MixedCustomPropertyAnimation) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);

  RegisterProperty(GetDocument(), "--foo", "<number> | <color>", "0", false);
  SetCustomProperty("--foo", "0");

  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe("--foo", "20", 0),
      CreateReplaceOpKeyframe("--foo", "rgb(0, 255, 0)", 1.0));

  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *effect) &
              CompositorAnimations::kMixedKeyframeValueTypes);
}

TEST_P(AnimationCompositorAnimationsTest,
       CancelIncompatibleCompositorAnimations) {
  Persistent<HeapVector<Member<StringKeyframe>>> key_frames =
      MakeGarbageCollected<HeapVector<Member<StringKeyframe>>>();
  key_frames->push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 0.0));
  key_frames->push_back(CreateDefaultKeyframe(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace, 1.0));
  KeyframeEffectModelBase* animation_effect1 =
      MakeGarbageCollected<StringKeyframeEffectModel>(*key_frames);
  KeyframeEffectModelBase* animation_effect2 =
      MakeGarbageCollected<StringKeyframeEffectModel>(*key_frames);

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);

  // The first animation for opacity is ok to run on compositor.
  auto* keyframe_effect1 = MakeGarbageCollected<KeyframeEffect>(
      element_.Get(), animation_effect1, timing);
  Animation* animation1 = timeline_->Play(keyframe_effect1);
  auto style = ComputedStyle::Create();
  animation_effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(),
                                                               *style, nullptr);
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing, *element_.Get(), animation1,
                                            *animation_effect1),
            CompositorAnimations::kNoFailure);

  // The second animation for opacity is not ok to run on compositor.
  auto* keyframe_effect2 = MakeGarbageCollected<KeyframeEffect>(
      element_.Get(), animation_effect2, timing);
  Animation* animation2 = timeline_->Play(keyframe_effect2);
  animation_effect2->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(),
                                                               *style, nullptr);
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing, *element_.Get(),
                                              animation2, *animation_effect2) &
              CompositorAnimations::kTargetHasIncompatibleAnimations);
  EXPECT_FALSE(animation2->HasActiveAnimationsOnCompositor());

  // A fallback to blink implementation needed, so cancel all compositor-side
  // opacity animations for this element.
  animation2->CancelIncompatibleAnimationsOnCompositor();

  EXPECT_FALSE(animation1->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation2->HasActiveAnimationsOnCompositor());

  SimulateFrame(0);
  EXPECT_EQ(2U, element_->GetElementAnimations()->Animations().size());

  // After finishing and collecting garbage there should be no
  // ElementAnimations on the element.
  SimulateFrame(1.);
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(element_->GetElementAnimations()->Animations().IsEmpty());
}

namespace {

void UpdateDummyTransformNode(ObjectPaintProperties& properties,
                              CompositingReasons reasons) {
  // Initialize with TransformationMatrix() to avoid 2d translation optimization
  // in case of transform animation.
  TransformPaintPropertyNode::State state{TransformationMatrix()};
  state.direct_compositing_reasons = reasons;
  properties.UpdateTransform(TransformPaintPropertyNode::Root(),
                             std::move(state));
}

void UpdateDummyEffectNode(ObjectPaintProperties& properties,
                           CompositingReasons reasons) {
  EffectPaintPropertyNode::State state;
  state.direct_compositing_reasons = reasons;
  properties.UpdateEffect(EffectPaintPropertyNode::Root(), std::move(state));
}

}  // namespace

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorTransformBasedOnPaintProperties) {
  Persistent<Element> element = GetDocument().CreateElementForBinding("shared");
  LayoutObjectProxy* layout_object = LayoutObjectProxy::Create(element.Get());
  layout_object->EnsureIdForTestingProxy();
  element->SetLayoutObject(layout_object);

  auto& properties = layout_object->GetMutableForPainting()
                         .FirstFragment()
                         .EnsurePaintProperties();

  // Add a transform with a compositing reason, which should allow starting
  // animation.
  UpdateDummyTransformNode(properties,
                           CompositingReason::kActiveTransformAnimation);
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*element, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);

  // Setting to CompositingReasonNone should produce false.
  UpdateDummyTransformNode(properties, CompositingReason::kNone);
  EXPECT_TRUE(
      CheckCanStartElementOnCompositor(*element, *keyframe_animation_effect2_) &
      CompositorAnimations::kTargetHasInvalidCompositingState);

  // Clearing the transform node entirely should also produce false.
  properties.ClearTransform();
  EXPECT_TRUE(
      CheckCanStartElementOnCompositor(*element, *keyframe_animation_effect2_) &
      CompositorAnimations::kTargetHasInvalidCompositingState);

  element->SetLayoutObject(nullptr);
  LayoutObjectProxy::Dispose(layout_object);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorEffectBasedOnPaintProperties) {
  Persistent<Element> element = GetDocument().CreateElementForBinding("shared");
  LayoutObjectProxy* layout_object = LayoutObjectProxy::Create(element.Get());
  layout_object->EnsureIdForTestingProxy();
  element->SetLayoutObject(layout_object);

  auto& properties = layout_object->GetMutableForPainting()
                         .FirstFragment()
                         .EnsurePaintProperties();

  // Add an effect with a compositing reason, which should allow starting
  // animation.
  UpdateDummyEffectNode(properties,
                        CompositingReason::kActiveTransformAnimation);
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*element, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);

  // Setting to CompositingReasonNone should produce false.
  UpdateDummyEffectNode(properties, CompositingReason::kNone);
  EXPECT_TRUE(
      CheckCanStartElementOnCompositor(*element, *keyframe_animation_effect2_) &
      CompositorAnimations::kTargetHasInvalidCompositingState);

  // Clearing the effect node entirely should also produce false.
  properties.ClearEffect();
  EXPECT_TRUE(
      CheckCanStartElementOnCompositor(*element, *keyframe_animation_effect2_) &
      CompositorAnimations::kTargetHasInvalidCompositingState);

  element->SetLayoutObject(nullptr);
  LayoutObjectProxy::Dispose(layout_object);
}

TEST_P(AnimationCompositorAnimationsTest, TrackRafAnimation) {
  LoadTestData("raf-countdown.html");

  cc::AnimationHost* host =
      GetFrame()->GetDocument()->View()->GetCompositorAnimationHost();

  // The test file registers two rAF 'animations'; one which ends after 5
  // iterations and the other that ends after 10.
  for (int i = 0; i < 9; i++) {
    BeginFrame();
    EXPECT_TRUE(host->CurrentFrameHadRAF());
    EXPECT_TRUE(host->NextFrameHasPendingRAF());
  }

  // On the 10th iteration, there should be a current rAF, but no more pending
  // rAFs.
  BeginFrame();
  EXPECT_TRUE(host->CurrentFrameHadRAF());
  EXPECT_FALSE(host->NextFrameHasPendingRAF());

  // On the 11th iteration, there should be no more rAFs firing.
  BeginFrame();
  EXPECT_FALSE(host->CurrentFrameHadRAF());
  EXPECT_FALSE(host->NextFrameHasPendingRAF());
}

TEST_P(AnimationCompositorAnimationsTest, TrackRafAnimationTimeout) {
  LoadTestData("raf-timeout.html");

  cc::AnimationHost* host =
      GetFrame()->GetDocument()->View()->GetCompositorAnimationHost();

  // The test file executes a rAF, which fires a setTimeout for the next rAF.
  // Even with setTimeout(func, 0), the next rAF is not considered pending.
  BeginFrame();
  EXPECT_TRUE(host->CurrentFrameHadRAF());
  EXPECT_FALSE(host->NextFrameHasPendingRAF());
}

TEST_P(AnimationCompositorAnimationsTest, TrackRafAnimationNoneRegistered) {
  SetBodyInnerHTML("<div id='box'></div>");

  // Run a full frame after loading the test data so that scripted animations
  // are serviced and data propagated.
  BeginFrame();

  // The HTML does not have any rAFs.
  cc::AnimationHost* host =
      GetFrame()->GetDocument()->View()->GetCompositorAnimationHost();
  EXPECT_FALSE(host->CurrentFrameHadRAF());
  EXPECT_FALSE(host->NextFrameHasPendingRAF());

  // And still shouldn't after another frame.
  BeginFrame();
  EXPECT_FALSE(host->CurrentFrameHadRAF());
  EXPECT_FALSE(host->NextFrameHasPendingRAF());
}

TEST_P(AnimationCompositorAnimationsTest, CompositedCustomProperty) {
  RegisterProperty(GetDocument(), "--foo", "<number>", "0", false);
  SetCustomProperty("--foo", "0");
  StringKeyframeEffectModel* effect =
      CreateKeyframeEffectModel(CreateReplaceOpKeyframe("--foo", "20", 0),
                                CreateReplaceOpKeyframe("--foo", "100", 1.0));
  LoadTestData("custom-property.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById("target");
  // Make sure the animation is started on the compositor.
  EXPECT_EQ(CheckCanStartElementOnCompositor(*target, *effect),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest, CompositedTransformAnimation) {
  // TODO(wangxianzhu): Fix this test for CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  LoadTestData("transform-animation.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById("target");
  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_NE(nullptr, properties);
  const auto* transform = properties->Transform();
  ASSERT_NE(nullptr, transform);
  EXPECT_TRUE(transform->HasDirectCompositingReasons());
  EXPECT_TRUE(transform->HasActiveTransformAnimation());

  // Make sure the animation state is initialized in paint properties.
  auto* property_trees =
      document->View()->RootCcLayer()->layer_tree_host()->property_trees();
  auto* cc_transform = property_trees->transform_tree.Node(
      property_trees->element_id_to_transform_node_index
          [transform->GetCompositorElementId()]);
  ASSERT_NE(nullptr, cc_transform);
  EXPECT_TRUE(cc_transform->has_potential_animation);
  EXPECT_TRUE(cc_transform->is_currently_animating);
  EXPECT_EQ(cc::kNotScaled, cc_transform->starting_animation_scale);
  EXPECT_EQ(cc::kNotScaled, cc_transform->maximum_animation_scale);

  // Make sure the animation is started on the compositor.
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
  EXPECT_EQ(document->Timeline().AnimationsNeedingUpdateCount(), 1u);
}

TEST_P(AnimationCompositorAnimationsTest, CompositedScaleAnimation) {
  // TODO(wangxianzhu): Fix this test for CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  LoadTestData("scale-animation.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById("target");
  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_NE(nullptr, properties);
  const auto* transform = properties->Transform();
  ASSERT_NE(nullptr, transform);
  EXPECT_TRUE(transform->HasDirectCompositingReasons());
  EXPECT_TRUE(transform->HasActiveTransformAnimation());

  // Make sure the animation state is initialized in paint properties.
  auto* property_trees =
      document->View()->RootCcLayer()->layer_tree_host()->property_trees();
  auto* cc_transform = property_trees->transform_tree.Node(
      property_trees->element_id_to_transform_node_index
          [transform->GetCompositorElementId()]);
  ASSERT_NE(nullptr, cc_transform);
  EXPECT_TRUE(cc_transform->has_potential_animation);
  EXPECT_TRUE(cc_transform->is_currently_animating);
  EXPECT_EQ(2.f, cc_transform->starting_animation_scale);
  EXPECT_EQ(5.f, cc_transform->maximum_animation_scale);

  // Make sure the animation is started on the compositor.
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
  EXPECT_EQ(document->Timeline().AnimationsNeedingUpdateCount(), 1u);
}

TEST_P(AnimationCompositorAnimationsTest,
       NonAnimatedTransformPropertyChangeGetsUpdated) {
  // TODO(wangxianzhu): Fix this test for CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  LoadTestData("transform-animation-update.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById("target");
  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_NE(nullptr, properties);
  const auto* transform = properties->Transform();
  ASSERT_NE(nullptr, transform);
  // Make sure composited animation is running on #target.
  EXPECT_TRUE(transform->HasDirectCompositingReasons());
  EXPECT_TRUE(transform->HasActiveTransformAnimation());
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
  // Make sure the animation state is initialized in paint properties.
  auto* property_trees =
      document->View()->RootCcLayer()->layer_tree_host()->property_trees();
  auto* cc_transform = property_trees->transform_tree.Node(
      property_trees->element_id_to_transform_node_index
          [transform->GetCompositorElementId()]);
  ASSERT_NE(nullptr, cc_transform);
  EXPECT_TRUE(cc_transform->has_potential_animation);
  EXPECT_TRUE(cc_transform->is_currently_animating);
  // Make sure the animation is started on the compositor.
  EXPECT_EQ(document->Timeline().AnimationsNeedingUpdateCount(), 1u);
  // Make sure the backface-visibility is correctly set, both in blink and on
  // the cc::Layer.
  EXPECT_FALSE(transform->Matrix().IsIdentity());  // Rotated
  EXPECT_EQ(transform->GetBackfaceVisibilityForTesting(),
            TransformPaintPropertyNode::BackfaceVisibility::kVisible);
  const CompositedLayerMapping* composited_layer_mapping =
      ToLayoutBoxModelObject(target->GetLayoutObject())
          ->Layer()
          ->GetCompositedLayerMapping();
  ASSERT_NE(nullptr, composited_layer_mapping);
  const auto& layer = composited_layer_mapping->MainGraphicsLayer()->CcLayer();
  EXPECT_FALSE(layer.should_check_backface_visibility());

  // Change the backface visibility, while the compositor animation is
  // happening.
  target->setAttribute(html_names::kClassAttr, "backface-hidden");
  ForceFullCompositingUpdate();
  // Make sure the setting made it to both blink and all the way to CC.
  EXPECT_EQ(transform->GetBackfaceVisibilityForTesting(),
            TransformPaintPropertyNode::BackfaceVisibility::kHidden);
  EXPECT_TRUE(layer.should_check_backface_visibility())
      << "Change to hidden did not get propagated to CC";
  // Make sure the animation state is initialized in paint properties after
  // blink pushing new paint properties without animation state change.
  property_trees =
      document->View()->RootCcLayer()->layer_tree_host()->property_trees();
  cc_transform = property_trees->transform_tree.Node(
      property_trees->element_id_to_transform_node_index
          [transform->GetCompositorElementId()]);
  ASSERT_NE(nullptr, cc_transform);
  EXPECT_TRUE(cc_transform->has_potential_animation);
  EXPECT_TRUE(cc_transform->is_currently_animating);
}

// Regression test for https://crbug.com/781305. When we have a transform
// animation on a SVG element, the effect can be started on compositor but the
// element itself cannot.
TEST_P(AnimationCompositorAnimationsTest,
       CannotStartElementOnCompositorEffectSVG) {
  LoadTestData("transform-animation-on-svg.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById("dots");
  EXPECT_TRUE(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_) &
      CompositorAnimations::kTargetHasInvalidCompositingState);
  EXPECT_EQ(document->Timeline().AnimationsNeedingUpdateCount(), 4u);
}

// Regression test for https://crbug.com/999333. We were relying on the Document
// always having Settings, which will not be the case if it is not attached to a
// Frame.
TEST_P(AnimationCompositorAnimationsTest,
       DocumentWithoutSettingShouldNotCauseCrash) {
  SetBodyInnerHTML("<div id='target'></div>");
  Element* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Move the target element to another Document, that does not have a frame
  // (and thus no Settings).
  Document* another_document = Document::CreateForTest();
  ASSERT_FALSE(another_document->GetSettings());

  another_document->adoptNode(target, ASSERT_NO_EXCEPTION);

  // This should not crash.
  EXPECT_NE(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
}

}  // namespace blink
