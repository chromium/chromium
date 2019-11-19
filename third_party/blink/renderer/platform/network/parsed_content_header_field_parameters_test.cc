// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/parsed_content_header_field_parameters.h"
#include "third_party/blink/renderer/platform/network/header_field_tokenizer.h"
#include "third_party/blink/renderer/platform/network/parsed_content_disposition.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentHeaderFieldParameters::Mode;

void CheckValidity(bool expected,
                   const String& input,
                   Mode mode = Mode::kNormal) {
  EXPECT_EQ(expected, !!ParsedContentHeaderFieldParameters::Parse(
                          HeaderFieldTokenizer(input), mode))
      << input;

  const String disposition_input = "attachment" + input;
  EXPECT_EQ(expected,
            ParsedContentDisposition(disposition_input, mode).IsValid())
      << disposition_input;

  const String type_input = "text/plain" + input;
  EXPECT_EQ(expected, ParsedContentType(type_input, mode).IsValid())
      << type_input;
}

TEST(ParsedContentHeaderFieldParametersTest, Validity) {
  CheckValidity(true, "");
  CheckValidity(true, "  ");
  CheckValidity(true, "\t");
  CheckValidity(true, "  ;p1=v1");
  CheckValidity(true, "\t;p1=v1");
  CheckValidity(true, ";  p1=v1");
  CheckValidity(true, ";\tp1=v1");
  CheckValidity(true, ";p1=v1  ");
  CheckValidity(true, ";p1=v1\t");
  CheckValidity(true, ";p1 = v1");
  CheckValidity(true, ";p1\t=\tv1");
  CheckValidity(true, ";  p1  =  v1  ");
  CheckValidity(true, ";\tp1\t=\tv1\t");
  CheckValidity(true, ";z=\"ttx&r=z;;\\u\\\"kd==\"");
  CheckValidity(true, "; z=\"\xff\"");

  CheckValidity(false, "\r");
  CheckValidity(false, "\n");
  CheckValidity(false, " p1=v1");
  CheckValidity(false, "\tp1=v1");
  CheckValidity(false, ";p1=v1;");
  CheckValidity(false, ";");
  CheckValidity(false, ";  ");
  CheckValidity(false, ";\t");
  CheckValidity(false, "; p1");
  CheckValidity(false, ";\tp1");
  CheckValidity(false, "; p1;");
  CheckValidity(false, ";\tp1;");
  CheckValidity(false, ";\"xx");
  CheckValidity(false, ";\"xx=y");
  CheckValidity(false, "; \"z\"=u");
  CheckValidity(false, "; z=\xff");

  CheckValidity(false, ";z=q/t:()<>@,:\\/[]?");
  CheckValidity(true, ";z=q/t:()<>@,:\\/[]?=", Mode::kRelaxed);
  CheckValidity(false, ";z=q r", Mode::kRelaxed);
  CheckValidity(false, ";z=q;r", Mode::kRelaxed);
  CheckValidity(false, ";z=q\"r", Mode::kRelaxed);
  CheckValidity(false, "; z=\xff", Mode::kRelaxed);
}

TEST(ParsedContentHeaderFieldParametersTest, ParameterName) {
  String input = "; y=z  ; y= u ;  t=r;k= \"t \\u\\\"x\" ;Q=U;T=S";

  CheckValidity(true, input);

  base::Optional<ParsedContentHeaderFieldParameters> t =
      ParsedContentHeaderFieldParameters::Parse(HeaderFieldTokenizer(input),
                                                Mode::kNormal);
  ASSERT_TRUE(t);

  EXPECT_EQ(6u, t->ParameterCount());
  EXPECT_TRUE(t->HasDuplicatedNames());
  EXPECT_EQ(String(), t->ParameterValueForName("a"));
  EXPECT_EQ(String(), t->ParameterValueForName("x"));
  EXPECT_EQ("u", t->ParameterValueForName("y"));
  EXPECT_EQ("S", t->ParameterValueForName("t"));
  EXPECT_EQ("t u\"x", t->ParameterValueForName("k"));
  EXPECT_EQ("U", t->ParameterValueForName("Q"));
  EXPECT_EQ("S", t->ParameterValueForName("T"));

  String kelvin = String::FromUTF8("\xe2\x84\xaa");
  DCHECK_EQ(CaseMap(AtomicString()).ToLower(kelvin), "k");
  EXPECT_EQ(String(), t->ParameterValueForName(kelvin));
}

TEST(ParsedContentHeaderFieldParametersTest, RelaxedParameterName) {
  String input = "; z=q/t:()<>@,:\\/[]?=;y=u";

  CheckValidity(true, input, Mode::kRelaxed);

  base::Optional<ParsedContentHeaderFieldParameters> t =
      ParsedContentHeaderFieldParameters::Parse(HeaderFieldTokenizer(input),
                                                Mode::kRelaxed);
  ASSERT_TRUE(t);
  EXPECT_EQ(2u, t->ParameterCount());
  EXPECT_FALSE(t->HasDuplicatedNames());
  EXPECT_EQ("q/t:()<>@,:\\/[]?=", t->ParameterValueForName("z"));
  EXPECT_EQ("u", t->ParameterValueForName("y"));
}

TEST(ParsedContentHeaderFieldParametersTest, BeginEnd) {
  String input = "; a=b; a=c; b=d";

  base::Optional<ParsedContentHeaderFieldParameters> t =
      ParsedContentHeaderFieldParameters::Parse(HeaderFieldTokenizer(input),
                                                Mode::kNormal);
  ASSERT_TRUE(t);
  EXPECT_TRUE(t->HasDuplicatedNames());
  EXPECT_EQ(3u, t->ParameterCount());

  auto* i = t->begin();
  ASSERT_NE(i, t->end());
  EXPECT_EQ(i->name, "a");
  EXPECT_EQ(i->value, "b");

  ++i;
  ASSERT_NE(i, t->end());
  EXPECT_EQ(i->name, "a");
  EXPECT_EQ(i->value, "c");

  ++i;
  ASSERT_NE(i, t->end());
  EXPECT_EQ(i->name, "b");
  EXPECT_EQ(i->value, "d");

  ++i;
  ASSERT_EQ(i, t->end());
}

TEST(ParsedContentHeaderFieldParametersTest, RBeginEnd) {
  String input = "; a=B; A=c; b=d";

  base::Optional<ParsedContentHeaderFieldParameters> t =
      ParsedContentHeaderFieldParameters::Parse(HeaderFieldTokenizer(input),
                                                Mode::kNormal);
  ASSERT_TRUE(t);
  EXPECT_TRUE(t->HasDuplicatedNames());
  EXPECT_EQ(3u, t->ParameterCount());

  auto i = t->rbegin();
  ASSERT_NE(i, t->rend());
  EXPECT_EQ(i->name, "b");
  EXPECT_EQ(i->value, "d");

  ++i;
  ASSERT_NE(i, t->rend());
  EXPECT_EQ(i->name, "A");
  EXPECT_EQ(i->value, "c");

  ++i;
  ASSERT_NE(i, t->rend());
  EXPECT_EQ(i->name, "a");
  EXPECT_EQ(i->value, "B");

  ++i;
  ASSERT_EQ(i, t->rend());
}

}  // namespace

}  // namespace blink
