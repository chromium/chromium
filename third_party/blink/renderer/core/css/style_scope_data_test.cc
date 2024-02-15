// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope_data.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StyleScopeDataTest : public PageTestBase {
 public:
  Element& QuerySelector(String selector) {
    Element* element = GetDocument().QuerySelector(AtomicString(selector));
    DCHECK(element);
    return *element;
  }

  StyleScopeData* GetStyleScopeData(String selector) {
    return QuerySelector(selector).GetStyleScopeData();
  }

  using TriggeredScopes = HeapVector<Member<const StyleScope>, 1>;

  TriggeredScopes GetTriggeredScopes(String selector) {
    if (StyleScopeData* style_scope_data = GetStyleScopeData(selector)) {
      return style_scope_data->triggered_implicit_scopes_;
    }
    return TriggeredScopes();
  }

  const StyleScope* GetSingleTriggeredScope(String selector) {
    const TriggeredScopes& scopes = GetTriggeredScopes(selector);
    return (scopes.size() == 1u) ? scopes.front().Get() : nullptr;
  }

  Element* MakeStyle(String style) {
    auto* style_element = MakeGarbageCollected<HTMLStyleElement>(GetDocument());
    style_element->setTextContent(style);
    return style_element;
  }

  void AppendChild(String selector, Element* child) {
    QuerySelector(selector).AppendChild(child);
  }

  void RemoveChild(String selector, Element* child) {
    QuerySelector(selector).RemoveChild(child);
  }
};

TEST_F(StyleScopeDataTest, NoScopes) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, NotImplicitScope) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        @scope (div) {
          * { color: green; }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, Trivial) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        @scope {
          * { color: green; }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, ExtraLeadingStyleRule) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        div { color: blue; }
        @scope {
          * { color: green; }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, ExtraTrailingStyleRule) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        @scope {
          * { color: green; }
        }
        div { color: blue; }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, TwoInOne) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        @scope {
          * { color: green; }
        }
        @scope {
          * { color: blue; }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(2u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, TwoInOneNested) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        @scope {
          * { color: green; }

          @scope {
            * { color: blue; }
          }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(2u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, NestedNonImplicitOuter) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style>
        @scope (div) {
          * { color: green; }

          @scope {
            * { color: blue; }
          }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, DistinctContent) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a>
      <style>
        @scope {
          * { color: green; }
        }
      </style>
    </div>
    <div id=b>
      <style>
        @scope {
          * { --different: true; }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  const StyleScope* a = GetSingleTriggeredScope("#a");
  const StyleScope* b = GetSingleTriggeredScope("#b");
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_NE(a, b);
}

TEST_F(StyleScopeDataTest, SharedContent) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a>
      <style>
        @scope {
          * { color: green; }
        }
      </style>
    </div>
    <div id=b>
      <style>
        @scope {
          * { color: green; }
        }
      </style>
    </div>
    <div id=c>
      <style>
        @scope {
          * { --different: true; }
        }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  const StyleScope* a = GetSingleTriggeredScope("#a");
  const StyleScope* b = GetSingleTriggeredScope("#b");
  const StyleScope* c = GetSingleTriggeredScope("#c");
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  // The StyleScope instances for #a and b are the same, because the two
  // stylesheets are identical, and therefore share the same StyleSheetContents.
  EXPECT_EQ(a, b);
  // The style for #c is not identical however.
  EXPECT_NE(a, c);
}

TEST_F(StyleScopeDataTest, Tree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a1></div>
    <div id=a2>
      <div id=b1></div>
      <div id=b2>
        <div id=c1></div>
        <style>
          @scope {
            * { color: green; }
          }
        </style>
        <div id=c2></div>
        <div id=c3></div>
      </div>
      <div id=b3></div>
    </div>
    <div id=a3></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(0u, GetTriggeredScopes("#a1").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#a2").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#a3").size());

  EXPECT_EQ(0u, GetTriggeredScopes("#b1").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b2").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b3").size());

  EXPECT_EQ(0u, GetTriggeredScopes("#c1").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#c2").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#c3").size());
}

// Mutations

TEST_F(StyleScopeDataTest, TrivialInsertRemove) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());

  Element* style = MakeStyle(R"CSS(
    @scope {
      * { color: green; }
    }
  )CSS");

  AppendChild("#b", style);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());

  RemoveChild("#b", style);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, DoubleInsertRemove) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());

  Element* style1 = MakeStyle(R"CSS(
    @scope {
      * { color: green; }
    }
  )CSS");

  Element* style2 = MakeStyle(R"CSS(
    @scope {
      * { color: blue; }
    }
  )CSS");

  AppendChild("#a", style1);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());

  AppendChild("#b", style2);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());

  // Move style2 to #a.
  RemoveChild("#b", style2);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());
  AppendChild("#a", style2);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, MutateSheet) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=a></div>
    <div id=b>
      <style id=s></style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());

  Element& s = QuerySelector("#s");

  s.setTextContent("@scope { * { color: green; } }");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#b").size());

  s.setTextContent("div { color: red; }");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#b").size());
}

TEST_F(StyleScopeDataTest, ShadowHost) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id=a></div>
    <div id=host>
      <template shadowrootmode=open>
        <style>
          @scope {
            * { color: green; }
          }
        </style>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#host").size());
}

TEST_F(StyleScopeDataTest, ShadowHostDoubleScope) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id=a></div>
    <div id=host>
      <template shadowrootmode=open>
        <style>
          @scope {
            * { color: green; }
          }
        </style>
      </template>
      <style>
          @scope {
            * { color: blue; }
          }
      </style>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(2u, GetTriggeredScopes("#host").size());
}

TEST_F(StyleScopeDataTest, AdoptedStylesheet) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id=a></div>
    <div id=host>
      <template shadowrootmode=open>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(0u, GetTriggeredScopes("#host").size());

  DummyExceptionStateForTesting exception_state;
  auto* init = CSSStyleSheetInit::Create();
  auto* sheet = CSSStyleSheet::Create(GetDocument(), init, exception_state);
  sheet->replaceSync("@scope { * { color: blue; } }", exception_state);

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->GetShadowRoot());

  HeapVector<Member<CSSStyleSheet>> stylesheets;
  stylesheets.push_back(sheet);
  host->GetShadowRoot()->SetAdoptedStyleSheetsForTesting(stylesheets);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(1u, GetTriggeredScopes("#host").size());

  // Add a second adopted stylesheet.
  auto* sheet2 = CSSStyleSheet::Create(GetDocument(), init, exception_state);
  sheet2->replaceSync("@scope { * { color: red; } }", exception_state);
  stylesheets.push_back(sheet2);
  host->GetShadowRoot()->SetAdoptedStyleSheetsForTesting(stylesheets);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(2u, GetTriggeredScopes("#host").size());

  // Insert a non-adopted stylesheet.
  host->AppendChild(MakeStyle("@scope { * { color: yellow; } }"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetTriggeredScopes("#a").size());
  EXPECT_EQ(3u, GetTriggeredScopes("#host").size());
}

}  // namespace blink
