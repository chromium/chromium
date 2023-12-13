// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(DocumentStateTest, ToStateVectorConnected) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());
  Element* html = doc.CreateRawElement(html_names::kHTMLTag);
  doc.appendChild(html);
  Node* body = html->appendChild(doc.CreateRawElement(html_names::kBodyTag));
  To<Element>(body)->setInnerHTML("<select form='ff'></select>");
  DocumentState* document_state = doc.GetFormController().ControlStates();
  Vector<String> state1 = document_state->ToStateVector();
  // <signature>, <control-size>, <form-key>, <name>, <type>, <data-size(0)>
  EXPECT_EQ(6u, state1.size());
  Node* select = body->firstChild();
  select->remove();
  // Success if the following ToStateVector() doesn't fail with a DCHECK.
  Vector<String> state2 = document_state->ToStateVector();
  EXPECT_EQ(0u, state2.size());
}

TEST(FormControllerTest, FormSignature) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  Document& doc = holder.GetDocument();
  doc.GetSettings()->SetScriptEnabled(true);
  auto* script = doc.CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"SCRIPT(
      class MyControl extends HTMLElement { static get formAssociated() { return true; }}
      customElements.define('my-control', MyControl);
      let container = document.body.appendChild(document.createElement('div'));
      container.innerHTML = `<form action="http://example.com/">
          <input type=checkbox name=1cb>
          <my-control name=2face></my-control>
          <select name="3s"></select>
          </form>`;
  )SCRIPT");
  doc.body()->appendChild(script);
  Element* form = doc.QuerySelector(AtomicString("form"), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(form);
  EXPECT_EQ(String("http://example.com/ [1cb 3s ]"),
            FormSignature(*To<HTMLFormElement>(form)))
      << "[] should contain names of the first and the third controls.";
}

}  // namespace blink
