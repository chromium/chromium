// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class AnimationUtilsTest : public RenderingTest {
 public:
  AnimationUtilsTest() = default;

  StringKeyframe* AddKeyframe(StringKeyframeVector& keyframes, double offset) {
    StringKeyframe* keyframe = MakeGarbageCollected<StringKeyframe>();
    keyframe->SetOffset(offset);
    keyframes.push_back(keyframe);
    return keyframe;
  }

  void AddProperty(StringKeyframe* keyframe,
                   CSSPropertyID property_id,
                   String value) {
    keyframe->SetCSSPropertyValue(property_id, value,
                                  SecureContextMode::kInsecureContext,
                                  /*style_sheet_contents=*/nullptr);
  }

  void AddInterpolation(ActiveInterpolationsMap& interpolations_map,
                        const StringKeyframeVector& keyframes,
                        PropertyHandle property_handle) {
    ActiveInterpolationsMap::AddResult entry = interpolations_map.insert(
        property_handle, MakeGarbageCollected<ActiveInterpolations>());
    ActiveInterpolations* active_interpolations = entry.stored_value->value;

    PropertySpecificKeyframe* from_keyframe =
        CreatePropertySpecificKeyframe(keyframes[0], property_handle, 0);
    PropertySpecificKeyframe* to_keyframe =
        CreatePropertySpecificKeyframe(keyframes[1], property_handle, 1);

    Interpolation* interpolation =
        MakeGarbageCollected<InvalidatableInterpolation>(
            property_handle, from_keyframe, to_keyframe);
    interpolation->Interpolate(/*iteration=*/0, /*progress=*/1);
    active_interpolations->push_back(interpolation);
  }

  PropertySpecificKeyframe* CreatePropertySpecificKeyframe(
      Keyframe* keyframe,
      PropertyHandle property_handle,
      double offset) {
    return keyframe->CreatePropertySpecificKeyframe(
        property_handle, EffectModel::kCompositeReplace, offset);
  }
};

TEST_F(AnimationUtilsTest, ForEachInterpolatedPropertyValue) {
  SetBodyInnerHTML("<div id='target' style='left:10px'></div>");
  Element* target = GetElementById("target");

  PropertyHandleSet properties;
  properties.insert(PropertyHandle(GetCSSPropertyLeft()));
  properties.insert(PropertyHandle(GetCSSPropertyTop()));

  HashMap<String, String> map;
  ActiveInterpolationsMap interpolations_map;

  auto callback = [&map](PropertyHandle property, const CSSValue* value) {
    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property);
    map.Set(property_name, value->CssText());
  };

  AnimationUtils::ForEachInterpolatedPropertyValue(
      target, properties, interpolations_map, callback);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ("10px", map.at("left"));
  EXPECT_EQ("auto", map.at("top"));

  map.clear();

  StringKeyframeVector keyframes;
  StringKeyframe* fromKeyframe = AddKeyframe(keyframes, 0);
  AddProperty(fromKeyframe, CSSPropertyID::kLeft, "10px");
  AddProperty(fromKeyframe, CSSPropertyID::kTop, "auto");

  StringKeyframe* toKeyframe = AddKeyframe(keyframes, 1);
  AddProperty(toKeyframe, CSSPropertyID::kLeft, "20px");
  AddProperty(toKeyframe, CSSPropertyID::kTop, "40px");

  AddInterpolation(interpolations_map, keyframes,
                   PropertyHandle(GetCSSPropertyLeft()));
  AddInterpolation(interpolations_map, keyframes,
                   PropertyHandle(GetCSSPropertyTop()));

  AnimationUtils::ForEachInterpolatedPropertyValue(
      target, properties, interpolations_map, callback);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ("20px", map.at("left"));
  EXPECT_EQ("40px", map.at("top"));
}

TEST_F(AnimationUtilsTest, ForEachInterpolatedPropertyValueWithContainerQuery) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size; }
      @container (min-width: 1px) {
        #target { left: 10px; }
      }
      @container (min-width: 99999px) {
        #target { left: 10000px; }
      }
    </style>
    <div id="container">
      <div id="target"></div>
    </div>
  )HTML");
  Element* target = GetElementById("target");

  PropertyHandleSet properties;
  properties.insert(PropertyHandle(GetCSSPropertyLeft()));

  HashMap<String, String> map;
  ActiveInterpolationsMap interpolations_map;

  auto callback = [&map](PropertyHandle property, const CSSValue* value) {
    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property);
    map.Set(property_name, value->CssText());
  };

  AnimationUtils::ForEachInterpolatedPropertyValue(
      target, properties, interpolations_map, callback);
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ("10px", map.at("left"));

  map.clear();

  StringKeyframeVector keyframes;
  StringKeyframe* fromKeyframe = AddKeyframe(keyframes, 0);
  AddProperty(fromKeyframe, CSSPropertyID::kLeft, "30px");

  StringKeyframe* toKeyframe = AddKeyframe(keyframes, 1);
  AddProperty(toKeyframe, CSSPropertyID::kLeft, "20px");

  AddInterpolation(interpolations_map, keyframes,
                   PropertyHandle(GetCSSPropertyLeft()));

  AnimationUtils::ForEachInterpolatedPropertyValue(
      target, properties, interpolations_map, callback);
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ("20px", map.at("left"));
}

}  // namespace blink
