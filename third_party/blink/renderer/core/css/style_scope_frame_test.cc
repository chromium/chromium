// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope_frame.h"

#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/css/style_scope_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class StyleScopeFrameTest : public PageTestBase {
 public:
  HeapVector<Member<const StyleScope>, 1> TriggeredScopes(Element& e) {
    if (StyleScopeData* style_scope_data = e.GetStyleScopeData()) {
      return style_scope_data->GetTriggeredScopes();
    }
    return HeapVector<Member<const StyleScope>, 1>();
  }
};

TEST_F(StyleScopeFrameTest, HasSeenImplicitScope) {
  SetBodyInnerHTML(R"HTML(
    <div id=a>
      <div id=b>
        <div id=c>
        </div>
      </div>
    </div>
    <div id=d>
      <div id=e>
        <style>
          @scope {
            div { }
          }
        </style>
        <div id=f>
        </div>
      </div>
    </div>
  )HTML");

  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  Element* c = GetElementById("c");
  Element* d = GetElementById("d");
  Element* e = GetElementById("e");
  Element* f = GetElementById("f");

  ASSERT_TRUE(a && b && c && d && e && f);

  HeapVector<Member<const StyleScope>, 1> style_scopes = TriggeredScopes(*e);
  ASSERT_EQ(1u, style_scopes.size());
  const StyleScope* scope = style_scopes[0];
  ASSERT_TRUE(scope && scope->IsImplicit());

  // Check HasSeenImplicitScope with a single frame,
  // simulating a recalc rooted at that element.

  {
    StyleScopeFrame a_frame(*a);
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*scope));
  }

  {
    StyleScopeFrame b_frame(*b);
    EXPECT_FALSE(b_frame.HasSeenImplicitScope(*scope));
  }

  {
    StyleScopeFrame c_frame(*c);
    EXPECT_FALSE(c_frame.HasSeenImplicitScope(*scope));
  }

  {
    StyleScopeFrame d_frame(*d);
    EXPECT_FALSE(d_frame.HasSeenImplicitScope(*scope));
  }

  {
    StyleScopeFrame e_frame(*e);
    EXPECT_TRUE(e_frame.HasSeenImplicitScope(*scope));
  }

  {
    StyleScopeFrame f_frame(*f);
    EXPECT_TRUE(f_frame.HasSeenImplicitScope(*scope));
  }

  // Check HasSeenImplicitScope when we have StyleScopeFrames for more
  // of the ancestor chain.

  // #c, #a and #b already on the stack.
  {
    StyleScopeFrame a_frame(*a);
    StyleScopeFrame b_frame(*b, &a_frame);
    StyleScopeFrame c_frame(*c, &b_frame);
    EXPECT_FALSE(c_frame.HasSeenImplicitScope(*scope));
  }

  // #e, with #d already on the stack.
  {
    StyleScopeFrame d_frame(*d);
    StyleScopeFrame e_frame(*e, &d_frame);
    EXPECT_TRUE(e_frame.HasSeenImplicitScope(*scope));
  }

  // #f, with #c and #d already on the stack.
  {
    StyleScopeFrame d_frame(*d);
    StyleScopeFrame e_frame(*e, &d_frame);
    StyleScopeFrame f_frame(*f, &e_frame);
    EXPECT_TRUE(f_frame.HasSeenImplicitScope(*scope));
  }
}

