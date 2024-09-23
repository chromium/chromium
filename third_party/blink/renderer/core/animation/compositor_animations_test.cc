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

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_model.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/mock_css_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/geometry/size.h"

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

    keyframe_vector2_ = CreateCompositableFloatKeyframeVector(2);
    keyframe_animation_effect2_ =
        MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector2_);

    keyframe_vector5_ = CreateCompositableFloatKeyframeVector(5);
    keyframe_animation_effect5_ =
        MakeGarbageCollected<StringKeyframeEffectModel>(*keyframe_vector5_);

    GetAnimationClock().ResetTimeForTesting();

    timeline_ = GetDocument().Timeline();
    timeline_->ResetForTesting();

    // Make sure the CompositableTiming is really compositable, otherwise
    // most other tests will fail.
    ASSERT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));

    // Using will-change ensures that this object will need paint properties.
    // Having an animation would normally ensure this but these tests don't
    // explicitly construct a full animation on the element.
    SetBodyInnerHTML(R"HTML(
      <div id='test' style='will-change: opacity,filter,transform,rotate;
                            height:100px; background: green;'>
      </div>
      <span id='inline' style='will-change: opacity,filter,transform;'>
        text
      </span>
    )HTML");
    element_ = GetDocument().getElementById(AtomicString("test"));
    inline_ = GetDocument().getElementById(AtomicString("inline"));

    helper_.Initialize(nullptr, nullptr, nullptr);
    helper_.Resize(gfx::Size(800, 600));
    base_url_ = "http://www.test.com/";
  }

 public:
  AnimationCompositorAnimationsTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  bool ConvertTimingForCompositor(const Timing& t,
                                  CompositorAnimations::CompositorTiming& out,
                                  double playback_rate = 1) {
    return CompositorAnimations::ConvertTimingForCompositor(
        t, NormalizedTiming(t), base::TimeDelta(), out, playback_rate);
  }

  CompositorAnimations::FailureReasons CanStartEffectOnCompositor(
      const Timing& timing,
      const KeyframeEffectModelBase& effect) {
    // TODO(crbug.com/725385): Remove once compositor uses InterpolationTypes.
    const auto* style = GetDocument().GetStyleResolver().ResolveStyle(
        element_, StyleRecalcContext());
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
        timing, NormalizedTiming(timing), element, animation, effect_model,
        paint_artifact_compositor, 1, unsupported_properties);
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
      Vector<std::unique_ptr<cc::KeyframeModel>>& keyframe_models,
      double animation_playback_rate) {
    CompositorAnimations::GetAnimationOnCompositor(
        *element_, timing, NormalizedTiming(timing), 0, std::nullopt,
        base::TimeDelta(), effect, keyframe_models, animation_playback_rate,
        /*is_monotonic_timeline=*/true, /*is_boundary_aligned=*/false);
  }

  CompositorAnimations::FailureReasons
  CreateKeyframeListAndTestIsCandidateOnResult(StringKeyframe* first_frame,
                                               StringKeyframe* second_frame) {
    EXPECT_EQ(first_frame->CheckedOffset(), 0);
    EXPECT_EQ(second_frame->CheckedOffset(), 1);
    StringKeyframeVector frames;
    frames.push_back(first_frame);
    frames.push_back(second_frame);
    return CanStartEffectOnCompositor(
        timing_, *MakeGarbageCollected<StringKeyframeEffectModel>(frames));
  }

  CompositorAnimations::FailureReasons CheckKeyframeVector(
      const StringKeyframeVector& frames) {
    return CanStartEffectOnCompositor(
        timing_, *MakeGarbageCollected<StringKeyframeEffectModel>(frames));
  }

  // -------------------------------------------------------------------

  Timing CreateCompositableTiming() {
    Timing timing;
    timing.start_delay = Timing::Delay(AnimationTimeDelta());
    timing.fill_mode = Timing::FillMode::NONE;
    timing.iteration_start = 0;
    timing.iteration_count = 1;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
    timing.direction = Timing::PlaybackDirection::NORMAL;
    timing.timing_function = linear_timing_function_;
    return timing;
  }

  // Simplified version of what happens in AnimationEffect::NormalizedTiming()
  Timing::NormalizedTiming NormalizedTiming(Timing timing) {
    Timing::NormalizedTiming normalized_timing;

    // Currently, compositor animation tests are using document timelines
    // exclusively. In order to support scroll timelines, the algorithm would
    // need to correct for the intrinsic iteration duration of the timeline.
    EXPECT_TRUE(timeline_->IsDocumentTimeline());

    normalized_timing.start_delay = timing.start_delay.AsTimeValue();
    normalized_timing.end_delay = timing.end_delay.AsTimeValue();

    normalized_timing.iteration_duration =
        timing.iteration_duration.value_or(AnimationTimeDelta());

    normalized_timing.active_duration =
        normalized_timing.iteration_duration * timing.iteration_count;

    normalized_timing.end_time = std::max(
        normalized_timing.start_delay + normalized_timing.active_duration +
            normalized_timing.end_delay,
        AnimationTimeDelta());

    return normalized_timing;
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

  StringKeyframeVector CreateDefaultKeyframeVector(
      CSSPropertyID id,
      EffectModel::CompositeOperation op) {
    StringKeyframeVector results;
    String first, second;
    switch (id) {
      case CSSPropertyID::kOpacity:
        first = "0.1";
        second = "1";
        break;

      case CSSPropertyID::kTransform:
        first = "none";
        second = "scale(1)";
        break;

      case CSSPropertyID::kColor:
        first = "red";
        second = "green";
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }

    StringKeyframe* keyframe = CreateReplaceOpKeyframe(id, first, 0);
    keyframe->SetComposite(op);
    results.push_back(keyframe);
    keyframe = CreateReplaceOpKeyframe(id, second, 1);
    keyframe->SetComposite(op);
    results.push_back(keyframe);
    return results;
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

  bool IsUseCounted(mojom::WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }

  void ClearUseCounters() {
    GetDocument().ClearUseCounterForTesting(
        WebFeature::kStaticPropertyInAnimation);
    // If other use counters are test, be sure the clear them here.
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
      return property_specific_.Get();  // We know a shortcut.
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
      bool IsRevertLayer() const final { return false; }
      PropertySpecificKeyframe* CloneWithOffset(double) const final {
        NOTREACHED_IN_MIGRATION();
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
        return compositor_keyframe_value_.Get();
      }
      PropertySpecificKeyframe* NeutralKeyframe(
          double,
          scoped_refptr<TimingFunction>) const final {
        NOTREACHED_IN_MIGRATION();
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
    GetAnimationClock().UpdateTime(base::TimeTicks() + base::Seconds(time));
    timeline_->ServiceAnimations(kTimingUpdateForAnimationFrame);
    GetPendingAnimations().Update(nullptr, false);
  }

  std::unique_ptr<cc::KeyframeModel> ConvertToCompositorAnimation(
      StringKeyframeEffectModel& effect,
      double animation_playback_rate) {
    // As the compositor code only understands CompositorKeyframeValues, we must
    // snapshot the effect to make those available.
    // TODO(crbug.com/725385): Remove once compositor uses InterpolationTypes.
    const auto* style = GetDocument().GetStyleResolver().ResolveStyle(
        element_, StyleRecalcContext());
    effect.SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), *style,
                                                     nullptr);

    Vector<std::unique_ptr<cc::KeyframeModel>> result;
    GetAnimationOnCompositor(timing_, effect, result, animation_playback_rate);
    DCHECK_EQ(1U, result.size());
    return std::move(result[0]);
  }

  std::unique_ptr<cc::KeyframeModel> ConvertToCompositorAnimation(
      StringKeyframeEffectModel& effect) {
    return ConvertToCompositorAnimation(effect, 1.0);
  }

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve>
  CreateKeyframedFloatAnimationCurve(cc::KeyframeModel* keyframe_model) {
    const gfx::AnimationCurve* curve = keyframe_model->curve();
    DCHECK_EQ(gfx::AnimationCurve::FLOAT, curve->Type());

    return base::WrapUnique(static_cast<gfx::KeyframedFloatAnimationCurve*>(
        curve->Clone().release()));
  }

  std::unique_ptr<gfx::KeyframedColorAnimationCurve>
  CreateKeyframedColorAnimationCurve(cc::KeyframeModel* keyframe_model) const {
    const gfx::AnimationCurve* curve = keyframe_model->curve();
    DCHECK_EQ(gfx::AnimationCurve::COLOR, curve->Type());

    return base::WrapUnique(static_cast<gfx::KeyframedColorAnimationCurve*>(
        curve->Clone().release()));
  }

  void ExpectKeyframeTimingFunctionCubic(
      const gfx::FloatKeyframe& keyframe,
      const CubicBezierTimingFunction::EaseType ease_type) {
    auto keyframe_timing_function =
        CreateCompositorTimingFunctionFromCC(keyframe.timing_function());
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
    return MakeGarbageCollected<LayoutObjectProxy>(node);
  }

  static void Dispose(LayoutObjectProxy* proxy) { proxy->Destroy(); }

  const char* GetName() const override { return nullptr; }
  gfx::RectF LocalBoundingBoxRectForAccessibility() const override {
    return gfx::RectF();
  }

  void EnsureIdForTestingProxy() {
    // We need Ids of proxies to be valid.
    EnsureIdForTesting();
  }

  explicit LayoutObjectProxy(Node* node) : LayoutObject(node) {}
};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

