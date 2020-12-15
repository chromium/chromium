// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style.h"

#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CounterStyleTest : public PageTestBase,
                         private ScopedCSSAtRuleCounterStyleForTest {
 public:
  CounterStyleTest() : ScopedCSSAtRuleCounterStyleForTest(true) {}

 protected:
  const CounterStyle& GetCounterStyle(const AtomicString& name) {
    if (const CounterStyleMap* document_map =
            CounterStyleMap::GetAuthorCounterStyleMap(GetDocument()))
      return document_map->FindCounterStyleAcrossScopes(name);
    return CounterStyleMap::GetUACounterStyleMap()
        ->FindCounterStyleAcrossScopes(name);
  }
};

TEST_F(CounterStyleTest, NumericAlgorithm) {
  const CounterStyle& decimal = GetCounterStyle("decimal");
  EXPECT_EQ("-123", decimal.GenerateRepresentation(-123));
  EXPECT_EQ("0", decimal.GenerateRepresentation(0));
  EXPECT_EQ("456", decimal.GenerateRepresentation(456));
}

TEST_F(CounterStyleTest, AdditiveAlgorithm) {
  const CounterStyle& upper_roman = GetCounterStyle("upper-roman");
  EXPECT_EQ("I", upper_roman.GenerateRepresentation(1));
  EXPECT_EQ("CDXLIV", upper_roman.GenerateRepresentation(444));
  EXPECT_EQ("MMMCMXCIX", upper_roman.GenerateRepresentation(3999));

  // Can't represent 0. Fallback to 'decimal'.
  EXPECT_EQ("0", upper_roman.GenerateRepresentation(0));
}

TEST_F(CounterStyleTest, ExtendsAdditive) {
  InsertStyleElement("@counter-style foo { system: extends upper-roman; }");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle("foo");
  EXPECT_EQ("I", foo.GenerateRepresentation(1));
  EXPECT_EQ("CDXLIV", foo.GenerateRepresentation(444));
  EXPECT_EQ("MMMCMXCIX", foo.GenerateRepresentation(3999));

  // Can't represent 0. Fallback to 'decimal'.
  EXPECT_EQ("0", foo.GenerateRepresentation(0));
}

TEST_F(CounterStyleTest, AdditiveLengthLimit) {
  InsertStyleElement(
      "@counter-style foo { system: additive; additive-symbols: 1 I; }");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle("foo");
  EXPECT_EQ("I", foo.GenerateRepresentation(1));
  EXPECT_EQ("II", foo.GenerateRepresentation(2));
  EXPECT_EQ("III", foo.GenerateRepresentation(3));

  // Length limit exceeded. Fallback to 'decimal'.
  EXPECT_EQ("1000000", foo.GenerateRepresentation(1000000));
}

TEST_F(CounterStyleTest, AdditiveWithZero) {
  InsertStyleElement(
      "@counter-style foo { system: additive; additive-symbols: 1 I, 0 O; }");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle("foo");
  EXPECT_EQ("O", foo.GenerateRepresentation(0));
  EXPECT_EQ("I", foo.GenerateRepresentation(1));
  EXPECT_EQ("II", foo.GenerateRepresentation(2));
  EXPECT_EQ("III", foo.GenerateRepresentation(3));
}

TEST_F(CounterStyleTest, AlphabeticAlgorithm) {
  const CounterStyle& lower_alpha = GetCounterStyle("lower-alpha");
  EXPECT_EQ("a", lower_alpha.GenerateRepresentation(1));
  EXPECT_EQ("ab", lower_alpha.GenerateRepresentation(28));
  EXPECT_EQ("cab", lower_alpha.GenerateRepresentation(26 + 26 * 26 * 3 + 2));
}

TEST_F(CounterStyleTest, CyclicAlgorithm) {
  InsertStyleElement("@counter-style foo { system: cyclic; symbols: A B C; }");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle("foo");
  EXPECT_EQ(String("B"), foo.GenerateRepresentation(-100));
  EXPECT_EQ(String("B"), foo.GenerateRepresentation(-1));
  EXPECT_EQ(String("C"), foo.GenerateRepresentation(0));
  EXPECT_EQ(String("A"), foo.GenerateRepresentation(1));
  EXPECT_EQ(String("B"), foo.GenerateRepresentation(2));
  EXPECT_EQ(String("C"), foo.GenerateRepresentation(3));
  EXPECT_EQ(String("A"), foo.GenerateRepresentation(4));
  EXPECT_EQ(String("A"), foo.GenerateRepresentation(100));
}

