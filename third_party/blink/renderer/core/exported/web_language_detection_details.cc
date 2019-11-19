// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_language_detection_details.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"

namespace blink {

namespace {

const AtomicString& DocumentLanguage(const Document& document) {
  Element* html_element = document.documentElement();
  if (!html_element)
    return g_null_atom;
  return html_element->FastGetAttribute(html_names::kLangAttr);
}

bool HasNoTranslate(const Document& document) {
  DEFINE_STATIC_LOCAL(const AtomicString, google, ("google"));

  HTMLHeadElement* head_element = document.head();
  if (!head_element)
    return false;

  for (const HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::ChildrenOf(*head_element)) {
    if (meta_element.GetName() != google)
      continue;

    // Check if the tag contains content="notranslate" or value="notranslate"
    AtomicString content = meta_element.Content();
    if (content.IsNull())
      content = meta_element.FastGetAttribute(html_names::kValueAttr);
    if (EqualIgnoringASCIICase(content, "notranslate"))
      return true;
  }

  return false;
}

}  // namespace

WebLanguageDetectionDetails
WebLanguageDetectionDetails::CollectLanguageDetectionDetails(
    const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();

  WebLanguageDetectionDetails details;
  details.content_language = document->ContentLanguage();
  details.html_language = DocumentLanguage(*document);
  details.url = document->Url();
  details.has_no_translate_meta = HasNoTranslate(*document);

  return details;
}

}  // namespace blink