INSTANTIATE_PAINT_TEST_SUITE_P(AnimationCompositorAnimationsTest);

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorKeyframeMultipleCSSProperties) {
  StringKeyframeVector supported_mixed_keyframe_vector;
  auto* keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kTransform, "none",
                                SecureContextMode::kInsecureContext, nullptr);

  supported_mixed_keyframe_vector.push_back(keyframe);
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kTransform, "scale(1, 1)",
                                SecureContextMode::kInsecureContext, nullptr);
  supported_mixed_keyframe_vector.push_back(keyframe);
  EXPECT_EQ(CheckKeyframeVector(supported_mixed_keyframe_vector),
            CompositorAnimations::kNoFailure);

  StringKeyframeVector unsupported_mixed_keyframe_vector;
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kColor, "red",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  unsupported_mixed_keyframe_vector.push_back(keyframe);
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kColor, "green",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  unsupported_mixed_keyframe_vector.push_back(keyframe);
  EXPECT_TRUE(CheckKeyframeVector(unsupported_mixed_keyframe_vector) &
              CompositorAnimations::kUnsupportedCSSProperty);

  StringKeyframeVector supported_mixed_keyframe_vector_static_color;
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kColor, "red",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  supported_mixed_keyframe_vector_static_color.push_back(keyframe);
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kColor, "red",
                                SecureContextMode::kInsecureContext, nullptr);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  supported_mixed_keyframe_vector_static_color.push_back(keyframe);
  EXPECT_EQ(CheckKeyframeVector(supported_mixed_keyframe_vector_static_color),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorKeyframeEffectModel) {
  StringKeyframeVector frames_same;

  frames_same.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kColor, EffectModel::kCompositeReplace, 0.0));
  frames_same.push_back(CreateDefaultKeyframe(
      CSSPropertyID::kColor, EffectModel::kCompositeReplace, 1.0));
  EXPECT_TRUE(CheckKeyframeVector(frames_same) &
              CompositorAnimations::kAnimationHasNoVisibleChange);

  StringKeyframeVector color_keyframes = CreateDefaultKeyframeVector(
      CSSPropertyID::kColor, EffectModel::kCompositeReplace);
  EXPECT_TRUE(CheckKeyframeVector(color_keyframes) &
              CompositorAnimations::kUnsupportedCSSProperty);

  StringKeyframeVector opacity_keyframes = CreateDefaultKeyframeVector(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace);
  EXPECT_EQ(CheckKeyframeVector(opacity_keyframes),
            CompositorAnimations::kNoFailure);
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
  const auto* style = GetDocument().GetStyleResolver().ResolveStyle(
      element_, StyleRecalcContext());
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
  EXPECT_TRUE(style->GetVariableData(AtomicString("--y")));
  EXPECT_TRUE(style->GetVariableData(AtomicString("--z")));

  NiceMock<MockCSSPaintImageGenerator>* mock_generator =
      MakeGarbageCollected<NiceMock<MockCSSPaintImageGenerator>>();
  base::AutoReset<MockCSSPaintImageGenerator*> scoped_override_generator(
      &g_override_generator, mock_generator);
  base::AutoReset<CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction>
      scoped_create_function(
          CSSPaintImageGenerator::GetCreateFunctionForTesting(),
          ProvideOverrideGenerator);

  mock_generator->AddCustomProperty(AtomicString("--foo"));
  mock_generator->AddCustomProperty(AtomicString("--bar"));
  mock_generator->AddCustomProperty(AtomicString("--loo"));
  mock_generator->AddCustomProperty(AtomicString("--y"));
  mock_generator->AddCustomProperty(AtomicString("--z"));
  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("foopainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  paint_value->CreateGeneratorForTesting(GetDocument());
  StyleGeneratedImage* style_image = MakeGarbageCollected<StyleGeneratedImage>(
      *paint_value, StyleGeneratedImage::ContainerSizes());

  ComputedStyleBuilder builder(*style);
  builder.AddPaintImage(style_image);
  element_->GetLayoutObject()->SetStyle(builder.TakeStyle());

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
  StringKeyframe* keyframe1 = CreateReplaceOpKeyframe("--foo", "10", 0);
  StringKeyframe* keyframe2 = CreateReplaceOpKeyframe("--foo", "20", 1);
  EXPECT_EQ(CreateKeyframeListAndTestIsCandidateOnResult(keyframe1, keyframe2),
            CompositorAnimations::kNoFailure);

  // Color-valued properties are supported
  StringKeyframe* color_keyframe1 =
      CreateReplaceOpKeyframe("--loo", "rgb(0, 255, 0)", 0);
  StringKeyframe* color_keyframe2 =
      CreateReplaceOpKeyframe("--loo", "rgb(0, 0, 255)", 1);
  EXPECT_EQ(CreateKeyframeListAndTestIsCandidateOnResult(color_keyframe1,
                                                         color_keyframe2),
            CompositorAnimations::kNoFailure);

  // Length-valued properties are not compositable.
  StringKeyframe* non_animatable_keyframe1 =
      CreateReplaceOpKeyframe("--bar", "10px", 0);
  StringKeyframe* non_animatable_keyframe2 =
      CreateReplaceOpKeyframe("--bar", "20px", 1);
  EXPECT_TRUE(CreateKeyframeListAndTestIsCandidateOnResult(
                  non_animatable_keyframe1, non_animatable_keyframe2) &
              CompositorAnimations::kUnsupportedCSSProperty);

  // Cannot composite due to side effect.
  SetCustomProperty("opacity", "var(--foo)");
  EXPECT_TRUE(
      CreateKeyframeListAndTestIsCandidateOnResult(keyframe1, keyframe2) &
      CompositorAnimations::kUnsupportedCSSProperty);

  // Cannot composite because "--x" is not used by the paint worklet.
  StringKeyframe* non_used_keyframe1 = CreateReplaceOpKeyframe("--x", "5", 0);
  StringKeyframe* non_used_keyframe2 = CreateReplaceOpKeyframe("--x", "15", 1);

  EXPECT_EQ(CreateKeyframeListAndTestIsCandidateOnResult(non_used_keyframe1,
                                                         non_used_keyframe2),
            CompositorAnimations::kUnsupportedCSSProperty);

  // Implicitly initial values are supported.
  StringKeyframe* y_keyframe = CreateReplaceOpKeyframe("--y", "1000", 1);
  StringKeyframeVector keyframe_vector;
  keyframe_vector.push_back(y_keyframe);
  EXPECT_EQ(CheckKeyframeVector(keyframe_vector),
            CompositorAnimations::kNoFailure);

  // Implicitly initial values are not supported when the property
  // has been referenced.
  SetCustomProperty("opacity", "var(--z)");
  StringKeyframe* z_keyframe = CreateReplaceOpKeyframe("--z", "1000", 1);
  StringKeyframeVector keyframe_vector2;
  keyframe_vector2.push_back(z_keyframe);
  EXPECT_EQ(CheckKeyframeVector(keyframe_vector2),
            CompositorAnimations::kUnsupportedCSSProperty);
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorStartDelay) {
  const double play_forward = 1;
  const double play_reverse = -1;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(20);

  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(2.0));
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_forward));
  EXPECT_DOUBLE_EQ(-2.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_reverse));
  EXPECT_DOUBLE_EQ(0.0, compositor_timing_.scaled_time_offset.InSecondsF());

  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-2.0));
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_forward));
  EXPECT_DOUBLE_EQ(2.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_TRUE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_reverse));
  EXPECT_DOUBLE_EQ(0.0, compositor_timing_.scaled_time_offset.InSecondsF());

  // Stress test with an effectively infinite start delay.
  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1e19));
  EXPECT_FALSE(
      ConvertTimingForCompositor(timing_, compositor_timing_, play_forward));
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
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(5);
  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-6.0));
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            compositor_timing_.adjusted_iteration_count);
}

