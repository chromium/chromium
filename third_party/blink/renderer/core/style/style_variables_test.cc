// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_variables.h"

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace {

class StyleVariablesTest : public PageTestBase {};

TEST_F(StyleVariablesTest, EmptyEqual) {
  StyleVariables vars1;
  StyleVariables vars2;
  EXPECT_EQ(vars1, vars1);
  EXPECT_EQ(vars2, vars2);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, Copy) {
  auto* foo_data = css_test_helpers::CreateVariableData("foo");
  const CSSValue* foo_value = css_test_helpers::CreateCustomIdent("foo");
  AtomicString x_string("--x");

  StyleVariables vars1;
  vars1.SetData(x_string, foo_data);
  vars1.SetValue(x_string, foo_value);

  StyleVariables vars2(vars1);
  EXPECT_EQ(foo_data, vars2.GetData(x_string).value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue(x_string).value_or(nullptr));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, Assignment) {
  auto* foo_data = css_test_helpers::CreateVariableData("foo");
  const CSSValue* foo_value = css_test_helpers::CreateCustomIdent("foo");
  AtomicString x_string("--x");
  AtomicString y_string("--y");
  AtomicString z_string("--z");

  StyleVariables vars1;
  vars1.SetData(x_string, foo_data);
  vars1.SetValue(x_string, foo_value);
  EXPECT_EQ(foo_data, vars1.GetData(x_string).value_or(nullptr));
  EXPECT_EQ(foo_value, vars1.GetValue(x_string).value_or(nullptr));

  StyleVariables vars2;
  EXPECT_FALSE(vars2.GetData(x_string).has_value());
  EXPECT_FALSE(vars2.GetValue(x_string).has_value());

  vars2.SetData(y_string, foo_data);
  vars2.SetValue(y_string, foo_value);
  EXPECT_EQ(foo_data, vars2.GetData(y_string).value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue(y_string).value_or(nullptr));

  vars2 = vars1;
  EXPECT_TRUE(vars2.GetData(x_string).has_value());
  EXPECT_TRUE(vars2.GetValue(x_string).has_value());
  EXPECT_FALSE(vars2.GetData(y_string).has_value());
  EXPECT_FALSE(vars2.GetValue(y_string).has_value());
  EXPECT_EQ(vars1, vars2);

  vars2.SetData(z_string, foo_data);
  vars2.SetValue(z_string, foo_value);
  EXPECT_EQ(foo_data, vars2.GetData(z_string).value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue(z_string).value_or(nullptr));

  // Should not affect vars1:
  EXPECT_FALSE(vars1.GetData(y_string).has_value());
  EXPECT_FALSE(vars1.GetValue(y_string).has_value());
  EXPECT_FALSE(vars1.GetData(z_string).has_value());
  EXPECT_FALSE(vars1.GetValue(z_string).has_value());
}

TEST_F(StyleVariablesTest, GetNames) {
  AtomicString x_string("--x");
  AtomicString y_string("--y");
  StyleVariables vars;
  vars.SetData(x_string, css_test_helpers::CreateVariableData("foo"));
  vars.SetData(y_string, css_test_helpers::CreateVariableData("bar"));

  HashSet<AtomicString> names;
  vars.CollectNames(names);
  EXPECT_EQ(2u, names.size());
  EXPECT_TRUE(names.Contains(x_string));
  EXPECT_TRUE(names.Contains(y_string));
}

// CSSVariableData

TEST_F(StyleVariablesTest, IsEmptyData) {
  AtomicString x_string("--x");
  StyleVariables vars;
  EXPECT_TRUE(vars.IsEmpty());
  vars.SetData(x_string, css_test_helpers::CreateVariableData("foo"));
  EXPECT_FALSE(vars.IsEmpty());
}

TEST_F(StyleVariablesTest, SetData) {
  AtomicString x_string("--x");
  StyleVariables vars;

  auto* foo = css_test_helpers::CreateVariableData("foo");
  auto* bar = css_test_helpers::CreateVariableData("bar");

  EXPECT_FALSE(vars.GetData(x_string).has_value());

  vars.SetData(x_string, foo);
  EXPECT_EQ(foo, vars.GetData(x_string).value_or(nullptr));

  vars.SetData(x_string, bar);
  EXPECT_EQ(bar, vars.GetData(x_string).value_or(nullptr));
}

TEST_F(StyleVariablesTest, SetNullData) {
  AtomicString x_string("--x");
  StyleVariables vars;
  EXPECT_FALSE(vars.GetData(x_string).has_value());
  vars.SetData(x_string, nullptr);
  auto data = vars.GetData(x_string);
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(nullptr, data.value());
}

TEST_F(StyleVariablesTest, SingleDataSamePointer) {
  AtomicString x_string("--x");
  auto* data = css_test_helpers::CreateVariableData("foo");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData(x_string, data);
  vars2.SetData(x_string, data);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleDataSameContent) {
  AtomicString x_string("--x");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData(x_string, css_test_helpers::CreateVariableData("foo"));
  vars2.SetData(x_string, css_test_helpers::CreateVariableData("foo"));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleDataContentNotEqual) {
  AtomicString x_string("--x");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData(x_string, css_test_helpers::CreateVariableData("foo"));
  vars2.SetData(x_string, css_test_helpers::CreateVariableData("bar"));
  EXPECT_NE(vars1, vars2);
}

TEST_F(StyleVariablesTest, DifferentDataSize) {
  AtomicString x_string("--x");
  AtomicString y_string("--y");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData(x_string, css_test_helpers::CreateVariableData("foo"));
  vars2.SetData(x_string, css_test_helpers::CreateVariableData("bar"));
  vars2.SetData(y_string, css_test_helpers::CreateVariableData("foz"));
  EXPECT_NE(vars1, vars2);
}

// CSSValue

TEST_F(StyleVariablesTest, IsEmptyValue) {
  AtomicString x_string("--x");
  StyleVariables vars;
  EXPECT_TRUE(vars.IsEmpty());
  vars.SetValue(x_string, css_test_helpers::CreateCustomIdent("foo"));
  EXPECT_FALSE(vars.IsEmpty());
}

TEST_F(StyleVariablesTest, SetValue) {
  AtomicString x_string("--x");
  StyleVariables vars;

  const CSSValue* foo = css_test_helpers::CreateCustomIdent("foo");
  const CSSValue* bar = css_test_helpers::CreateCustomIdent("bar");

  EXPECT_FALSE(vars.GetValue(x_string).has_value());

  vars.SetValue(x_string, foo);
  EXPECT_EQ(foo, vars.GetValue(x_string).value_or(nullptr));

  vars.SetValue(x_string, bar);
  EXPECT_EQ(bar, vars.GetValue(x_string).value_or(nullptr));
}

TEST_F(StyleVariablesTest, SetNullValue) {
  AtomicString x_string("--x");
  StyleVariables vars;
  EXPECT_FALSE(vars.GetValue(x_string).has_value());
  vars.SetValue(x_string, nullptr);
  auto value = vars.GetValue(x_string);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(nullptr, value.value());
}

TEST_F(StyleVariablesTest, SingleValueSamePointer) {
  AtomicString x_string("--x");
  const CSSValue* foo = css_test_helpers::CreateCustomIdent("foo");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue(x_string, foo);
  vars2.SetValue(x_string, foo);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleValueSameContent) {
  AtomicString x_string("--x");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue(x_string, css_test_helpers::CreateCustomIdent("foo"));
  vars2.SetValue(x_string, css_test_helpers::CreateCustomIdent("foo"));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleValueContentNotEqual) {
  AtomicString x_string("--x");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue(x_string, css_test_helpers::CreateCustomIdent("foo"));
  vars2.SetValue(x_string, css_test_helpers::CreateCustomIdent("bar"));
  EXPECT_NE(vars1, vars2);
}

TEST_F(StyleVariablesTest, DifferentValueSize) {
  AtomicString x_string("--x");
  AtomicString y_string("--y");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue(x_string, css_test_helpers::CreateCustomIdent("foo"));
  vars2.SetValue(x_string, css_test_helpers::CreateCustomIdent("bar"));
  vars2.SetValue(y_string, css_test_helpers::CreateCustomIdent("foz"));
  EXPECT_NE(vars1, vars2);
}

}  // namespace
}  // namespace blink
