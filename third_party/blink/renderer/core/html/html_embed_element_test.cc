// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_embed_element.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLEmbedElementTest : public PageTestBase {};

TEST_F(HTMLEmbedElementTest, FallbackState) {
  // Load <object> element with a <embed> child.
  // This can be seen on sites with Flash cookies,
  // for example on www.yandex.ru
  SetHtmlInnerHTML(R"HTML(
    <div>
    <object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000' width='1'
    height='1' id='fco'>
    <param name='movie' value='//site.com/flash-cookie.swf'>
    <param name='allowScriptAccess' value='Always'>
    <embed src='//site.com/flash-cookie.swf' allowscriptaccess='Always'
    width='1' height='1' id='fce'>
    </object></div>
  )HTML");

  auto* object_element = GetElementById("fco");
  ASSERT_TRUE(object_element);
  auto* object = To<HTMLObjectElement>(object_element);

  // At this moment updatePlugin() function is not called, so
  // useFallbackContent() will return false.
  // But the element will likely to use fallback content after updatePlugin().
  EXPECT_TRUE(object->HasFallbackContent());
  EXPECT_FALSE(object->UseFallbackContent());
  EXPECT_TRUE(object->WillUseFallbackContentAtLayout());

  auto* embed_element = GetElementById("fce");
  ASSERT_TRUE(embed_element);
  auto* embed = To<HTMLEmbedElement>(embed_element);

  UpdateAllLifecyclePhasesForTest();

  const ComputedStyle* initial_style =
      GetDocument().GetStyleResolver().InitialStyleForElement();

  // We should get |true| as a result and don't trigger a DCHECK.
  EXPECT_TRUE(
      static_cast<Element*>(embed)->LayoutObjectIsNeeded(*initial_style));

  // This call will update fallback state of the object.
  object->UpdatePlugin();

  EXPECT_TRUE(object->HasFallbackContent());
  EXPECT_TRUE(object->UseFallbackContent());
  EXPECT_TRUE(object->WillUseFallbackContentAtLayout());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      static_cast<Element*>(embed)->LayoutObjectIsNeeded(*initial_style));
}

TEST_F(HTMLEmbedElementTest, NotEnforceLayoutImageType) {
  SetHtmlInnerHTML(R"HTML(
    <object type="text/plain" id="object">
      <embed id="embed" type="image/png">
    </object>)HTML");
  auto* object_element = GetElementById("object");
  auto* object = To<HTMLObjectElement>(object_element);
  auto* embed_element = GetElementById("embed");
  auto* embed = To<HTMLEmbedElement>(embed_element);

  EXPECT_TRUE(object->HasFallbackContent());
  EXPECT_FALSE(object->UseFallbackContent());
  EXPECT_FALSE(object->WillUseFallbackContentAtLayout());

  UpdateAllLifecyclePhasesForTest();

  const ComputedStyle* initial_style =
      GetDocument().GetStyleResolver().InitialStyleForElement();

  EXPECT_FALSE(
      static_cast<Element*>(embed)->LayoutObjectIsNeeded(*initial_style));

  object->UpdatePlugin();

  EXPECT_TRUE(object->HasFallbackContent());
  EXPECT_TRUE(object->UseFallbackContent());
  EXPECT_FALSE(object->WillUseFallbackContentAtLayout());

  EXPECT_TRUE(
      static_cast<Element*>(embed)->LayoutObjectIsNeeded(*initial_style));
}

}  // namespace blink
