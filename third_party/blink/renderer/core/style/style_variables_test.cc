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
  auto foo_data = css_test_helpers::CreateVariableData("foo");
  const CSSValue* foo_value = css_test_helpers::CreateCustomIdent("foo");

  StyleVariables vars1;
  vars1.SetData("--x", foo_data);
  vars1.SetValue("--x", foo_value);

  StyleVariables vars2(vars1);
  EXPECT_EQ(foo_data, vars2.GetData("--x").value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue("--x").value_or(nullptr));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, Assignment) {
  auto foo_data = css_test_helpers::CreateVariableData("foo");
  const CSSValue* foo_value = css_test_helpers::CreateCustomIdent("foo");

  StyleVariables vars1;
  vars1.SetData("--x", foo_data);
  vars1.SetValue("--x", foo_value);
  EXPECT_EQ(foo_data, vars1.GetData("--x").value_or(nullptr));
  EXPECT_EQ(foo_value, vars1.GetValue("--x").value_or(nullptr));

  StyleVariables vars2;
  EXPECT_FALSE(vars2.GetData("--x").has_value());
  EXPECT_FALSE(vars2.GetValue("--x").has_value());

  vars2.SetData("--y", foo_data);
  vars2.SetValue("--y", foo_value);
  EXPECT_EQ(foo_data, vars2.GetData("--y").value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue("--y").value_or(nullptr));

  vars2 = vars1;
  EXPECT_TRUE(vars2.GetData("--x").has_value());
  EXPECT_TRUE(vars2.GetValue("--x").has_value());
  EXPECT_FALSE(vars2.GetData("--y").has_value());
  EXPECT_FALSE(vars2.GetValue("--y").has_value());
  EXPECT_EQ(vars1, vars2);

  vars2.SetData("--z", foo_data);
  vars2.SetValue("--z", foo_value);
  EXPECT_EQ(foo_data, vars2.GetData("--z").value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue("--z").value_or(nullptr));

  // Should not affect vars1:
  EXPECT_FALSE(vars1.GetData("--y").has_value());
  EXPECT_FALSE(vars1.GetValue("--y").has_value());
  EXPECT_FALSE(vars1.GetData("--z").has_value());
  EXPECT_FALSE(vars1.GetValue("--z").has_value());
}

TEST_F(StyleVariablesTest, GetNames) {
  StyleVariables vars;
  vars.SetData("--x", css_test_helpers::CreateVariableData("foo"));
  vars.SetData("--y", css_test_helpers::CreateVariableData("bar"));

  HashSet<AtomicString> names;
  vars.CollectNames(names);
  EXPECT_EQ(2u, names.size());
  EXPECT_TRUE(names.Contains("--x"));
  EXPECT_TRUE(names.Contains("--y"));
}

// CSSVariableData

TEST_F(StyleVariablesTest, IsEmptyData) {
  StyleVariables vars;
  EXPECT_TRUE(vars.IsEmpty());
  vars.SetData("--x", css_test_helpers::CreateVariableData("foo"));
  EXPECT_FALSE(vars.IsEmpty());
}

TEST_F(StyleVariablesTest, SetData) {
  StyleVariables vars;

  auto foo = css_test_helpers::CreateVariableData("foo");
  auto bar = css_test_helpers::CreateVariableData("bar");

  EXPECT_FALSE(vars.GetData("--x").has_value());

  vars.SetData("--x", foo);
  EXPECT_EQ(foo, vars.GetData("--x").value_or(nullptr));

  vars.SetData("--x", bar);
  EXPECT_EQ(bar, vars.GetData("--x").value_or(nullptr));
}

TEST_F(StyleVariablesTest, SetNullData) {
  StyleVariables vars;
  EXPECT_FALSE(vars.GetData("--x").has_value());
  vars.SetData("--x", nullptr);
  auto data = vars.GetData("--x");
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(nullptr, data.value());
}

TEST_F(StyleVariablesTest, SingleDataSamePointer) {
  auto data = css_test_helpers::CreateVariableData("foo");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", data);
  vars2.SetData("--x", data);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleDataSameContent) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", css_test_helpers::CreateVariableData("foo"));
  vars2.SetData("--x", css_test_helpers::CreateVariableData("foo"));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleDataContentNotEqual) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", css_test_helpers::CreateVariableData("foo"));
  vars2.SetData("--x", css_test_helpers::CreateVariableData("bar"));
  EXPECT_NE(vars1, vars2);
}

TEST_F(StyleVariablesTest, DifferentDataSize) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", css_test_helpers::CreateVariableData("foo"));
  vars2.SetData("--x", css_test_helpers::CreateVariableData("bar"));
  vars2.SetData("--y", css_test_helpers::CreateVariableData("foz"));
  EXPECT_NE(vars1, vars2);
}

// CSSValue

TEST_F(StyleVariablesTest, IsEmptyValue) {
  StyleVariables vars;
  EXPECT_TRUE(vars.IsEmpty());
  vars.SetValue("--x", css_test_helpers::CreateCustomIdent("foo"));
  EXPECT_FALSE(vars.IsEmpty());
}

TEST_F(StyleVariablesTest, SetValue) {
  StyleVariables vars;

  const CSSValue* foo = css_test_helpers::CreateCustomIdent("foo");
  const CSSValue* bar = css_test_helpers::CreateCustomIdent("bar");

  EXPECT_FALSE(vars.GetValue("--x").has_value());

  vars.SetValue("--x", foo);
  EXPECT_EQ(foo, vars.GetValue("--x").value_or(nullptr));

  vars.SetValue("--x", bar);
  EXPECT_EQ(bar, vars.GetValue("--x").value_or(nullptr));
}

TEST_F(StyleVariablesTest, SetNullValue) {
  StyleVariables vars;
  EXPECT_FALSE(vars.GetValue("--x").has_value());
  vars.SetValue("--x", nullptr);
  auto value = vars.GetValue("--x");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(nullptr, value.value());
}

TEST_F(StyleVariablesTest, SingleValueSamePointer) {
  const CSSValue* foo = css_test_helpers::CreateCustomIdent("foo");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", foo);
  vars2.SetValue("--x", foo);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleValueSameContent) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", css_test_helpers::CreateCustomIdent("foo"));
  vars2.SetValue("--x", css_test_helpers::CreateCustomIdent("foo"));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleValueContentNotEqual) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", css_test_helpers::CreateCustomIdent("foo"));
  vars2.SetValue("--x", css_test_helpers::CreateCustomIdent("bar"));
  EXPECT_NE(vars1, vars2);
}

TEST_F(StyleVariablesTest, DifferentValueSize) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", css_test_helpers::CreateCustomIdent("foo"));
  vars2.SetValue("--x", css_test_helpers::CreateCustomIdent("bar"));
  vars2.SetValue("--y", css_test_helpers::CreateCustomIdent("foz"));
  EXPECT_NE(vars1, vars2);
}

}  // namespace
}  // namespace blink