TEST_P(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationsAndStartDelay) {
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(5);

  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(6.0));
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(-6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_DOUBLE_EQ(4.0, compositor_timing_.adjusted_iteration_count);

  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-6.0));
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_DOUBLE_EQ(4.0, compositor_timing_.adjusted_iteration_count);

  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(21.0));
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
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(5);
  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-6.0));
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(5);
  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-11.0));
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(11.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(5);
  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-6.0));
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset.InSecondsF());
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(5);
  timing_.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(-11.0));
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
  StringKeyframeVector key_frames = CreateDefaultKeyframeVector(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace);
  KeyframeEffectModelBase* animation_effect =
      MakeGarbageCollected<StringKeyframeEffectModel>(key_frames);

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);

  // The first animation for opacity is ok to run on compositor.
  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_, animation_effect, timing);
  Animation* animation = timeline_->Play(keyframe_effect1);
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();
  animation_effect->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(),
                                                              style, nullptr);

  // Now we can check that we are set up correctly.
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing, *element_.Get(), animation,
                                            *animation_effect),
            CompositorAnimations::kNoFailure);

  // Timings have to be convertible for compositor.
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing, *element_.Get(), animation,
                                            *animation_effect),
            CompositorAnimations::kNoFailure);
  timing.end_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1.0));
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing, *element_.Get(),
                                              animation, *animation_effect) &
              CompositorAnimations::kEffectHasUnsupportedTimingParameters);
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing, *element_.Get(),
                                              animation, *animation_effect) &
              (CompositorAnimations::kTargetHasInvalidCompositingState |
               CompositorAnimations::kEffectHasUnsupportedTimingParameters));
}

TEST_P(AnimationCompositorAnimationsTest, ForceReduceMotion) {
  ScopedForceReduceMotionForTest force_reduce_motion(true);
  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes slide {
        0% { transform: translateX(100px); }
        50% { transform: translateX(200px); }
        100% { transform: translateX(300px); }
      }
      html, body {
        margin: 0;
      }
    </style>
    <div id='test' style='animation: slide 2s linear'></div>
  )HTML");
  element_ = GetDocument().getElementById(AtomicString("test"));
  Animation* animation = element_->getAnimations()[0];

  // The effect should snap between keyframes at the halfway points.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(450),
                            ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(element_->GetBoundingClientRect()->x(), 100.0, 0.001);
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(550),
                            ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(element_->GetBoundingClientRect()->x(), 200.0, 0.001);
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(1450),
                            ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(element_->GetBoundingClientRect()->x(), 200.0, 0.001);
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(1550),
                            ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(element_->GetBoundingClientRect()->x(), 300.0, 0.001);
}

