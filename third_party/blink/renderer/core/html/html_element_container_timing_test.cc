// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLElementContainerTimingAttributesTest : public PageTestBase {};

TEST_F(HTMLElementContainerTimingAttributesTest,
       DeprecatedDashedIgnoreWarnsInConsole) {
  ConsoleMessageStorage& storage = GetPage().GetConsoleMessageStorage();
  wtf_size_t initial_size = storage.size();

  SetBodyContent("<div id=a containertimingignore></div>");

  // The canonical spelling must not warn.
  EXPECT_EQ(storage.size(), initial_size);

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  element_a->setAttribute(html_names::kContainertimingIgnoreAttr, g_empty_atom);

  ASSERT_EQ(storage.size(), initial_size + 1);
  const ConsoleMessage* message = storage.at(storage.size() - 1);
  EXPECT_EQ(message->GetSource(),
            mojom::blink::ConsoleMessageSource::kDeprecation);
  EXPECT_EQ(message->GetLevel(), mojom::blink::ConsoleMessageLevel::kWarning);
  EXPECT_TRUE(message->Message().contains("containertiming-ignore"));
  EXPECT_TRUE(message->Message().contains("containertimingignore"));

  // Removing the attribute must not warn again.
  element_a->removeAttribute(html_names::kContainertimingIgnoreAttr);
  EXPECT_EQ(storage.size(), initial_size + 1);
}

}  // namespace blink
