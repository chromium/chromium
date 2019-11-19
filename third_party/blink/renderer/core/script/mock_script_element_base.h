// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MOCK_SCRIPT_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MOCK_SCRIPT_ELEMENT_BASE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/bindings/core/v8/html_script_element_or_svg_script_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"

namespace blink {

class MockScriptElementBase : public GarbageCollected<MockScriptElementBase>,
                              public ScriptElementBase {
  USING_GARBAGE_COLLECTED_MIXIN(MockScriptElementBase);

 public:
  static MockScriptElementBase* Create() {
    return MakeGarbageCollected<testing::StrictMock<MockScriptElementBase>>();
  }
  virtual ~MockScriptElementBase() {}

  MOCK_METHOD0(DispatchLoadEvent, void());
  MOCK_METHOD0(DispatchErrorEvent, void());
  MOCK_CONST_METHOD0(AsyncAttributeValue, bool());
  MOCK_CONST_METHOD0(CharsetAttributeValue, String());
  MOCK_CONST_METHOD0(CrossOriginAttributeValue, String());
  MOCK_CONST_METHOD0(DeferAttributeValue, bool());
  MOCK_CONST_METHOD0(EventAttributeValue, String());
  MOCK_CONST_METHOD0(ForAttributeValue, String());
  MOCK_CONST_METHOD0(IntegrityAttributeValue, String());
  MOCK_CONST_METHOD0(ReferrerPolicyAttributeValue, String());
  MOCK_CONST_METHOD0(ImportanceAttributeValue, String());
  MOCK_CONST_METHOD0(LanguageAttributeValue, String());
  MOCK_CONST_METHOD0(NomoduleAttributeValue, bool());
  MOCK_CONST_METHOD0(SourceAttributeValue, String());
  MOCK_CONST_METHOD0(TypeAttributeValue, String());

  MOCK_METHOD0(TextFromChildren, String());
  MOCK_CONST_METHOD0(HasSourceAttribute, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(HasChildren, bool());
  MOCK_CONST_METHOD0(GetNonceForElement, const AtomicString&());
  MOCK_CONST_METHOD0(ElementHasDuplicateAttributes, bool());
  MOCK_CONST_METHOD0(InitiatorName, AtomicString());
  MOCK_METHOD3(AllowInlineScriptForCSP,
               bool(const AtomicString&,
                    const WTF::OrdinalNumber&,
                    const String&));
  MOCK_CONST_METHOD0(GetDocument, Document&());
  MOCK_METHOD1(SetScriptElementForBinding,
               void(HTMLScriptElementOrSVGScriptElement&));
  MOCK_CONST_METHOD0(Loader, ScriptLoader*());

  void Trace(Visitor* visitor) override { ScriptElementBase::Trace(visitor); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MOCK_SCRIPT_ELEMENT_BASE_H_