TEST_P(AnimationCompositorAnimationsTest,
       ForceReduceMotionDocumentSupportsReduce) {
  ScopedForceReduceMotionForTest force_reduce_motion(true);
  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  SetBodyInnerHTML(R"HTML(
    <meta name='supports-reduced-motion' content='reduce'>
    <style>
      @keyframes slide {
        0% { transform: translateX(100px); }
        100% { transform: translateX(200px); }
      }
      html, body {
        margin: 0;
      }
    </style>
    <div id='test' style='animation: slide 1s linear'></div>
  )HTML");
  element_ = GetDocument().getElementById(AtomicString("test"));
  Animation* animation = element_->getAnimations()[0];

  // As the page has indicated support for reduce motion, the effect should not
  // jump to the nearest keyframe.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(500),
                            ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(element_->GetBoundingClientRect()->x(), 150.0, 0.001);
}

TEST_P(AnimationCompositorAnimationsTest,
       ForceReduceMotionChildDocumentSupportsReduce) {
  ScopedForceReduceMotionForTest force_reduce_motion(true);
  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  SetBodyInnerHTML(R"HTML(
    <iframe></iframe>
    <style>
      @keyframes slide {
        0% { transform: translateX(100px); }
        100% { transform: translateX(200px); }
      }
      html, body {
        margin: 0;
      }
    </style>
    <div id='parent-anim' style='animation: slide 1s linear'></div>
    )HTML");
  SetChildFrameHTML(R"HTML(
    <meta name='supports-reduced-motion' content='reduce'>
    <style>
      @keyframes slide {
        0% { transform: translateX(100px); }
        100% { transform: translateX(200px); }
      }
      html, body {
        margin: 0;
      }
    </style>
    <div id='child-anim' style='animation: slide 1s linear'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  element_ = GetDocument().getElementById(AtomicString("parent-anim"));
  Animation* animation = element_->getAnimations()[0];

  // As the parent document does not support reduce motion, the effect will jump
  // to the nearest keyframe.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(400),
                            ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(element_->GetBoundingClientRect()->x(), 100.0, 0.001);

  // As the child document does support reduce motion, its animation will not be
  // snapped.
  Element* child_element =
      ChildDocument().getElementById(AtomicString("child-anim"));
  Animation* child_animation = child_element->getAnimations()[0];
  child_animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(400),
                                  ASSERT_NO_EXCEPTION);
  EXPECT_NEAR(child_element->GetBoundingClientRect()->x(), 140.0, 0.001);
}

TEST_P(AnimationCompositorAnimationsTest, CheckCanStartForceReduceMotion) {
  ScopedForceReduceMotionForTest force_reduce_motion(true);
  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateX(100px)"),
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateX(200px)",
                              1));

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(2);
  auto* keyframe_effect =
      MakeGarbageCollected<KeyframeEffect>(element_, effect, timing);
  Animation* animation = timeline_->Play(keyframe_effect);
  // The animation should not run on the compositor since we are forcing reduced
  // motion.
  EXPECT_NE(CheckCanStartEffectOnCompositor(timing_, *element_.Get(), animation,
                                            *effect),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartElementOnCompositorEffectInvalid) {
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();

  // Check that we notice the value is not animatable correctly.
  const CSSProperty& target_property1(GetCSSPropertyOutlineStyle());
  PropertyHandle target_property1h(target_property1);
  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(target_property1.PropertyID(), "dotted", 0),
      CreateReplaceOpKeyframe(target_property1.PropertyID(), "dashed", 1.0));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
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
  effect2->SnapshotAllCompositorKeyframesIfNecessary(*inline_.Get(), style,
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
  effect3->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
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
  // Filter Properties use a different ID namespace
  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "none", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kFilter, "sepia(50%)", 1.0));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
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
  effect2->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
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
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();

  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "none", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "rotate(45deg)", 1.0));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
                                                     nullptr);

  // Check if our layout object is not TransformApplicable
  EXPECT_TRUE(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(),
                                              animation1, *effect1) &
              CompositorAnimations::
                  kTransformRelatedPropertyCannotBeAcceleratedOnTarget);
}

