// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_script_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class HTMLScriptElementTest : public testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>();
  }

  Document& document() { return dummy_page_holder_->GetDocument(); }

  HTMLScriptElement* MakeScript() {
    HTMLScriptElement* script = To<HTMLScriptElement>(
        document().body()->AppendChild(MakeGarbageCollected<HTMLScriptElement>(
            document(), CreateElementFlags::ByParser(&document()))));
    EXPECT_TRUE(script);
    return script;
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(HTMLScriptElementTest, ScriptTextInternalSlotSimple) {
  HTMLScriptElement* script = MakeScript();
  ScriptElementBase* script_base = script;
  EXPECT_EQ(script_base->ScriptTextInternalSlot(), "");

  script->ParserAppendChild(Text::Create(document(), "abc"));
  EXPECT_EQ(script_base->ScriptTextInternalSlot(), "");

  script->FinishParsingChildren();
  EXPECT_EQ(script_base->ScriptTextInternalSlot(), "abc");
}

TEST_F(HTMLScriptElementTest, ScriptTextInternalSlotMultiple) {
  HTMLScriptElement* script = MakeScript();
  script->ParserAppendChild(Text::Create(document(), "abc"));
  script->ParserAppendChild(Text::Create(document(), "def"));
  script->ParserAppendChild(Text::Create(document(), "ghi"));
  script->FinishParsingChildren();

  ScriptElementBase* script_base = script;
  EXPECT_EQ(script_base->ScriptTextInternalSlot(), "abcdefghi");
}

TEST_F(HTMLScriptElementTest,
       ScriptTextInternalSlotScriptParsingInterruptedByApiCall) {
  HTMLScriptElement* script = MakeScript();
  script->ParserAppendChild(Text::Create(document(), "abc"));
  script->AppendChild(Text::Create(document(), "def"));
  script->ParserAppendChild(Text::Create(document(), "ghi"));
  script->FinishParsingChildren();

  ScriptElementBase* script_base = script;
  EXPECT_EQ(script_base->ScriptTextInternalSlot(), "");
}

}  // namespace blink
