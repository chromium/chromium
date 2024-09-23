/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_tests.h"

#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_static_string_list.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/language.h"

namespace blink {

SVGTests::SVGTests(SVGElement* context_element)
    : required_extensions_(
          SVGStaticStringList::Create<' '>(context_element,
                                           svg_names::kRequiredExtensionsAttr)),
      system_language_(
          SVGStaticStringList::Create<','>(context_element,
                                           svg_names::kSystemLanguageAttr)) {
  DCHECK(context_element);
}

void SVGTests::Trace(Visitor* visitor) const {
  visitor->Trace(required_extensions_);
  visitor->Trace(system_language_);
}

SVGStringListTearOff* SVGTests::requiredExtensions() {
  return required_extensions_->TearOff();
}

SVGStringListTearOff* SVGTests::systemLanguage() {
  return system_language_->TearOff();
}

SVGAnimatedPropertyBase* SVGTests::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kRequiredExtensionsAttr) {
    return required_extensions_.Get();
  } else if (attribute_name == svg_names::kSystemLanguageAttr) {
    return system_language_.Get();
  } else {
    return nullptr;
  }
}

void SVGTests::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{required_extensions_.Get(),
                                   system_language_.Get()};
  SVGElement::SynchronizeListOfSVGAttributes(attrs);
}

static bool IsLangTagPrefix(const String& lang_tag, const String& language) {
  if (!lang_tag.StartsWithIgnoringASCIICase(language))
    return false;
  return lang_tag.length() == language.length() ||
         lang_tag[language.length()] == '-';
}

static bool MatchLanguageList(const String& lang_tag,
                              const Vector<String>& languages) {
  for (const auto& value : languages) {
    if (IsLangTagPrefix(lang_tag, value))
      return true;
  }
  return false;
}

bool SVGTests::IsValid() const {
  if (system_language_->IsSpecified()) {
    bool match_found = false;
    Vector<String> languages;
    system_language_->ContextElement()
        ->GetDocument()
        .GetPage()
        ->GetChromeClient()
        .AcceptLanguages()
        .Split(',', languages);
    for (const auto& lang_tag : system_language_->Value()->Values()) {
      if (MatchLanguageList(lang_tag, languages)) {
        match_found = true;
        break;
      }
    }
    if (!match_found)
      return false;
  }

  if (required_extensions_->IsSpecified()) {
    const Vector<String>& extensions = required_extensions_->Value()->Values();
    // 'If a null string or empty string value is given to attribute
    // 'requiredExtensions', the attribute evaluates to "false".'
    if (extensions.empty())
      return false;
    for (const auto& extension : extensions) {
      if (extension != html_names::xhtmlNamespaceURI &&
          extension != mathml_names::kNamespaceURI) {
        return false;
      }
    }
  }
  return true;
}

bool SVGTests::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name == svg_names::kRequiredExtensionsAttr ||
         attr_name == svg_names::kSystemLanguageAttr;
}

}  // namespace blink
