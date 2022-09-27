// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_language_detection_details.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/metrics/accept_language_and_content_language_usage.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/xml_names.h"

namespace blink {

namespace {

const AtomicString& DocumentLanguage(const Document& document) {
  Element* html_element = document.documentElement();
  if (!html_element)
    return g_null_atom;
  return html_element->FastGetAttribute(html_names::kLangAttr);
}

const AtomicString& DocumentXmlLanguage(const Document& document) {
  Element* html_element = document.documentElement();
  if (!html_element)
    return g_null_atom;
  return html_element->FastGetAttribute(xml_names::kLangAttr);
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

// Get language code ignoring locales. For `zh` family, as the
// languages with different locales have major difference, we return the value
// include its locales.
String GetLanguageCode(const String& language) {
  if (language.StartsWith("zh")) {
    return language;
  }

  Vector<String> language_codes;
  language.Split("-", language_codes);
  // Split function default is not allowed empty entry which cause potentical
  // crash when |langauge_codes| may be empty (for example, if |language| is
  // '-').
  return language_codes.empty() ? "" : language_codes[0];
}

void MatchTargetLanguageWithAcceptLanguages(
    const Document& document,
    const AtomicString& target_language,
    bool is_xml_lang,
    const std::string& language_histogram_name) {
  if (!document.domWindow() || !document.domWindow()->navigator()) {
    return;
  }

  // Get navigator()->languages value from Prefs.
  // Notes: navigator.language and Accept-Languages are almost always the same,
  // but sometimes might not be. For example: Accept-Languages had a country
  // specific language but not the base language. We consider them are the same
  // here.
  bool is_accept_language_dirty =
      document.domWindow()->navigator()->IsLanguagesDirty();
  const Vector<String>& accept_languages =
      document.domWindow()->navigator()->languages();

  // Match |target_language| and accept languages list:
  // 1. If the |target_language| matches the top-most accept languages
  // 2. If there are any overlap between |target_language| and accept languages
  if (GetLanguageCode(accept_languages.front()) ==
      GetLanguageCode(target_language)) {
    base::UmaHistogramEnumeration(
        language_histogram_name,
        is_xml_lang ? AcceptLanguageAndXmlHtmlLangUsage::
                          kXmlLangMatchesPrimaryAcceptLanguage
                    : AcceptLanguageAndXmlHtmlLangUsage::
                          kHtmlLangMatchesPrimaryAcceptLanguage);
  } else if (base::Contains(accept_languages, target_language,
                            &GetLanguageCode)) {
    base::UmaHistogramEnumeration(
        language_histogram_name,
        is_xml_lang ? AcceptLanguageAndXmlHtmlLangUsage::
                          kXmlLangMatchesAnyNonPrimayAcceptLanguage
                    : AcceptLanguageAndXmlHtmlLangUsage::
                          kHtmlLangMatchesAnyNonPrimayAcceptLanguage);
  }

  // navigator()->languages() is a potential update operation, it could set
  // |is_dirty_language| to false which causes future override operations
  // can't update the accep_language list. We should reset the language to
  // dirty if accept language is dirty before we read from Prefs.
  if (is_accept_language_dirty) {
    document.domWindow()->navigator()->SetLanguagesDirty();
  }
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

void WebLanguageDetectionDetails::RecordAcceptLanguageAndXmlHtmlLangMetric(
    const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();

  // We only record UMA metrics where URLs are in http family.
  if (!document->Url().ProtocolIsInHTTPFamily()) {
    return;
  }

  // Get document Content-Language value, which has been set as the top-most
  // content language value from http head.
  constexpr const char language_histogram_name[] =
      "LanguageUsage.AcceptLanguageAndXmlHtmlLangUsage";

  // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
  const AtomicString& xml_language = DocumentXmlLanguage(*document);
  if (xml_language) {
    if (xml_language.empty()) {
      base::UmaHistogramEnumeration(
          language_histogram_name,
          AcceptLanguageAndXmlHtmlLangUsage::kXmlLangEmpty);
      return;
    }

    if (xml_language == "*") {
      base::UmaHistogramEnumeration(
          language_histogram_name,
          AcceptLanguageAndXmlHtmlLangUsage::kXmlLangWildcard);
      return;
    }

    MatchTargetLanguageWithAcceptLanguages(*document, xml_language, true,
                                           language_histogram_name);
    return;
  }

  // We only record html language metric if xml:lang not exists.
  const AtomicString& html_language = DocumentLanguage(*document);
  if (!html_language || html_language.empty()) {
    base::UmaHistogramEnumeration(
        language_histogram_name,
        AcceptLanguageAndXmlHtmlLangUsage::kHtmlLangEmpty);
    return;
  }

  if (html_language == "*") {
    base::UmaHistogramEnumeration(
        language_histogram_name,
        AcceptLanguageAndXmlHtmlLangUsage::kHtmlLangWildcard);
    return;
  }

  MatchTargetLanguageWithAcceptLanguages(*document, html_language, false,
                                         language_histogram_name);
}

}  // namespace blink
