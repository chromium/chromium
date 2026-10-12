// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class HTMLDataListElementTest : public SimTest {};

TEST_F(HTMLDataListElementTest, FinishedParsingChildren) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<datalist id=list></datalist>");

  auto* data_list = GetDocument().getElementById(AtomicString("list"));
  ASSERT_TRUE(data_list);
  EXPECT_TRUE(data_list->IsFinishedParsingChildren());
}

TEST_F(HTMLDataListElementTest,
       DataListIndicatorNotFocusableAndReplaceChildrenDoesNotCrash) {
  ScopedDialogNewFocusBehaviorForTest dialog_focus(false);
  ScopedOmitBlurEventOnElementRemovalForTest omit_blur(false);

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      input { visibility: hidden }
      input::-webkit-calendar-picker-indicator {
        visibility: visible;
        overflow: scroll;
        width: 20px;
        height: 6px;
        font-size: 40px;
        line-height: 60px;
      }
    </style>
    <datalist id=options><option value=a></option></datalist>
    <datalist id=other><option value=b></option></datalist>
    <dialog><input id=target list=options></dialog>
    <script>
      const dialog = document.querySelector('dialog');
      const input = document.querySelector('input');
      const list = document.querySelector('datalist');

      list.removeAttribute('id');
      for (let i = 0; i < 7; i++) list.setAttribute('a' + i, '');
      list.setAttribute('id', 'options');

      const word = BigInt('0x0000123456789ab0');
      let tail = '';
      for (let i = 0n; i < 8n; i++) {
        tail += String.fromCharCode(Number((word >> (8n * i)) & 255n));
      }
      const strings = [], nodes = [];
      for (let i = 0; i < 256; i++) {
        strings.push((String(i).padStart(8, '0') + 'x'.repeat(100) + tail).split('').join(''));
      }

      dialog.show();
      input.getBoundingClientRect();
      dialog.close();
      dialog.show();

      input.addEventListener('blur', () => {
        input.setAttribute('list', 'other');
        list.setAttribute('b', '');
        for (const s of strings) nodes.push(document.createTextNode(s));
      }, {capture: true, once: true});
    </script>
  )HTML");

  auto* input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(input);
  Element* picker = input->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdPickerIndicator);
  ASSERT_TRUE(picker);
  EXPECT_FALSE(picker->IsFocusable());

  auto* list = GetDocument().getElementById(AtomicString("options"));
  ASSERT_TRUE(list);
  list->RemoveChildren();
}

TEST(HTMLDataListElementTest2, DecrementedAfterGc) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  Persistent<Document> document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body><datalist id=x></datalist></body>");
  EXPECT_TRUE(document->HasAtLeastOneDataList());
  auto* data_list = document->getElementById(AtomicString("x"));
  ASSERT_TRUE(data_list);
  data_list->parentElement()->RemoveChild(data_list);
  data_list = nullptr;
  blink::ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(document->HasAtLeastOneDataList());
}

}  // namespace blink