TEST_P(AnimationCompositorAnimationsTest,
       CheckCanStartEffectOnCompositorUnsupportedCSSProperties) {
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();

  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "1", 1));

  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);

  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
                                                     nullptr);

  // Make sure supported properties do not register a failure
  PropertyHandleSet unsupported_properties1;
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing_, *inline_.Get(), animation1,
                                            *effect1, &unsupported_properties1),
            CompositorAnimations::kNoFailure);
  EXPECT_TRUE(unsupported_properties1.empty());

  StringKeyframeEffectModel* effect2 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kHeight, "100px", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kHeight, "200px", 1));

  auto* keyframe_effect2 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect2, timing_);

  Animation* animation2 = timeline_->Play(keyframe_effect2);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
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
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
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
  StringKeyframeVector basic_frames_vector = CreateDefaultKeyframeVector(
      CSSPropertyID::kOpacity, EffectModel::kCompositeReplace);

  StringKeyframeVector non_basic_frames_vector;
  non_basic_frames_vector.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0", 0));
  non_basic_frames_vector.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 0.5));
  non_basic_frames_vector.push_back(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "1", 1));

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

  StringKeyframeVector non_allowed_frames_vector = CreateDefaultKeyframeVector(
      CSSPropertyID::kOpacity, EffectModel::kCompositeAdd);
  auto* non_allowed_frames = MakeGarbageCollected<StringKeyframeEffectModel>(
      non_allowed_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *non_allowed_frames) &
              CompositorAnimations::kEffectHasNonReplaceCompositeMode);

  // Set SVGAttribute keeps a pointer to this thing for the lifespan of
  // the Keyframe.  This is ugly but sufficient to work around it.
  QualifiedName fake_name(AtomicString("prefix"), AtomicString("local"),
                          AtomicString("uri"));

  StringKeyframeVector non_css_frames_vector;
  non_css_frames_vector.push_back(CreateSVGKeyframe(fake_name, "cargo", 0.0));
  non_css_frames_vector.push_back(CreateSVGKeyframe(fake_name, "Fargo", 1.0));
  auto* non_css_frames =
      MakeGarbageCollected<StringKeyframeEffectModel>(non_css_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *non_css_frames) &
              CompositorAnimations::kAnimationAffectsNonCSSProperties);
  EXPECT_TRUE(non_css_frames->RequiresPropertyNode());
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

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(1.0, keyframe_model->iterations());
  EXPECT_EQ(0, keyframe_model->time_offset().InSecondsF());
  EXPECT_EQ(cc::KeyframeModel::Direction::NORMAL, keyframe_model->direction());
  EXPECT_EQ(1.0, keyframe_model->playback_rate());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[0]->timing_function())
                                              ->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time().InSecondsF());
  EXPECT_EQ(0.5f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationDuration) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  const AnimationTimeDelta kDuration = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  timing_.iteration_duration = kDuration;

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(kDuration, keyframes[1]->Time().InSecondsF() * kDuration);
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

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect, 2.0);
  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(5.0, keyframe_model->iterations());
  EXPECT_EQ(0, keyframe_model->time_offset().InSecondsF());
  EXPECT_EQ(cc::KeyframeModel::Direction::ALTERNATE_NORMAL,
            keyframe_model->direction());
  EXPECT_EQ(2.0, keyframe_model->playback_rate());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[0]->timing_function())
                                              ->GetType());

  EXPECT_EQ(0.25, keyframes[1]->Time().InSecondsF());
  EXPECT_EQ(0, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());

  EXPECT_EQ(0.5, keyframes[2]->Time().InSecondsF());
  EXPECT_EQ(0.25f, keyframes[2]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[2]->timing_function())
                                              ->GetType());

  EXPECT_EQ(1.0, keyframes[3]->Time().InSecondsF());
  EXPECT_EQ(0.5f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[3]->timing_function())
                                              ->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationStartDelay) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  const double kStartDelay = 3.25;

  timing_.iteration_count = 5.0;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1.75);
  timing_.start_delay =
      Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(kStartDelay));

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);

  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(5.0, keyframe_model->iterations());
  EXPECT_EQ(-kStartDelay, keyframe_model->time_offset().InSecondsF());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(1.75, keyframes[1]->Time().InSecondsF() *
                      timing_.iteration_duration->InSecondsF());
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
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(2);
  timing_.iteration_count = 10;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(10.0, keyframe_model->iterations());
  EXPECT_EQ(0, keyframe_model->time_offset().InSecondsF());
  EXPECT_EQ(cc::KeyframeModel::Direction::ALTERNATE_NORMAL,
            keyframe_model->direction());
  EXPECT_EQ(1.0, keyframe_model->playback_rate());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF() *
                   timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  ExpectKeyframeTimingFunctionCubic(*keyframes[0],
                                    CubicBezierTimingFunction::EaseType::EASE);

  EXPECT_EQ(0.5, keyframes[1]->Time().InSecondsF() *
                     timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());

  EXPECT_EQ(1.0, keyframes[2]->Time().InSecondsF() *
                     timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.35f, keyframes[2]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[2], CubicBezierTimingFunction::EaseType::CUSTOM);

  EXPECT_EQ(2.0, keyframes[3]->Time().InSecondsF() *
                     timing_.iteration_duration->InSecondsF());
  EXPECT_EQ(0.5f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[3]->timing_function())
                                              ->GetType());
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

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(10.0, keyframe_model->iterations());
  EXPECT_EQ(0, keyframe_model->time_offset().InSecondsF());
  EXPECT_EQ(cc::KeyframeModel::Direction::ALTERNATE_REVERSE,
            keyframe_model->direction());
  EXPECT_EQ(1.0, keyframe_model->playback_rate());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(CreateCompositorTimingFunctionFromCC(
                keyframed_float_curve->timing_function_for_testing())
                ->GetType(),
            TimingFunction::Type::LINEAR);

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[0], CubicBezierTimingFunction::EaseType::EASE_IN);

  EXPECT_EQ(0.25, keyframes[1]->Time().InSecondsF());
  EXPECT_EQ(0, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());

  EXPECT_EQ(0.5, keyframes[2]->Time().InSecondsF());
  EXPECT_EQ(0.25f, keyframes[2]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[2], CubicBezierTimingFunction::EaseType::CUSTOM);

  EXPECT_EQ(1.0, keyframes[3]->Time().InSecondsF());
  EXPECT_EQ(0.5f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[3]->timing_function())
                                              ->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateReversedOpacityAnimationNegativeStartDelay) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  const double kNegativeStartDelay = -3;

  timing_.iteration_count = 5.0;
  timing_.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1.5);
  timing_.start_delay =
      Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(kNegativeStartDelay));
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(5.0, keyframe_model->iterations());
  EXPECT_EQ(-kNegativeStartDelay, keyframe_model->time_offset().InSecondsF());
  EXPECT_EQ(cc::KeyframeModel::Direction::ALTERNATE_REVERSE,
            keyframe_model->direction());
  EXPECT_EQ(1.0, keyframe_model->playback_rate());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationFillModeNone) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.fill_mode = Timing::FillMode::NONE;

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  // Time based animations implicitly fill forwards to remain active until
  // the subsequent commit.
  EXPECT_EQ(cc::KeyframeModel::FillMode::FORWARDS, keyframe_model->fill_mode());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationFillModeAuto) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.fill_mode = Timing::FillMode::AUTO;

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::OPACITY, keyframe_model->TargetProperty());
  EXPECT_EQ(1.0, keyframe_model->iterations());
  EXPECT_EQ(0, keyframe_model->time_offset().InSecondsF());
  EXPECT_EQ(cc::KeyframeModel::Direction::NORMAL, keyframe_model->direction());
  EXPECT_EQ(1.0, keyframe_model->playback_rate());
  // Time based animations implicitly fill forwards to remain active until
  // the subsequent commit.
  EXPECT_EQ(cc::KeyframeModel::FillMode::FORWARDS, keyframe_model->fill_mode());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleOpacityAnimationWithTimingFunction) {
  // KeyframeEffect to convert
  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.2", 0),
      CreateReplaceOpKeyframe(CSSPropertyID::kOpacity, "0.5", 1.0));

  timing_.timing_function = cubic_custom_timing_function_;

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  auto curve_timing_function = CreateCompositorTimingFunctionFromCC(
      keyframed_float_curve->timing_function_for_testing());
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

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF());
  EXPECT_EQ(0.2f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[0]->timing_function())
                                              ->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time().InSecondsF());
  EXPECT_EQ(0.5f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());
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

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::CSS_CUSTOM_PROPERTY,
            keyframe_model->TargetProperty());
  EXPECT_EQ(keyframe_model->custom_property_name(),
            property_name.Utf8().data());
  EXPECT_FALSE(effect->RequiresPropertyNode());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleCustomFloatPropertyAnimation) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);

  RegisterProperty(GetDocument(), "--foo", "<number>", "0", false);
  SetCustomProperty("--foo", "10");

  StringKeyframeEffectModel* effect =
      CreateKeyframeEffectModel(CreateReplaceOpKeyframe("--foo", "10", 0),
                                CreateReplaceOpKeyframe("--foo", "20", 1.0));

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::CSS_CUSTOM_PROPERTY,
            keyframe_model->TargetProperty());

  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> keyframed_float_curve =
      CreateKeyframedFloatAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::FloatKeyframe>>& keyframes =
      keyframed_float_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF());
  EXPECT_EQ(10, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[0]->timing_function())
                                              ->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time().InSecondsF());
  EXPECT_EQ(20, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());
}

