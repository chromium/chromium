// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_element.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(StyleElementTest, CreateSheetUsesCache) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();

  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<style id=style>a { top: 0; }</style>");

  auto& style_element =
      To<HTMLStyleElement>(*document.getElementById(AtomicString("style")));
  EXPECT_FALSE(style_element.IsModule());
  StyleSheetContents* sheet = style_element.sheet()->Contents();

  Comment* comment = document.createComment("hello!");
  style_element.AppendChild(comment);
  EXPECT_EQ(style_element.sheet()->Contents(), sheet);

  style_element.RemoveChild(comment);
  EXPECT_EQ(style_element.sheet()->Contents(), sheet);
}

TEST(StyleElementTest, CSSModule) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();

  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<style id='style' type='module'>a { top: 0; }</style>");

  auto& style_element_module =
      To<HTMLStyleElement>(*document.getElementById(AtomicString("style")));

  EXPECT_TRUE(style_element_module.IsModule());

  // Modules do not have an associated CSSStyleSheet.
  EXPECT_EQ(style_element_module.sheet(), nullptr);

  // Once a CSS module is created, it cannot be changed to a non-module by
  // changing the "type" value.
  style_element_module.setAttribute(html_names::kTypeAttr,
                                    AtomicString("text/css"));
  EXPECT_TRUE(style_element_module.IsModule());
  EXPECT_EQ(style_element_module.sheet(), nullptr);

  // Likewise, a classic <style> tag cannot be converted to a module.
  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<style id='style'>a { top: 0; }</style>");
  auto& style_element_classic =
      To<HTMLStyleElement>(*document.getElementById(AtomicString("style")));
  EXPECT_FALSE(style_element_classic.IsModule());
  EXPECT_NE(style_element_classic.sheet(), nullptr);

  // Attempting to convert a classic <style> tag to a module won't be a module,
  // but the sheet will be empty because only an empty type or "text/css" is
  // allowed.
  style_element_classic.setAttribute(html_names::kTypeAttr,
                                     AtomicString("module"));
  EXPECT_FALSE(style_element_classic.IsModule());
  EXPECT_EQ(style_element_classic.sheet(), nullptr);

  // Switching back to a valid type will create a stylesheet.
  style_element_classic.setAttribute(html_names::kTypeAttr,
                                     AtomicString("text/css"));
  EXPECT_FALSE(style_element_classic.IsModule());
  EXPECT_NE(style_element_classic.sheet(), nullptr);

  // Test dynamically creating and inserting style elements.
  auto* style_element_dynamic_module =
      MakeGarbageCollected<HTMLStyleElement>(document);
  style_element_dynamic_module->SetInnerHTMLWithoutTrustedTypes(
      "a { top: 0; }");

  style_element_dynamic_module->setAttribute(html_names::kTypeAttr,
                                             AtomicString("module"));

  // Module-ness doesn't get computed until an element is connected.
  EXPECT_FALSE(style_element_dynamic_module->IsModule());
  EXPECT_EQ(style_element_dynamic_module->sheet(), nullptr);

  document.body()->AppendChild(style_element_dynamic_module);
  EXPECT_TRUE(style_element_dynamic_module->IsModule());
  EXPECT_EQ(style_element_dynamic_module->sheet(), nullptr);

  // Once connected, module-ness is fixed.
  style_element_dynamic_module->setAttribute(html_names::kTypeAttr,
                                             AtomicString("text/css"));
  EXPECT_TRUE(style_element_dynamic_module->IsModule());
  EXPECT_EQ(style_element_dynamic_module->sheet(), nullptr);

  // This behavior persists, even after being removed and re-inserted.
  document.body()->RemoveChild(style_element_dynamic_module);
  EXPECT_TRUE(style_element_dynamic_module->IsModule());
  EXPECT_EQ(style_element_dynamic_module->sheet(), nullptr);

  // Module type is not passed along to clones.
  // TODO(kschmi): For consistency with Import Maps, should we mimic passing
  // "Already Started" state when cloneNode is called? This would involve
  // passing `element_type_` to the clone.
  HTMLStyleElement* module_clone = static_cast<HTMLStyleElement*>(
      style_element_dynamic_module->cloneNode(/*deep=*/true));
  EXPECT_EQ(module_clone->sheet(), nullptr);
  EXPECT_FALSE(module_clone->IsModule());
  EXPECT_EQ(module_clone->getAttribute(html_names::kTypeAttr),
            AtomicString("text/css"));
  document.body()->AppendChild(module_clone);
  EXPECT_FALSE(module_clone->IsModule());
  EXPECT_NE(module_clone->sheet(), nullptr);

  // Test a dynamic classic style element.
  auto* style_element_dynamic_classic =
      MakeGarbageCollected<HTMLStyleElement>(document);
  style_element_dynamic_classic->SetInnerHTMLWithoutTrustedTypes(
      "a { top: 0; }");

  EXPECT_FALSE(style_element_dynamic_classic->IsModule());
  EXPECT_EQ(style_element_dynamic_classic->sheet(), nullptr);

  document.body()->AppendChild(style_element_dynamic_classic);
  EXPECT_FALSE(style_element_dynamic_classic->IsModule());
  EXPECT_NE(style_element_dynamic_classic->sheet(), nullptr);

  style_element_dynamic_classic->setAttribute(html_names::kTypeAttr,
                                              AtomicString("module"));
  EXPECT_FALSE(style_element_dynamic_classic->IsModule());

  // Setting the "type" to anything but "text/css" clears the sheet immediately.
  EXPECT_EQ(style_element_dynamic_classic->sheet(), nullptr);

  // Removing a classic module with type later set to "module" and re-inserting
  // it will not convert it to a module, as it is fixed at first insertion time.
  document.body()->RemoveChild(style_element_dynamic_classic);
  document.body()->AppendChild(style_element_dynamic_classic);
  EXPECT_FALSE(style_element_dynamic_classic->IsModule());
  EXPECT_EQ(style_element_dynamic_classic->sheet(), nullptr);

  // Module type is not passed along to clones.
  // TODO(kschmi): For consistency with Import Maps, should we mimic passing
  // "Already Started" state when cloneNode is called? This would involve
  // passing `element_type_` to the clone.
  HTMLStyleElement* classic_clone = static_cast<HTMLStyleElement*>(
      style_element_dynamic_classic->cloneNode(/*deep=*/true));
  EXPECT_EQ(classic_clone->getAttribute(html_names::kTypeAttr),
            AtomicString("module"));
  document.body()->AppendChild(classic_clone);
  EXPECT_TRUE(classic_clone->IsModule());
  EXPECT_EQ(classic_clone->sheet(), nullptr);

  // TODO(kschmi) - Updating the child contents shouldn't create a new Module
  // Map entry, but this can't be tested until Module Map functionality is
  // added.
}

TEST(StyleElementTest, CSSModuleImportMapDataURI) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kDeclarativeCSSModulesUseDataURI};

  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();

  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<style id='style' type='module' specifier='foo'>a { top: 0; }</style>");

  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(document.GetFrame()));
  const ImportMap* import_map = modulator->GetImportMapForTest();

  // Verify that the internal structure of the document's Import Map contains
  // an entry for the URL-encoded contents of the <style> tag.
  EXPECT_EQ(
      import_map->ToStringForTesting(),
      "{\"imports\":{\"foo\":\"data:text/"
      "css,a%20%7B%20top%3A%200%3B%20%7D\"},\"scopes\":{},\"integrity\": {}}");
}

}  // namespace blink