TEST_F(StyleScopeFrameTest, HasSeenImplicitScope_Nested) {
  SetBodyInnerHTML(R"HTML(
    <div id=a>
      <div id=b>
        <style>
          @scope {
            div { }
            @scope {
              div { }
            }
          }
        </style>
        <div id=c>
        </div>
      </div>
    </div>
  )HTML");

  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  Element* c = GetElementById("c");

  ASSERT_TRUE(a && b && c);

  HeapVector<Member<const StyleScope>> style_scopes = TriggeredScopes(*b);
  ASSERT_EQ(2u, style_scopes.size());
  const StyleScope* outer_scope = style_scopes[0];
  ASSERT_TRUE(outer_scope && outer_scope->IsImplicit());
  const StyleScope* inner_scope = style_scopes[1];
  ASSERT_TRUE(inner_scope && inner_scope->IsImplicit());

  {
    StyleScopeFrame a_frame(*a);
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*outer_scope));
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*inner_scope));
  }

  {
    StyleScopeFrame b_frame(*b);
    EXPECT_TRUE(b_frame.HasSeenImplicitScope(*outer_scope));
    EXPECT_TRUE(b_frame.HasSeenImplicitScope(*inner_scope));
  }

  {
    StyleScopeFrame c_frame(*c);
    EXPECT_TRUE(c_frame.HasSeenImplicitScope(*outer_scope));
    EXPECT_TRUE(c_frame.HasSeenImplicitScope(*inner_scope));
  }

  {
    StyleScopeFrame a_frame(*a);
    StyleScopeFrame b_frame(*b, &a_frame);
    StyleScopeFrame c_frame(*c, &b_frame);
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*outer_scope));
    EXPECT_TRUE(b_frame.HasSeenImplicitScope(*outer_scope));
    EXPECT_TRUE(c_frame.HasSeenImplicitScope(*outer_scope));
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*inner_scope));
    EXPECT_TRUE(b_frame.HasSeenImplicitScope(*inner_scope));
    EXPECT_TRUE(c_frame.HasSeenImplicitScope(*inner_scope));
  }
}

TEST_F(StyleScopeFrameTest, HasSeenImplicitScope_Multi) {
  SetBodyInnerHTML(R"HTML(
    <div id=a>
      <div id=b>
        <style>
          @scope {
            div { }
          }
        </style>
        <div id=c>
        </div>
      </div>
    </div>
    <div id=d>
      <div id=e>
        <style>
          @scope {
            span { }
          }
        </style>
        <div id=f>
        </div>
      </div>
    </div>
  )HTML");

  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  Element* c = GetElementById("c");
  Element* d = GetElementById("d");
  Element* e = GetElementById("e");
  Element* f = GetElementById("f");

  ASSERT_TRUE(a && b && c && d && e && f);

  HeapVector<Member<const StyleScope>, 1> b_scopes = TriggeredScopes(*b);
  ASSERT_EQ(1u, b_scopes.size());
  const StyleScope* b_scope = b_scopes[0];
  ASSERT_TRUE(b_scope && b_scope->IsImplicit());

  HeapVector<Member<const StyleScope>, 1> e_scopes = TriggeredScopes(*e);
  ASSERT_EQ(1u, e_scopes.size());
  const StyleScope* e_scope = e_scopes[0];
  ASSERT_TRUE(e_scope && e_scope->IsImplicit());

  {
    StyleScopeFrame c_frame(*c);
    EXPECT_TRUE(c_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_FALSE(c_frame.HasSeenImplicitScope(*e_scope));
  }

  {
    StyleScopeFrame f_frame(*f);
    EXPECT_FALSE(f_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_TRUE(f_frame.HasSeenImplicitScope(*e_scope));
  }

  {
    StyleScopeFrame a_frame(*a);
    StyleScopeFrame b_frame(*b, &a_frame);
    StyleScopeFrame c_frame(*c, &b_frame);
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_FALSE(a_frame.HasSeenImplicitScope(*e_scope));
    EXPECT_TRUE(b_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_FALSE(b_frame.HasSeenImplicitScope(*e_scope));
    EXPECT_TRUE(c_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_FALSE(c_frame.HasSeenImplicitScope(*e_scope));
  }

  {
    StyleScopeFrame d_frame(*d);
    StyleScopeFrame e_frame(*e, &d_frame);
    StyleScopeFrame f_frame(*f, &e_frame);
    EXPECT_FALSE(d_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_FALSE(d_frame.HasSeenImplicitScope(*e_scope));
    EXPECT_FALSE(e_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_TRUE(e_frame.HasSeenImplicitScope(*e_scope));
    EXPECT_FALSE(f_frame.HasSeenImplicitScope(*b_scope));
    EXPECT_TRUE(f_frame.HasSeenImplicitScope(*e_scope));
  }
}

}  // namespace blink