TEST_P(AnimationCompositorAnimationsTest,
       CreateSimpleCustomColorPropertyAnimation) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);

  RegisterProperty(GetDocument(), "--foo", "<color>", "rgb(0, 0, 0)", false);
  SetCustomProperty("--foo", "rgb(0, 0, 0)");

  StringKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe("--foo", "rgb(0, 0, 0)", 0),
      CreateReplaceOpKeyframe("--foo", "rgb(0, 255, 0)", 1.0));

  std::unique_ptr<cc::KeyframeModel> keyframe_model =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(cc::TargetProperty::CSS_CUSTOM_PROPERTY,
            keyframe_model->TargetProperty());

  std::unique_ptr<gfx::KeyframedColorAnimationCurve> keyframed_color_curve =
      CreateKeyframedColorAnimationCurve(keyframe_model.get());

  const std::vector<std::unique_ptr<gfx::ColorKeyframe>>& keyframes =
      keyframed_color_curve->keyframes_for_testing();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time().InSecondsF());
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[0]->timing_function())
                                              ->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time().InSecondsF());
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR, CreateCompositorTimingFunctionFromCC(
                                              keyframes[1]->timing_function())
                                              ->GetType());
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
      MakeGarbageCollected<HeapVector<Member<StringKeyframe>>>(
          CreateDefaultKeyframeVector(CSSPropertyID::kOpacity,
                                      EffectModel::kCompositeReplace));
  KeyframeEffectModelBase* animation_effect1 =
      MakeGarbageCollected<StringKeyframeEffectModel>(*key_frames);
  KeyframeEffectModelBase* animation_effect2 =
      MakeGarbageCollected<StringKeyframeEffectModel>(*key_frames);

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);

  // The first animation for opacity is ok to run on compositor.
  auto* keyframe_effect1 = MakeGarbageCollected<KeyframeEffect>(
      element_.Get(), animation_effect1, timing);
  Animation* animation1 = timeline_->Play(keyframe_effect1);
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();
  animation_effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(),
                                                               style, nullptr);
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing, *element_.Get(), animation1,
                                            *animation_effect1),
            CompositorAnimations::kNoFailure);

  // The second animation for opacity is not ok to run on compositor.
  auto* keyframe_effect2 = MakeGarbageCollected<KeyframeEffect>(
      element_.Get(), animation_effect2, timing);
  Animation* animation2 = timeline_->Play(keyframe_effect2);
  animation_effect2->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(),
                                                               style, nullptr);
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
  EXPECT_TRUE(element_->GetElementAnimations()->Animations().empty());
}

