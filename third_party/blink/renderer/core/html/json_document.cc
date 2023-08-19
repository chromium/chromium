// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/json_document.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_pre_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

namespace {

String GetStyleSheetContents() {
  return R"CSS(
    body {
      margin: 0
    }
    pre {
      word-wrap: break-word;
      white-space: pre-wrap;
    }
    .line-wrap-control {
      width: 100%;
      font-size: initial;
      font-family: monospace;
      user-select: none;
      background-color: rgb(240,240,240);
      border-bottom: 1px solid rgb(187, 187, 187);
      display: flex;
      justify-content: flex-start;
      align-items: center;
    }
    @media (prefers-color-scheme: dark) {
    .line-wrap-control {
      background-color: rgb(60, 60, 60);
    }
  )CSS";
}

}  // namespace

class PrettyPrintJSONListener : public NativeEventListener {
 public:
  PrettyPrintJSONListener(HTMLPreElement* pre, HTMLInputElement* checkbox)
      : checkbox_(checkbox), pre_(pre) {}

  void Invoke(ExecutionContext*, Event* event) override {
    DCHECK_EQ(event->type(), event_type_names::kChange);
    JSONParseError opt_error{};
    if (!parsed_json_value_) {
      parsed_json_value_ = ParseJSON(pre_->textContent(), &opt_error);
    }
    if (opt_error.type != JSONParseErrorType::kNoError) {
      return;
    }
    if (checkbox_->Checked()) {
      pre_->setTextContent(parsed_json_value_->ToPrettyJSONString());
    } else {
      pre_->setTextContent(parsed_json_value_->ToJSONString());
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(checkbox_);
    visitor->Trace(pre_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<HTMLInputElement> checkbox_;
  Member<HTMLPreElement> pre_;
  std::unique_ptr<JSONValue> parsed_json_value_;
};

class JSONDocumentParser : public HTMLDocumentParser {
 public:
  explicit JSONDocumentParser(JSONDocument& document,
                              ParserSynchronizationPolicy sync_policy)
      : HTMLDocumentParser(document, sync_policy, kDisallowPrefetching) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(pre_);
    HTMLDocumentParser::Trace(visitor);
  }

 private:
  void AppendBytes(const char* data, size_t length) override {
    CHECK(RuntimeEnabledFeatures::PrettyPrintJSONDocumentEnabled());
    if (!document_initialized_) {
      CreateDocumentStructure();
    }
    pre_->insertAdjacentText("beforeEnd", WTF::String::FromUTF8(data, length),
                             ASSERT_NO_EXCEPTION);
  }

  void CreateDocumentStructure() {
    CHECK(RuntimeEnabledFeatures::PrettyPrintJSONDocumentEnabled());
    auto* html = MakeGarbageCollected<HTMLHtmlElement>(*GetDocument());
    GetDocument()->ParserAppendChild(html);
    auto* head = MakeGarbageCollected<HTMLHeadElement>(*GetDocument());
    auto* meta = MakeGarbageCollected<HTMLMetaElement>(*GetDocument(),
                                                       CreateElementFlags());
    meta->setAttribute(html_names::kNameAttr, AtomicString("color-scheme"));
    meta->setAttribute(html_names::kContentAttr, AtomicString("light dark"));
    auto* meta_charset = MakeGarbageCollected<HTMLMetaElement>(
        *GetDocument(), CreateElementFlags());
    meta_charset->setAttribute(html_names::kCharsetAttr, AtomicString("utf-8"));
    head->ParserAppendChild(meta);
    head->ParserAppendChild(meta_charset);
    html->ParserAppendChild(head);
    auto* body = MakeGarbageCollected<HTMLBodyElement>(*GetDocument());
    html->ParserAppendChild(body);
    body->setAttribute(html_names::kStyleAttr, AtomicString("margin: 0"));
    pre_ = MakeGarbageCollected<HTMLPreElement>(html_names::kPreTag,
                                                *GetDocument());
    auto* label = MakeGarbageCollected<HTMLLabelElement>(*GetDocument());
    label->ParserAppendChild(Text::Create(
        *GetDocument(), WTF::AtomicString(Locale::DefaultLocale().QueryString(
                            IDS_PRETTY_PRINT_JSON))));
    label->setAttribute(html_names::kClassAttr,
                        AtomicString("line-wrap-control"));
    auto* checkbox = MakeGarbageCollected<HTMLInputElement>(*GetDocument());
    checkbox->setAttribute(html_names::kTypeAttr, input_type_names::kCheckbox);
    checkbox->addEventListener(
        event_type_names::kChange,
        MakeGarbageCollected<PrettyPrintJSONListener>(pre_, checkbox),
        /*use_capture=*/false);
    checkbox->setAttribute(
        html_names::kAriaLabelAttr,
        WTF::AtomicString(
            Locale::DefaultLocale().QueryString(IDS_PRETTY_PRINT_JSON)));
    label->ParserAppendChild(checkbox);
    // Add the checkbox to a form with autocomplete=off, to avoid form
    // restoration from changing the value of the checkbox.
    auto* form = MakeGarbageCollected<HTMLFormElement>(*GetDocument());
    form->setAttribute(html_names::kAutocompleteAttr, AtomicString("off"));
    form->ParserAppendChild(label);
    HTMLStyleElement* style =
        MakeGarbageCollected<HTMLStyleElement>(*GetDocument());
    style->setTextContent(GetStyleSheetContents());
    auto* div = MakeGarbageCollected<HTMLDivElement>(*GetDocument());
    ShadowRoot& shadow_root = div->EnsureUserAgentShadowRoot();
    shadow_root.ParserAppendChild(style);
    shadow_root.ParserAppendChild(form);
    body->ParserAppendChild(div);
    body->ParserAppendChild(pre_);
    document_initialized_ = true;
  }

  Member<HTMLPreElement> pre_;
  bool document_initialized_{false};
};

JSONDocument::JSONDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, {DocumentClass::kText}) {
  SetCompatibilityMode(kNoQuirksMode);
  LockCompatibilityMode();
}

DocumentParser* JSONDocument::CreateParser() {
  return MakeGarbageCollected<JSONDocumentParser>(
      *this, GetParserSynchronizationPolicy());
}
}  // namespace blink