TEST_F(CounterStyleTest, FixedAlgorithm) {
  const CounterStyle& eb = GetCounterStyle("cjk-earthly-branch");
  EXPECT_EQ(String(u"\u5B50"), eb.GenerateRepresentation(1));
  EXPECT_EQ(String(u"\u4EA5"), eb.GenerateRepresentation(12));

  // Fallback to decimal
  EXPECT_EQ("-1", eb.GenerateRepresentation(-1));
  EXPECT_EQ("0", eb.GenerateRepresentation(0));
  EXPECT_EQ("13", eb.GenerateRepresentation(13));
}

TEST_F(CounterStyleTest, SymbolicAlgorithm) {
  InsertStyleElement(R"HTML(
    @counter-style upper-alpha-legal {
      system: symbolic;
      symbols: A B C D E F G H I J K L M
               N O P Q R S T U V W X Y Z;
    }
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  const CounterStyle& legal = GetCounterStyle("upper-alpha-legal");

  EXPECT_EQ("A", legal.GenerateRepresentation(1));
  EXPECT_EQ("BB", legal.GenerateRepresentation(28));
  EXPECT_EQ("CCC", legal.GenerateRepresentation(55));

  // Length limit exceeded. Fallback to 'decimal'.
  EXPECT_EQ("1000000", legal.GenerateRepresentation(1000000));
}

TEST_F(CounterStyleTest, CyclicFallback) {
  InsertStyleElement(R"HTML(
    @counter-style foo {
      system: fixed;
      symbols: A B;
      fallback: bar;
    }

    @counter-style bar {
      system: fixed;
      symbols: C D E F;
      fallback: baz;
    }

    @counter-style baz {
      system: additive;
      additive-symbols: 5 V;
      fallback: foo;
    }
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const CounterStyle& foo = GetCounterStyle("foo");
  const CounterStyle& bar = GetCounterStyle("bar");
  const CounterStyle& baz = GetCounterStyle("baz");

  // foo -> bar
  EXPECT_EQ("E", foo.GenerateRepresentation(3));

  // bar -> baz
  EXPECT_EQ("V", bar.GenerateRepresentation(5));

  // baz -> foo
  EXPECT_EQ("A", baz.GenerateRepresentation(1));

  // baz -> foo -> bar
  EXPECT_EQ("F", baz.GenerateRepresentation(4));

  // foo -> bar -> baz -> foo. Break fallback cycle with 'decimal'.
  EXPECT_EQ("6", foo.GenerateRepresentation(6));
}

TEST_F(CounterStyleTest, CustomNegative) {
  InsertStyleElement(R"CSS(
    @counter-style financial-decimal {
      system: extends decimal;
      negative: '(' ')';
    }

    @counter-style extended {
      system: extends financial-decimal;
    }
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  // Getting custom 'negative' directly from descriptor value.
  const CounterStyle& financial_decimal = GetCounterStyle("financial-decimal");
  EXPECT_EQ("(999)", financial_decimal.GenerateRepresentation(-999));
  EXPECT_EQ("(1)", financial_decimal.GenerateRepresentation(-1));
  EXPECT_EQ("0", financial_decimal.GenerateRepresentation(0));
  EXPECT_EQ("1", financial_decimal.GenerateRepresentation(1));
  EXPECT_EQ("99", financial_decimal.GenerateRepresentation(99));

  // Getting custom 'negative' indirectly by extending a counter style.
  const CounterStyle& extended = GetCounterStyle("extended");
  EXPECT_EQ("(999)", extended.GenerateRepresentation(-999));
  EXPECT_EQ("(1)", extended.GenerateRepresentation(-1));
  EXPECT_EQ("0", extended.GenerateRepresentation(0));
  EXPECT_EQ("1", extended.GenerateRepresentation(1));
  EXPECT_EQ("99", extended.GenerateRepresentation(99));
}

TEST_F(CounterStyleTest, CustomPad) {
  InsertStyleElement(R"CSS(
    @counter-style financial-decimal-pad {
      system: extends decimal;
      negative: '(' ')';
      pad: 4 '0';
    }

    @counter-style extended {
      system: extends financial-decimal-pad;
    }
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  // Getting custom 'pad' directly from descriptor value.
  const CounterStyle& financial_decimal_pad =
      GetCounterStyle("financial-decimal-pad");
  EXPECT_EQ("(99)", financial_decimal_pad.GenerateRepresentation(-99));
  EXPECT_EQ("(01)", financial_decimal_pad.GenerateRepresentation(-1));
  EXPECT_EQ("0000", financial_decimal_pad.GenerateRepresentation(0));
  EXPECT_EQ("0001", financial_decimal_pad.GenerateRepresentation(1));
  EXPECT_EQ("0099", financial_decimal_pad.GenerateRepresentation(99));

  // Getting custom 'pad' indirectly by extending a counter style.
  const CounterStyle& extended = GetCounterStyle("extended");
  EXPECT_EQ("(99)", extended.GenerateRepresentation(-99));
  EXPECT_EQ("(01)", extended.GenerateRepresentation(-1));
  EXPECT_EQ("0000", extended.GenerateRepresentation(0));
  EXPECT_EQ("0001", extended.GenerateRepresentation(1));
  EXPECT_EQ("0099", extended.GenerateRepresentation(99));
}

TEST_F(CounterStyleTest, PadLengthLimit) {
  InsertStyleElement(R"CSS(
    @counter-style foo {
      system: extends decimal;
      pad: 1000 '0';
    }
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  // Pad length is too long. Fallback to 'decimal'.
  const CounterStyle& foo = GetCounterStyle("foo");
  EXPECT_EQ("0", foo.GenerateRepresentation(0));
}

TEST_F(CounterStyleTest, SymbolicWithExtendedRange) {
  InsertStyleElement(R"CSS(
    @counter-style base {
      system: symbolic;
      symbols: A B;
    }

    @counter-style custom {
      system: extends base;
      range: infinite -2, 0 infinite;
    }

    @counter-style extended {
      system: extends custom;
    }
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  // Getting custom 'range' directly from descriptor value.
  const CounterStyle& custom = GetCounterStyle("custom");
  EXPECT_EQ("-AA", custom.GenerateRepresentation(-3));
  EXPECT_EQ("-B", custom.GenerateRepresentation(-2));
  // -1 is out of 'range' value. Fallback to 'decimal'
  EXPECT_EQ("-1", custom.GenerateRepresentation(-1));
  // 0 is within 'range' but not representable. Fallback to 'decimal'.
  EXPECT_EQ("0", custom.GenerateRepresentation(0));
  EXPECT_EQ("A", custom.GenerateRepresentation(1));

  // Getting custom 'range' indirectly by extending a counter style.
  const CounterStyle& extended = GetCounterStyle("extended");
  EXPECT_EQ("-AA", extended.GenerateRepresentation(-3));
  EXPECT_EQ("-B", extended.GenerateRepresentation(-2));
  EXPECT_EQ("-1", extended.GenerateRepresentation(-1));
  EXPECT_EQ("0", extended.GenerateRepresentation(0));
  EXPECT_EQ("A", extended.GenerateRepresentation(1));
}

TEST_F(CounterStyleTest, AdditiveWithExtendedRange) {
  InsertStyleElement(R"CSS(
    @counter-style base {
      system: additive;
      additive-symbols: 2 B, 1 A;
    }

    @counter-style custom {
      system: extends base;
      range: infinite -2, 0 infinite;
    }

    @counter-style extended {
      system: extends custom;
    }
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  // Getting custom 'range' directly from descriptor value.
  const CounterStyle& custom = GetCounterStyle("custom");
  EXPECT_EQ("-BA", custom.GenerateRepresentation(-3));
  EXPECT_EQ("-B", custom.GenerateRepresentation(-2));
  // -1 is out of 'range' value. Fallback to 'decimal'.
  EXPECT_EQ("-1", custom.GenerateRepresentation(-1));
  // 0 is within 'range' but not representable. Fallback to 'decimal'.
  EXPECT_EQ("0", custom.GenerateRepresentation(0));
  EXPECT_EQ("A", custom.GenerateRepresentation(1));

  // Getting custom 'range' indirectly by extending a counter style.
  const CounterStyle& extended = GetCounterStyle("extended");
  EXPECT_EQ("-BA", extended.GenerateRepresentation(-3));
  EXPECT_EQ("-B", extended.GenerateRepresentation(-2));
  EXPECT_EQ("-1", extended.GenerateRepresentation(-1));
  EXPECT_EQ("0", extended.GenerateRepresentation(0));
  EXPECT_EQ("A", extended.GenerateRepresentation(1));
}

}  // namespace blink