namespace {

void UpdateDummyTransformNode(ObjectPaintProperties& properties,
                              CompositingReasons reasons) {
  TransformPaintPropertyNode::State state;
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
  Persistent<Element> element =
      GetDocument().CreateElementForBinding(AtomicString("shared"));
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
  Persistent<Element> element =
      GetDocument().CreateElementForBinding(AtomicString("shared"));
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

TEST_P(AnimationCompositorAnimationsTest, TrackSVGAnimation) {
  LoadTestData("svg-smil-animation.html");

  cc::AnimationHost* host = GetFrame()->View()->GetCompositorAnimationHost();

  BeginFrame();
  EXPECT_TRUE(host->HasSmilAnimation());
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
  Element* target = document->getElementById(AtomicString("target"));
  // Make sure the animation is started on the compositor.
  EXPECT_EQ(CheckCanStartElementOnCompositor(*target, *effect),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest, CompositedTransformAnimation) {
  LoadTestData("transform-animation.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById(AtomicString("target"));
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
  const auto* cc_transform =
      property_trees->transform_tree().FindNodeFromElementId(
          transform->GetCompositorElementId());
  ASSERT_NE(nullptr, cc_transform);
  EXPECT_TRUE(cc_transform->has_potential_animation);
  EXPECT_TRUE(cc_transform->is_currently_animating);
  EXPECT_EQ(1.f, cc_transform->maximum_animation_scale);

  // Make sure the animation is started on the compositor.
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
  EXPECT_EQ(document->Timeline().AnimationsNeedingUpdateCount(), 1u);
}

TEST_P(AnimationCompositorAnimationsTest, CompositedScaleAnimation) {
  LoadTestData("scale-animation.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById(AtomicString("target"));
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
  const auto* cc_transform =
      property_trees->transform_tree().FindNodeFromElementId(
          transform->GetCompositorElementId());
  ASSERT_NE(nullptr, cc_transform);
  EXPECT_TRUE(cc_transform->has_potential_animation);
  EXPECT_TRUE(cc_transform->is_currently_animating);
  EXPECT_EQ(5.f, cc_transform->maximum_animation_scale);

  // Make sure the animation is started on the compositor.
  EXPECT_EQ(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
  EXPECT_EQ(document->Timeline().AnimationsNeedingUpdateCount(), 1u);
}

TEST_P(AnimationCompositorAnimationsTest,
       NonAnimatedTransformPropertyChangeGetsUpdated) {
  LoadTestData("transform-animation-update.html");
  Document* document = GetFrame()->GetDocument();
  Element* target = document->getElementById(AtomicString("target"));
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
  const auto* cc_transform =
      property_trees->transform_tree().FindNodeFromElementId(
          transform->GetCompositorElementId());
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
  const auto& layer =
      *CcLayersByDOMElementId(document->View()->RootCcLayer(), "target")[0];
  EXPECT_FALSE(layer.should_check_backface_visibility());

  // Change the backface visibility, while the compositor animation is
  // happening.
  target->setAttribute(html_names::kClassAttr, AtomicString("backface-hidden"));
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
  cc_transform = property_trees->transform_tree().FindNodeFromElementId(
      transform->GetCompositorElementId());
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
  Element* target = document->getElementById(AtomicString("dots"));
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

  ScopedNullExecutionContext execution_context;
  // Move the target element to another Document, that does not have a frame
  // (and thus no Settings).
  Document* another_document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  ASSERT_FALSE(another_document->GetSettings());

  another_document->adoptNode(target, ASSERT_NO_EXCEPTION);

  // This should not crash.
  EXPECT_NE(
      CheckCanStartElementOnCompositor(*target, *keyframe_animation_effect2_),
      CompositorAnimations::kNoFailure);
}

TEST_P(AnimationCompositorAnimationsTest, DetachCompositorTimelinesTest) {
  LoadTestData("transform-animation.html");
  Document* document = GetFrame()->GetDocument();
  cc::AnimationHost* host = document->View()->GetCompositorAnimationHost();

  Element* target = document->getElementById(AtomicString("target"));
  const Animation& animation =
      *target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_TRUE(animation.GetCompositorAnimation());

  cc::AnimationTimeline* compositor_timeline =
      animation.TimelineInternal()->CompositorTimeline();
  ASSERT_TRUE(compositor_timeline);
  int id = compositor_timeline->id();
  ASSERT_TRUE(host->GetTimelineById(id));
  document->GetDocumentAnimations().DetachCompositorTimelines();
  ASSERT_FALSE(host->GetTimelineById(id));
}

TEST_P(AnimationCompositorAnimationsTest,
       CanStartTransformAnimationOnCompositorForSVG) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .animate {
        width: 100px;
        height: 100px;
        animation: wave 1s infinite;
      }
      @keyframes wave {
        0% { transform: rotate(-5deg); }
        100% { transform: rotate(5deg); }
      }
    </style>
    <svg id="svg" class="animate">
      <rect id="rect" class="animate"/>
      <rect id="rect-useref" class="animate"/>
      <rect id="rect-smil" class="animate">
        <animateMotion dur="10s" repeatCount="indefinite"
                       path="M0,0 L100,100 z"/>
      </rect>
      <rect id="rect-effect" class="animate"
            vector-effect="non-scaling-stroke"/>
      <g id="g-effect" class="animate">
        <rect class="animate" vector-effect="non-scaling-stroke"/>
      </g>
      <svg id="nested-svg" class="animate"/>
      <foreignObject id="foreign" class="animate"/>
      <foreignObject id="foreign-zoomed" class="animate"
                     style="zoom: 1.5; will-change: opacity"/>
      <use id="use" href="#rect-useref" class="animate"/>
      <use id="use-offset" href="#rect-useref" x="10" class="animate"/>
    </svg>
    <svg id="svg-zoomed" class="animate" style="zoom: 1.5">
      <rect id="rect-zoomed" class="animate"/>
    </svg>
  )HTML");

  auto CanStartAnimation = [&](const char* id) -> bool {
    return CompositorAnimations::CanStartTransformAnimationOnCompositorForSVG(
        To<SVGElement>(*GetElementById(id)));
  };

  EXPECT_TRUE(CanStartAnimation("svg"));
  EXPECT_TRUE(CanStartAnimation("rect"));
  EXPECT_FALSE(CanStartAnimation("rect-useref"));
  EXPECT_FALSE(CanStartAnimation("rect-smil"));
  EXPECT_FALSE(CanStartAnimation("rect-effect"));
  EXPECT_FALSE(CanStartAnimation("g-effect"));
  EXPECT_FALSE(CanStartAnimation("nested-svg"));
  EXPECT_TRUE(CanStartAnimation("foreign"));
  EXPECT_FALSE(CanStartAnimation("foreign-zoomed"));
  EXPECT_TRUE(CanStartAnimation("use"));
  EXPECT_FALSE(CanStartAnimation("use-offset"));

  EXPECT_FALSE(CanStartAnimation("svg-zoomed"));
  EXPECT_FALSE(CanStartAnimation("rect-zoomed"));

  To<SVGElement>(GetDocument().getElementById(AtomicString("rect")))
      ->SetWebAnimatedAttribute(
          svg_names::kXAttr,
          MakeGarbageCollected<SVGLength>(SVGLength::Initial::kPercent50,
                                          SVGLengthMode::kOther));
  EXPECT_FALSE(CanStartAnimation("rect"));
}

TEST_P(AnimationCompositorAnimationsTest, UnsupportedSVGCSSProperty) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes mixed {
        0% { transform: rotate(-5deg); stroke-dashoffset: 0; }
        100% { transform: rotate(5deg); stroke-dashoffset: 180; }
      }
    </style>
    <svg>
      <rect id="rect"
            style="width: 100px; height: 100px; animation: mixed 1s infinite"/>
    </svg>
  )HTML");

  Element* element = GetDocument().getElementById(AtomicString("rect"));
  const Animation& animation =
      *element->GetElementAnimations()->Animations().begin()->key;
  EXPECT_EQ(CompositorAnimations::kUnsupportedCSSProperty,
            animation.CheckCanStartAnimationOnCompositor(
                GetDocument().View()->GetPaintArtifactCompositor()));
}

TEST_P(AnimationCompositorAnimationsTest,
       TotalAnimationCountAcrossAllDocuments) {
  LoadTestData("animation-in-main-frame.html");

  cc::AnimationHost* host =
      GetFrame()->GetDocument()->View()->GetCompositorAnimationHost();

  // We are checking that the animation count for all documents is 1 for every
  // frame.
  for (int i = 0; i < 9; i++) {
    BeginFrame();
    EXPECT_EQ(1U, host->MainThreadAnimationsCount());
  }
}

TEST_P(AnimationCompositorAnimationsTest,
       MainAnimationCountExcludesInactiveAnimations) {
  LoadTestData("inactive-animations.html");

  cc::AnimationHost* host =
      GetFrame()->GetDocument()->View()->GetCompositorAnimationHost();

  // Verify that the paused animation does not count as a running main thread
  // animation.
  EXPECT_EQ(0U, host->MainThreadAnimationsCount());
}

TEST_P(AnimationCompositorAnimationsTest, TrackRafAnimationAcrossAllDocuments) {
  LoadTestData("raf-countdown-in-main-frame.html");

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

TEST_P(AnimationCompositorAnimationsTest, Fragmented) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes move {
        0% { transform: translateX(10px); }
        100% { transform: translateX(20px); }
      }
      #target {
        width: 10px;
        height: 150px;
        background: green;
      }
    </style>
    <div style="columns: 2; height: 100px">
      <div id="target" style="animation: move 1s infinite">
      </div>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  const Animation& animation =
      *target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_TRUE(target->GetLayoutObject()->IsFragmented());
  EXPECT_EQ(CompositorAnimations::kTargetHasInvalidCompositingState,
            animation.CheckCanStartAnimationOnCompositor(
                GetDocument().View()->GetPaintArtifactCompositor()));
}

TEST_P(AnimationCompositorAnimationsTest,
       CancelIncompatibleTransformCompositorAnimation) {
  const auto& style = GetDocument().GetStyleResolver().InitialStyle();

  // The first animation for transform is ok to run on the compositor.
  StringKeyframeEffectModel* effect1 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "none", 0.0),
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "scale(2)", 1.0));
  auto* keyframe_effect1 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect1, timing_);
  Animation* animation1 = timeline_->Play(keyframe_effect1);
  effect1->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
                                                     nullptr);
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                            animation1, *effect1),
            CompositorAnimations::kNoFailure);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(animation1->HasActiveAnimationsOnCompositor());

  // The animation for rotation is ok to run on the compositor as it is a
  // different transformation property.
  StringKeyframeEffectModel* effect2 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kRotate, "0deg", 0.0),
      CreateReplaceOpKeyframe(CSSPropertyID::kRotate, "90deg", 1.0));
  KeyframeEffect* keyframe_effect2 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect2, timing_);
  Animation* animation2 = timeline_->Play(keyframe_effect2);
  effect2->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
                                                     nullptr);
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                            animation2, *effect2),
            CompositorAnimations::kNoFailure);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(animation1->HasActiveAnimationsOnCompositor());
  EXPECT_TRUE(animation2->HasActiveAnimationsOnCompositor());

  // The second animation for transform is not ok to run on the compositor.
  StringKeyframeEffectModel* effect3 = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "none", 0.0),
      CreateReplaceOpKeyframe(CSSPropertyID::kTransform, "translateX(10px)",
                              1.0));
  KeyframeEffect* keyframe_effect3 =
      MakeGarbageCollected<KeyframeEffect>(element_.Get(), effect3, timing_);
  Animation* animation3 = timeline_->Play(keyframe_effect3);
  effect3->SnapshotAllCompositorKeyframesIfNecessary(*element_.Get(), style,
                                                     nullptr);
  EXPECT_EQ(CheckCanStartEffectOnCompositor(timing_, *element_.Get(),
                                            animation3, *effect3),
            CompositorAnimations::kTargetHasIncompatibleAnimations);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(animation1->HasActiveAnimationsOnCompositor());
  EXPECT_TRUE(animation2->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation3->HasActiveAnimationsOnCompositor());
}

