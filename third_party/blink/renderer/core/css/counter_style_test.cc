// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style.h"

#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class CounterStyleTest : public PageTestBase {
 protected:
  const CounterStyle& GetCounterStyle(const char* name) {
    AtomicString name_string(name);
    if (const CounterStyleMap* document_map =
            CounterStyleMap::GetAuthorCounterStyleMap(GetDocument())) {
      return *document_map->FindCounterStyleAcrossScopes(name_string);
    }
    return *CounterStyleMap::GetUACounterStyleMap()
                ->FindCounterStyleAcrossScopes(name_string);
  }

  const CounterStyle AddCounterStyle(const char* name,
                                     const String& descriptors) {
    StringBuilder declaration;
    declaration.Append("@counter-style ");
    declaration.Append(name);
    declaration.Append("{");
    declaration.Append(descriptors);
    declaration.Append("}");
    InsertStyleElement(declaration.ToString().Utf8());
    UpdateAllLifecyclePhasesForTest();
    return GetCounterStyle(name);
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

  // Fallback to cjk-decimal
  EXPECT_EQ("-1", eb.GenerateRepresentation(-1));
  EXPECT_EQ(String(u"\u3007"), eb.GenerateRepresentation(0));
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

TEST_F(CounterStyleTest, CustomFirstSymbolValue) {
  InsertStyleElement(R"CSS(
    @counter-style base {
      system: fixed 2;
      symbols: A B C;
    }

    @counter-style extended {
      system: extends base;
    }
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  // Getting custom first symbol value directly from descriptor value.
  const CounterStyle& base = GetCounterStyle("base");
  EXPECT_EQ("1", base.GenerateRepresentation(1));
  EXPECT_EQ("A", base.GenerateRepresentation(2));
  EXPECT_EQ("B", base.GenerateRepresentation(3));
  EXPECT_EQ("C", base.GenerateRepresentation(4));
  EXPECT_EQ("5", base.GenerateRepresentation(5));

  // Getting custom first symbol value indirectly using 'extends'.
  const CounterStyle& extended = GetCounterStyle("extended");
  EXPECT_EQ("1", extended.GenerateRepresentation(1));
  EXPECT_EQ("A", extended.GenerateRepresentation(2));
  EXPECT_EQ("B", extended.GenerateRepresentation(3));
  EXPECT_EQ("C", extended.GenerateRepresentation(4));
  EXPECT_EQ("5", extended.GenerateRepresentation(5));
}

TEST_F(CounterStyleTest, ExtremeValuesCyclic) {
  const CounterStyle& cyclic =
      AddCounterStyle("cyclic", "system: cyclic; symbols: A B C;");
  EXPECT_EQ("A",
            cyclic.GenerateRepresentation(std::numeric_limits<int>::min()));
  EXPECT_EQ("A",
            cyclic.GenerateRepresentation(std::numeric_limits<int>::max()));
}

TEST_F(CounterStyleTest, ExtremeValuesNumeric) {
  const CounterStyle& numeric =
      AddCounterStyle("numeric",
                      "system: numeric; symbols: '0' '1' '2' '3' '4' '5' '6' "
                      "'7' '8' '9' A B C D E F");
  EXPECT_EQ("-80000000",
            numeric.GenerateRepresentation(std::numeric_limits<int>::min()));
  EXPECT_EQ("7FFFFFFF",
            numeric.GenerateRepresentation(std::numeric_limits<int>::max()));
}

TEST_F(CounterStyleTest, ExtremeValuesAlphabetic) {
  const CounterStyle& alphabetic = AddCounterStyle(
      "alphabetic",
      "system: alphabetic; symbols: A B C; range: infinite infinite;");
  EXPECT_EQ("-ABAABABBBAACCCACACCB",
            alphabetic.GenerateRepresentation(std::numeric_limits<int>::min()));
  EXPECT_EQ("ABAABABBBAACCCACACCA",
            alphabetic.GenerateRepresentation(std::numeric_limits<int>::max()));
}

TEST_F(CounterStyleTest, ExtremeValuesAdditive) {
  const CounterStyle& additive =
      AddCounterStyle("additive",
                      "system: additive; range: infinite infinite;"
                      "additive-symbols: 2000000000 '2B',"
                      "                   100000000 '1CM',"
                      "                    40000000 '4DM',"
                      "                     7000000 '7M',"
                      "                      400000 '4CK',"
                      "                       80000 '8DK',"
                      "                        3000 '3K',"
                      "                         600 '6C',"
                      "                          40 '4D',"
                      "                           8 '8I',"
                      "                           7 '7I';");
  EXPECT_EQ("-2B1CM4DM7M4CK8DK3K6C4D8I",
            additive.GenerateRepresentation(std::numeric_limits<int>::min()));
  EXPECT_EQ("2B1CM4DM7M4CK8DK3K6C4D7I",
            additive.GenerateRepresentation(std::numeric_limits<int>::max()));
}

TEST_F(CounterStyleTest, ExtremeValuesSymbolic) {
  // No symbolic counter style can possibly represent such large values without
  // exceeding the length limit. Always fallbacks to 'decimal'.
  const CounterStyle& symbolic = AddCounterStyle(
      "symbolic",
      "system: symbolic; symbols: A B C; range: infinite infinite;");
  EXPECT_EQ("-2147483648",
            symbolic.GenerateRepresentation(std::numeric_limits<int>::min()));
  EXPECT_EQ("2147483647",
            symbolic.GenerateRepresentation(std::numeric_limits<int>::max()));
}

TEST_F(CounterStyleTest, ExtremeValuesFixed) {
  const CounterStyle& fixed =
      AddCounterStyle("fixed", "system: fixed 2147483646; symbols: A B C D;");
  // An int subtraction would overflow and return 2 as the result.
  EXPECT_EQ("-2147483648",
            fixed.GenerateRepresentation(std::numeric_limits<int>::min()));
  EXPECT_EQ("B", fixed.GenerateRepresentation(std::numeric_limits<int>::max()));
}

TEST_F(CounterStyleTest, PrefixAndSuffix) {
  const CounterStyle& base = AddCounterStyle(
      "base", "system: symbolic; symbols: A; prefix: X; suffix: Y;");
  EXPECT_EQ("X", base.GetPrefix());
  EXPECT_EQ("Y", base.GetSuffix());

  const CounterStyle& extended =
      AddCounterStyle("extended", "system: extends base");
  EXPECT_EQ("X", extended.GetPrefix());
  EXPECT_EQ("Y", extended.GetSuffix());
}

TEST_F(CounterStyleTest, Hebrew) {
  // Verifies that our 'hebrew' implementation matches the spec in the
  // officially specified range 1-10999.
  // https://drafts.csswg.org/css-counter-styles-3/#hebrew
  const CounterStyle& hebrew_as_specced =
      AddCounterStyle("hebrew-as-specced", R"CSS(
    system: additive;
    range: 1 10999;
    additive-symbols: 10000 \5D9\5F3, 9000 \5D8\5F3, 8000 \5D7\5F3, 7000 \5D6\5F3, 6000 \5D5\5F3, 5000 \5D4\5F3, 4000 \5D3\5F3, 3000 \5D2\5F3, 2000 \5D1\5F3, 1000 \5D0\5F3, 400 \5EA, 300 \5E9, 200 \5E8, 100 \5E7, 90 \5E6, 80 \5E4, 70 \5E2, 60 \5E1, 50 \5E0, 40 \5DE, 30 \5DC, 20 \5DB, 19 \5D9\5D8, 18 \5D9\5D7, 17 \5D9\5D6, 16 \5D8\5D6, 15 \5D8\5D5, 10 \5D9, 9 \5D8, 8 \5D7, 7 \5D6, 6 \5D5, 5 \5D4, 4 \5D3, 3 \5D2, 2 \5D1, 1 \5D0;
  )CSS");
  const CounterStyle& hebrew_as_implemented = GetCounterStyle("hebrew");
  for (int value = 1; value <= 10999; ++value) {
    String expected = hebrew_as_specced.GenerateRepresentation(value);
    String actual = hebrew_as_implemented.GenerateRepresentation(value);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(CounterStyleTest, LowerArmenian) {
  // Verifies that our 'lower-armenian' implementation matches the spec in the
  // officially specified range 1-9999.
  // https://drafts.csswg.org/css-counter-styles-3/#valdef-counter-style-name-lower-armenian
  const CounterStyle& lower_armenian_as_specced =
      AddCounterStyle("lower-armenian-as-specced", R"CSS(
    system: additive;
    range: 1 9999;
    additive-symbols: 9000 "\584", 8000 "\583", 7000 "\582", 6000 "\581", 5000 "\580", 4000 "\57F", 3000 "\57E", 2000 "\57D", 1000 "\57C", 900 "\57B", 800 "\57A", 700 "\579", 600 "\578", 500 "\577", 400 "\576", 300 "\575", 200 "\574", 100 "\573", 90 "\572", 80 "\571", 70 "\570", 60 "\56F", 50 "\56E", 40 "\56D", 30 "\56C", 20 "\56B", 10 "\56A", 9 "\569", 8 "\568", 7 "\567", 6 "\566", 5 "\565", 4 "\564", 3 "\563", 2 "\562", 1 "\561";
  )CSS");
  const CounterStyle& lower_armenian_as_implemented =
      GetCounterStyle("lower-armenian");
  for (int value = 1; value <= 9999; ++value) {
    String expected = lower_armenian_as_specced.GenerateRepresentation(value);
    String actual = lower_armenian_as_implemented.GenerateRepresentation(value);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(CounterStyleTest, UpperArmenian) {
  // Verifies that our 'upper-armenian' implementation matches the spec in the
  // officially specified range 1-9999.
  // https://drafts.csswg.org/css-counter-styles-3/#valdef-counter-style-name-upper-armenian
  const CounterStyle& upper_armenian_as_specced =
      AddCounterStyle("upper-armenian-as-specced", R"CSS(
    system: additive;
    range: 1 9999;
    additive-symbols: 9000 \554, 8000 \553, 7000 \552, 6000 \551, 5000 \550, 4000 \54F, 3000 \54E, 2000 \54D, 1000 \54C, 900 \54B, 800 \54A, 700 \549, 600 \548, 500 \547, 400 \546, 300 \545, 200 \544, 100 \543, 90 \542, 80 \541, 70 \540, 60 \53F, 50 \53E, 40 \53D, 30 \53C, 20 \53B, 10 \53A, 9 \539, 8 \538, 7 \537, 6 \536, 5 \535, 4 \534, 3 \533, 2 \532, 1 \531;
  )CSS");
  const CounterStyle& upper_armenian_as_implemented =
      GetCounterStyle("upper-armenian");
  for (int value = 1; value <= 9999; ++value) {
    String expected = upper_armenian_as_specced.GenerateRepresentation(value);
    String actual = upper_armenian_as_implemented.GenerateRepresentation(value);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(CounterStyleTest, ExtendArmenianRangeToIncludeZero) {
  // 'lower-armenian' and 'upper-armenian' counter styles cannot represent 0.
  // Even if we extend them to include 0 into the range, we still fall back.
  const CounterStyle& extends_lower_armenian =
      AddCounterStyle("extends-lower-armenian", R"CSS(
    system: extends lower-armenian;
    range: 0 infinity;
  )CSS");
  EXPECT_EQ("0", extends_lower_armenian.GenerateRepresentation(0));

  const CounterStyle& extends_upper_armenian =
      AddCounterStyle("extends-upper-armenian", R"CSS(
    system: extends upper-armenian;
    range: 0 infinity;
  )CSS");
  EXPECT_EQ("0", extends_upper_armenian.GenerateRepresentation(0));
}

TEST_F(CounterStyleTest, ExtendArmenianRangeToAuto) {
  // 'lower-armenian' and 'upper-armenian' counter styles cannot represent 0,
  // even if we extend their range to 'auto'.
  const CounterStyle& extends_lower_armenian =
      AddCounterStyle("extends-lower-armenian", R"CSS(
    system: extends lower-armenian;
    range: auto;
  )CSS");
  EXPECT_EQ("0", extends_lower_armenian.GenerateRepresentation(0));

  const CounterStyle& extends_upper_armenian =
      AddCounterStyle("extends-upper-armenian", R"CSS(
    system: extends upper-armenian;
    range: 0 auto;
  )CSS");
  EXPECT_EQ("0", extends_upper_armenian.GenerateRepresentation(0));
}

TEST_F(CounterStyleTest, KoreanHangulFormal) {
  // Verifies that our 'korean-hangul-formal' implementation matches the spec in
  // the officially specified range 1-9999.
  // https://drafts.csswg.org/css-counter-styles-3/#korean-hangul-formal
  const CounterStyle& korean_hangul_formal_as_specced =
      AddCounterStyle("korean-hangul-formal-as-specced", R"CSS(
    system: additive;
    range: -9999 9999;
    additive-symbols: 9000 \AD6C\CC9C, 8000 \D314\CC9C, 7000 \CE60\CC9C, 6000 \C721\CC9C, 5000 \C624\CC9C, 4000 \C0AC\CC9C, 3000 \C0BC\CC9C, 2000 \C774\CC9C, 1000 \C77C\CC9C, 900 \AD6C\BC31, 800 \D314\BC31, 700 \CE60\BC31, 600 \C721\BC31, 500 \C624\BC31, 400 \C0AC\BC31, 300 \C0BC\BC31, 200 \C774\BC31, 100 \C77C\BC31, 90 \AD6C\C2ED, 80 \D314\C2ED, 70 \CE60\C2ED, 60 \C721\C2ED, 50 \C624\C2ED, 40 \C0AC\C2ED, 30 \C0BC\C2ED, 20 \C774\C2ED, 10 \C77C\C2ED, 9 \AD6C, 8 \D314, 7 \CE60, 6 \C721, 5 \C624, 4 \C0AC, 3 \C0BC, 2 \C774, 1 \C77C, 0 \C601;
    negative: "\B9C8\C774\B108\C2A4  ";
  )CSS");
  const CounterStyle& korean_hangul_formal_as_implemented =
      GetCounterStyle("korean-hangul-formal");
  for (int value = -9999; value <= 9999; ++value) {
    String expected =
        korean_hangul_formal_as_specced.GenerateRepresentation(value);
    String actual =
        korean_hangul_formal_as_implemented.GenerateRepresentation(value);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(CounterStyleTest, KoreanHanjaFormal) {
  // Verifies that our 'korean-hanja-formal' implementation matches the spec in
  // the officially specified range 1-9999.
  // https://drafts.csswg.org/css-counter-styles-3/#korean-hanja-formal
  const CounterStyle& korean_hanja_formal_as_specced =
      AddCounterStyle("korean-hanja-formal-as-specced", R"CSS(
    system: additive;
    range: -9999 9999;
    additive-symbols: 9000 \4E5D\4EDF, 8000 \516B\4EDF, 7000 \4E03\4EDF, 6000 \516D\4EDF, 5000 \4E94\4EDF, 4000 \56DB\4EDF, 3000 \53C3\4EDF, 2000 \8CB3\4EDF, 1000 \58F9\4EDF, 900 \4E5D\767E, 800 \516B\767E, 700 \4E03\767E, 600 \516D\767E, 500 \4E94\767E, 400 \56DB\767E, 300 \53C3\767E, 200 \8CB3\767E, 100 \58F9\767E, 90 \4E5D\62FE, 80 \516B\62FE, 70 \4E03\62FE, 60 \516D\62FE, 50 \4E94\62FE, 40 \56DB\62FE, 30 \53C3\62FE, 20 \8CB3\62FE, 10 \58F9\62FE, 9 \4E5D, 8 \516B, 7 \4E03, 6 \516D, 5 \4E94, 4 \56DB, 3 \53C3, 2 \8CB3, 1 \58F9, 0 \96F6;
    negative: "\B9C8\C774\B108\C2A4  ";
  )CSS");
  const CounterStyle& korean_hanja_formal_as_implemented =
      GetCounterStyle("korean-hanja-formal");
  for (int value = -9999; value <= 9999; ++value) {
    String expected =
        korean_hanja_formal_as_specced.GenerateRepresentation(value);
    String actual =
        korean_hanja_formal_as_implemented.GenerateRepresentation(value);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(CounterStyleTest, KoreanHanjaInformal) {
  // Verifies that our 'korean-hanja-informal' implementation matches the spec
  // in the officially specified range 1-9999.
  // https://drafts.csswg.org/css-counter-styles-3/#korean-hanja-informal
  const CounterStyle& korean_hanja_informal_as_specced =
      AddCounterStyle("korean-hanja-informal-as-specced", R"CSS(
    system: additive;
    range: -9999 9999;
    additive-symbols: 9000 \4E5D\5343, 8000 \516B\5343, 7000 \4E03\5343, 6000 \516D\5343, 5000 \4E94\5343, 4000 \56DB\5343, 3000 \4E09\5343, 2000 \4E8C\5343, 1000 \5343, 900 \4E5D\767E, 800 \516B\767E, 700 \4E03\767E, 600 \516D\767E, 500 \4E94\767E, 400 \56DB\767E, 300 \4E09\767E, 200 \4E8C\767E, 100 \767E, 90 \4E5D\5341, 80 \516B\5341, 70 \4E03\5341, 60 \516D\5341, 50 \4E94\5341, 40 \56DB\5341, 30 \4E09\5341, 20 \4E8C\5341, 10 \5341, 9 \4E5D, 8 \516B, 7 \4E03, 6 \516D, 5 \4E94, 4 \56DB, 3 \4E09, 2 \4E8C, 1 \4E00, 0 \96F6;
    negative: "\B9C8\C774\B108\C2A4  ";
  )CSS");
  const CounterStyle& korean_hanja_informal_as_implemented =
      GetCounterStyle("korean-hanja-informal");
  for (int value = -9999; value <= 9999; ++value) {
    String expected =
        korean_hanja_informal_as_specced.GenerateRepresentation(value);
    String actual =
        korean_hanja_informal_as_implemented.GenerateRepresentation(value);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(CounterStyleTest, EthiopicNumeric) {
  const CounterStyle& style = GetCounterStyle("ethiopic-numeric");
  EXPECT_EQ(String(u"\u1369"), style.GenerateRepresentation(1));
  EXPECT_EQ(String(u"\u136A"), style.GenerateRepresentation(2));
  EXPECT_EQ(String(u"\u136B"), style.GenerateRepresentation(3));
  EXPECT_EQ(String(u"\u136C"), style.GenerateRepresentation(4));
  EXPECT_EQ(String(u"\u136D"), style.GenerateRepresentation(5));
  EXPECT_EQ(String(u"\u136E"), style.GenerateRepresentation(6));
  EXPECT_EQ(String(u"\u136F"), style.GenerateRepresentation(7));
  EXPECT_EQ(String(u"\u1370"), style.GenerateRepresentation(8));
  EXPECT_EQ(String(u"\u1371"), style.GenerateRepresentation(9));
  EXPECT_EQ(String(u"\u1372"), style.GenerateRepresentation(10));
  EXPECT_EQ(String(u"\u1372\u1369"), style.GenerateRepresentation(11));
  EXPECT_EQ(String(u"\u1372\u136A"), style.GenerateRepresentation(12));
  EXPECT_EQ(String(u"\u1375\u136B"), style.GenerateRepresentation(43));
  EXPECT_EQ(String(u"\u1378\u136F"), style.GenerateRepresentation(77));
  EXPECT_EQ(String(u"\u1379"), style.GenerateRepresentation(80));
  EXPECT_EQ(String(u"\u137A\u1371"), style.GenerateRepresentation(99));
  EXPECT_EQ(String(u"\u137B"), style.GenerateRepresentation(100));
  EXPECT_EQ(String(u"\u137B\u1369"), style.GenerateRepresentation(101));
  EXPECT_EQ(String(u"\u136A\u137B\u1373\u136A"),
            style.GenerateRepresentation(222));
  EXPECT_EQ(String(u"\u136D\u137B\u1375"), style.GenerateRepresentation(540));
  EXPECT_EQ(String(u"\u1371\u137B\u137A\u1371"),
            style.GenerateRepresentation(999));
  EXPECT_EQ(String(u"\u1372\u137B"), style.GenerateRepresentation(1000));
  EXPECT_EQ(String(u"\u1372\u137B\u136D"), style.GenerateRepresentation(1005));
  EXPECT_EQ(String(u"\u1372\u137B\u1377"), style.GenerateRepresentation(1060));
  EXPECT_EQ(String(u"\u1372\u137B\u1377\u136D"),
            style.GenerateRepresentation(1065));
  EXPECT_EQ(String(u"\u1372\u1370\u137B"), style.GenerateRepresentation(1800));
  EXPECT_EQ(String(u"\u1372\u1370\u137B\u1377"),
            style.GenerateRepresentation(1860));
  EXPECT_EQ(String(u"\u1372\u1370\u137B\u1377\u136D"),
            style.GenerateRepresentation(1865));
  EXPECT_EQ(String(u"\u1376\u1370\u137B\u1377\u136D"),
            style.GenerateRepresentation(5865));
  EXPECT_EQ(String(u"\u1378\u137B\u136D"), style.GenerateRepresentation(7005));
  EXPECT_EQ(String(u"\u1378\u1370\u137B"), style.GenerateRepresentation(7800));
  EXPECT_EQ(String(u"\u1378\u1370\u137B\u1377\u136C"),
            style.GenerateRepresentation(7864));
  EXPECT_EQ(String(u"\u137A\u1371\u137B\u137A\u1371"),
            style.GenerateRepresentation(9999));
  EXPECT_EQ(String(u"\u137C"), style.GenerateRepresentation(10000));
  EXPECT_EQ(String(u"\u1378\u1370\u137B\u1369\u137C\u137A\u136A"),
            style.GenerateRepresentation(78010092));
  EXPECT_EQ(String(u"\u137B\u137C\u1369"),
            style.GenerateRepresentation(1000001));
}

TEST_F(CounterStyleTest, GenerateTextAlternativeSpeakAsDisabled) {
  ScopedCSSAtRuleCounterStyleSpeakAsDescriptorForTest disabled(false);

  AddCounterStyle("base", R"CSS(
    system: fixed;
    symbols: 'One' 'Two' 'Three';
    suffix: '. ';
  )CSS");

  const CounterStyle& bullets = AddCounterStyle("bullets", R"CSS(
    system: extends base;
    speak-as: bullets;
  )CSS");
  EXPECT_EQ("One. ", bullets.GenerateTextAlternative(1));
  EXPECT_EQ("Two. ", bullets.GenerateTextAlternative(2));
  EXPECT_EQ("Three. ", bullets.GenerateTextAlternative(3));

  const CounterStyle& numbers = AddCounterStyle("numbers", R"CSS(
    system: extends base;
    speak-as: numbers;
  )CSS");
  EXPECT_EQ("One. ", numbers.GenerateTextAlternative(1));
  EXPECT_EQ("Two. ", numbers.GenerateTextAlternative(2));
  EXPECT_EQ("Three. ", numbers.GenerateTextAlternative(3));

  const CounterStyle& words = AddCounterStyle("words", R"CSS(
    system: extends base;
    speak-as: words;
  )CSS");
  EXPECT_EQ("One. ", words.GenerateTextAlternative(1));
  EXPECT_EQ("Two. ", words.GenerateTextAlternative(2));
  EXPECT_EQ("Three. ", words.GenerateTextAlternative(3));
}

}  // namespace blink