TEST_P(AnimationCompositorAnimationsTest,
       LongActiveDurationWithNegativePlaybackRate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes move {
        0% { transform: translateX(10px); }
        100% { transform: translateX(20px); }
      }
      #target {
        width: 10px;
        height: 150px;
        background: green;
        animation: move 1s 2222222200000;
      }
    </style>
    <div id="target"></div>
  )HTML");
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Animation* animation =
      target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(
                GetDocument().View()->GetPaintArtifactCompositor()));
  // Setting a small negative playback rate has the following effects:
  // The scaled active duration in microseconds now exceeds the max
  // for an int64. Since the playback rate is negative we need to jump
  // to the end and play backwards, which triggers problems with math
  // involving infinity.
  animation->setPlaybackRate(-0.01);
  EXPECT_TRUE(CompositorAnimations::kInvalidAnimationOrEffect &
              animation->CheckCanStartAnimationOnCompositor(
                  GetDocument().View()->GetPaintArtifactCompositor()));
}

class ScopedBackgroundColorPaintImageGenerator {
 public:
  explicit ScopedBackgroundColorPaintImageGenerator(LocalFrame* frame)
      : paint_image_generator_(
        MakeGarbageCollected<FakeBackgroundColorPaintImageGenerator>()),
        frame_(frame) {
    frame_->SetBackgroundColorPaintImageGeneratorForTesting(
        paint_image_generator_);
  }

  ~ScopedBackgroundColorPaintImageGenerator() {
    frame_->SetBackgroundColorPaintImageGeneratorForTesting(nullptr);
  }

 private:
  class FakeBackgroundColorPaintImageGenerator
      : public BackgroundColorPaintImageGenerator {
    scoped_refptr<Image> Paint(const gfx::SizeF& container_size,
                               const Node* node) override {
      return BitmapImage::Create();
    }

    Animation* GetAnimationIfCompositable(const Element* element) override {
      // Note that the complete test for determining eligibility to run on the
      // compositor is in modules code. It is a layering violation to include
      // here. Instead, we assume that no paint definition specific constraints
      // are violated. These additional constraints should be tested in
      // *_paint_definitiion_test.cc.
      return element->GetElementAnimations()->Animations().begin()->key;
    }

    void Shutdown() override {}
  };

  Persistent<FakeBackgroundColorPaintImageGenerator> paint_image_generator_;
  Persistent<LocalFrame> frame_;
};

TEST_P(AnimationCompositorAnimationsTest, BackgroundShorthand) {
  ClearUseCounters();
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        0% { background: red; }
        100% { background: green; }
      }
      #target {
        width: 100px;
        height: 100px;
        animation: colorize 1s linear;
      }
    </style>
    <div id="target"></div>
  )HTML");

  // Normally, we don't get image generators set up in a testing environment.
  // Construct a fake one to allow us to test that we are making the correct
  // compositing decision.
  ScopedBackgroundColorPaintImageGenerator image_generator(
      GetDocument().GetFrame());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  Animation* animation =
      target->GetElementAnimations()->Animations().begin()->key;

  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(
                GetDocument().View()->GetPaintArtifactCompositor()));

  EXPECT_TRUE(IsUseCounted(WebFeature::kStaticPropertyInAnimation));
}

TEST_P(AnimationCompositorAnimationsTest, StaticNonCompositableProperty) {
  ClearUseCounters();
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes fade-in {
        0% { opacity: 0; left: 0px; }
        100% { opacity: 1; left: 0px; }
      }
      #target {
        width: 100px;
        height: 100px;
        animation: fade-in 1s linear;
      }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  Animation* animation =
      target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(
                GetDocument().View()->GetPaintArtifactCompositor()));
  EXPECT_TRUE(IsUseCounted(WebFeature::kStaticPropertyInAnimation));
}

TEST_P(AnimationCompositorAnimationsTest, StaticCompositableProperty) {
  ClearUseCounters();
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes static {
        0% { opacity: 1; }
        100% { opacity: 1; }
      }
      #target {
        width: 100px;
        height: 100px;
        animation: static 1s linear;
      }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  Animation* animation =
      target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_TRUE(CompositorAnimations::kAnimationHasNoVisibleChange &
              animation->CheckCanStartAnimationOnCompositor(
                  GetDocument().View()->GetPaintArtifactCompositor()));
  EXPECT_TRUE(IsUseCounted(WebFeature::kStaticPropertyInAnimation));
}

TEST_P(AnimationCompositorAnimationsTest, EmptyKeyframes) {
  ClearUseCounters();
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes no-op {
      }
      #target {
        width: 100px;
        height: 100px;
        animation: no-op 1s linear;
      }
    </style>
    <div id="target"></div>
  )HTML");
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Animation* animation =
      target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_TRUE(CompositorAnimations::kAnimationHasNoVisibleChange &
              animation->CheckCanStartAnimationOnCompositor(
                  GetDocument().View()->GetPaintArtifactCompositor()));
  EXPECT_FALSE(IsUseCounted(WebFeature::kStaticPropertyInAnimation));
}

TEST_P(AnimationCompositorAnimationsTest,
       WebKitPrefixedPlusUnprefixedProperty) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes test {
        from {
          -webkit-filter: saturate(0.25);
          filter: saturate(0.25);
        }
        to {
          -webkit-filter: saturate(0.75);
          filter: saturate(0.75);
        }
      }
      #target {
        animation: test 1e3s;
        height: 100px;
        width: 100px;
        background: green;
      }
    </style>
    <div id="target"></div>
  )HTML");
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Animation* animation =
      target->GetElementAnimations()->Animations().begin()->key;
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(
                GetDocument().View()->GetPaintArtifactCompositor()));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
}

}  // namespace blink
