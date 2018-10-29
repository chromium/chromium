/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/serializers/markup_formatter.h"

#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

using namespace HTMLNames;

struct EntityDescription {
  UChar entity;
  const CString& reference;
  EntityMask mask;
};

template <typename CharType>
static inline void AppendCharactersReplacingEntitiesInternal(
    StringBuilder& result,
    CharType* text,
    unsigned length,
    const EntityDescription entity_maps[],
    unsigned entity_maps_count,
    EntityMask entity_mask) {
  unsigned position_after_last_entity = 0;
  for (unsigned i = 0; i < length; ++i) {
    for (unsigned entity_index = 0; entity_index < entity_maps_count;
         ++entity_index) {
      if (text[i] == entity_maps[entity_index].entity &&
          entity_maps[entity_index].mask & entity_mask) {
        result.Append(text + position_after_last_entity,
                      i - position_after_last_entity);
        const CString& replacement = entity_maps[entity_index].reference;
        result.Append(replacement.data(), replacement.length());
        position_after_last_entity = i + 1;
        break;
      }
    }
  }
  result.Append(text + position_after_last_entity,
                length - position_after_last_entity);
}

void MarkupFormatter::AppendCharactersReplacingEntities(
    StringBuilder& result,
    const String& source,
    unsigned offset,
    unsigned length,
    EntityMask entity_mask) {
  DEFINE_STATIC_LOCAL(const CString, amp_reference, ("&amp;"));
  DEFINE_STATIC_LOCAL(const CString, lt_reference, ("&lt;"));
  DEFINE_STATIC_LOCAL(const CString, gt_reference, ("&gt;"));
  DEFINE_STATIC_LOCAL(const CString, quot_reference, ("&quot;"));
  DEFINE_STATIC_LOCAL(const CString, nbsp_reference, ("&nbsp;"));
  DEFINE_STATIC_LOCAL(const CString, tab_reference, ("&#9;"));
  DEFINE_STATIC_LOCAL(const CString, line_feed_reference, ("&#10;"));
  DEFINE_STATIC_LOCAL(const CString, carriage_return_reference, ("&#13;"));

  static const EntityDescription kEntityMaps[] = {
      {'&', amp_reference, kEntityAmp},
      {'<', lt_reference, kEntityLt},
      {'>', gt_reference, kEntityGt},
      {'"', quot_reference, kEntityQuot},
      {kNoBreakSpaceCharacter, nbsp_reference, kEntityNbsp},
      {'\t', tab_reference, kEntityTab},
      {'\n', line_feed_reference, kEntityLineFeed},
      {'\r', carriage_return_reference, kEntityCarriageReturn},
  };

  if (!(offset + length))
    return;

  DCHECK_LE(offset + length, source.length());
  if (source.Is8Bit()) {
    AppendCharactersReplacingEntitiesInternal(
        result, source.Characters8() + offset, length, kEntityMaps,
        arraysize(kEntityMaps), entity_mask);
  } else {
    AppendCharactersReplacingEntitiesInternal(
        result, source.Characters16() + offset, length, kEntityMaps,
        arraysize(kEntityMaps), entity_mask);
  }
}

MarkupFormatter::MarkupFormatter(EAbsoluteURLs resolve_urls_method,
                                 SerializationType serialization_type)
    : resolve_urls_method_(resolve_urls_method),
      serialization_type_(serialization_type) {}

MarkupFormatter::~MarkupFormatter() = default;

String MarkupFormatter::ResolveURLIfNeeded(const Element& element,
                                           const String& url_string) const {
  switch (resolve_urls_method_) {
    case kResolveAllURLs:
      return element.GetDocument().CompleteURL(url_string).GetString();

    case kResolveNonLocalURLs:
      if (!element.GetDocument().Url().IsLocalFile())
        return element.GetDocument().CompleteURL(url_string).GetString();
      break;

    case kDoNotResolveURLs:
      break;
  }
  return url_string;
}

void MarkupFormatter::AppendStartMarkup(StringBuilder& result,
                                        const Node& node,
                                        Namespaces* namespaces) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      NOTREACHED();
      break;
    case Node::kCommentNode:
      AppendComment(result, ToComment(node).data());
      break;
    case Node::kDocumentNode:
      AppendXMLDeclaration(result, To<Document>(node));
      break;
    case Node::kDocumentFragmentNode:
      break;
    case Node::kDocumentTypeNode:
      AppendDocumentType(result, ToDocumentType(node));
      break;
    case Node::kProcessingInstructionNode:
      AppendProcessingInstruction(result,
                                  ToProcessingInstruction(node).target(),
                                  ToProcessingInstruction(node).data());
      break;
    case Node::kElementNode:
      NOTREACHED();
      break;
    case Node::kCdataSectionNode:
      AppendCDATASection(result, ToCDATASection(node).data());
      break;
    case Node::kAttributeNode:
      NOTREACHED();
      break;
  }
}

void MarkupFormatter::AppendEndMarkup(StringBuilder& result,
                                      const Element& element) {
  if (ShouldSelfClose(element) ||
      (!element.HasChildren() && ElementCannotHaveEndTag(element)))
    return;

  result.Append("</");
  result.Append(element.TagQName().ToString());
  result.Append('>');
}

void MarkupFormatter::AppendAttributeValue(StringBuilder& result,
                                           const String& attribute,
                                           bool document_is_html) {
  AppendCharactersReplacingEntities(result, attribute, 0, attribute.length(),
                                    document_is_html
                                        ? kEntityMaskInHTMLAttributeValue
                                        : kEntityMaskInAttributeValue);
}

void MarkupFormatter::AppendQuotedURLAttributeValue(
    StringBuilder& result,
    const Element& element,
    const Attribute& attribute) {
  DCHECK(element.IsURLAttribute(attribute)) << element;
  String resolved_url_string = ResolveURLIfNeeded(element, attribute.Value());
  UChar quote_char = '"';
  if (ProtocolIsJavaScript(resolved_url_string)) {
    // minimal escaping for javascript urls
    if (resolved_url_string.Contains('&'))
      resolved_url_string.Replace('&', "&amp;");

    if (resolved_url_string.Contains('"')) {
      if (resolved_url_string.Contains('\''))
        resolved_url_string.Replace('"', "&quot;");
      else
        quote_char = '\'';
    }
    result.Append(quote_char);
    result.Append(resolved_url_string);
    result.Append(quote_char);
    return;
  }

  // FIXME: This does not fully match other browsers. Firefox percent-escapes
  // non-ASCII characters for innerHTML.
  result.Append(quote_char);
  AppendAttributeValue(result, resolved_url_string, false);
  result.Append(quote_char);
}

void MarkupFormatter::AppendNamespace(StringBuilder& result,
                                      const AtomicString& prefix,
                                      const AtomicString& namespace_uri,
                                      Namespaces& namespaces) {
  const AtomicString& lookup_key = (!prefix) ? g_empty_atom : prefix;
  AtomicString found_uri = namespaces.at(lookup_key);
  if (!EqualIgnoringNullity(found_uri, namespace_uri)) {
    namespaces.Set(lookup_key, namespace_uri);
    result.Append(' ');
    result.Append(g_xmlns_atom.GetString());
    if (!prefix.IsEmpty()) {
      result.Append(':');
      result.Append(prefix);
    }

    result.Append("=\"");
    AppendAttributeValue(result, namespace_uri, false);
    result.Append('"');
  }
}

void MarkupFormatter::AppendText(StringBuilder& result, Text& text) {
  const String& str = text.data();
  AppendCharactersReplacingEntities(result, str, 0, str.length(),
                                    EntityMaskForText(text));
}

void MarkupFormatter::AppendComment(StringBuilder& result,
                                    const String& comment) {
  // FIXME: Comment content is not escaped, but XMLSerializer (and possibly
  // other callers) should raise an exception if it includes "-->".
  result.Append("<!--");
  result.Append(comment);
  result.Append("-->");
}

void MarkupFormatter::AppendXMLDeclaration(StringBuilder& result,
                                           const Document& document) {
  if (!document.HasXMLDeclaration())
    return;

  result.Append("<?xml version=\"");
  result.Append(document.xmlVersion());
  const String& encoding = document.xmlEncoding();
  if (!encoding.IsEmpty()) {
    result.Append("\" encoding=\"");
    result.Append(encoding);
  }
  if (document.XmlStandaloneStatus() != Document::kStandaloneUnspecified) {
    result.Append("\" standalone=\"");
    if (document.xmlStandalone())
      result.Append("yes");
    else
      result.Append("no");
  }

  result.Append("\"?>");
}

void MarkupFormatter::AppendDocumentType(StringBuilder& result,
                                         const DocumentType& n) {
  if (n.name().IsEmpty())
    return;

  result.Append("<!DOCTYPE ");
  result.Append(n.name());
  if (!n.publicId().IsEmpty()) {
    result.Append(" PUBLIC \"");
    result.Append(n.publicId());
    result.Append('"');
    if (!n.systemId().IsEmpty()) {
      result.Append(" \"");
      result.Append(n.systemId());
      result.Append('"');
    }
  } else if (!n.systemId().IsEmpty()) {
    result.Append(" SYSTEM \"");
    result.Append(n.systemId());
    result.Append('"');
  }
  result.Append('>');
}

void MarkupFormatter::AppendProcessingInstruction(StringBuilder& result,
                                                  const String& target,
                                                  const String& data) {
  // FIXME: PI data is not escaped, but XMLSerializer (and possibly other
  // callers) this should raise an exception if it includes "?>".
  result.Append("<?");
  result.Append(target);
  result.Append(' ');
  result.Append(data);
  result.Append("?>");
}

void MarkupFormatter::AppendOpenTag(StringBuilder& result,
                                    const Element& element,
                                    Namespaces* namespaces) {
  result.Append('<');
  result.Append(element.TagQName().ToString());
  if (!SerializeAsHTMLDocument(element) && namespaces &&
      ShouldAddNamespaceElement(element, *namespaces))
    AppendNamespace(result, element.prefix(), element.namespaceURI(),
                    *namespaces);
}

void MarkupFormatter::AppendCloseTag(StringBuilder& result,
                                     const Element& element) {
  if (ShouldSelfClose(element)) {
    if (element.IsHTMLElement())
      result.Append(' ');  // XHTML 1.0 <-> HTML compatibility.
    result.Append('/');
  }
  result.Append('>');
}

void MarkupFormatter::AppendAttribute(StringBuilder& result,
                                      const Element& element,
                                      const Attribute& attribute,
                                      Namespaces* namespaces) {
  bool document_is_html = SerializeAsHTMLDocument(element);

  QualifiedName prefixed_name = attribute.GetName();
  if (document_is_html) {
    if (attribute.NamespaceURI() == xmlns_names::kNamespaceURI) {
      if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
        prefixed_name.SetPrefix(g_xmlns_atom);
    } else if (attribute.NamespaceURI() == xml_names::kNamespaceURI) {
      prefixed_name.SetPrefix(g_xml_atom);
    } else if (attribute.NamespaceURI() == xlink_names::kNamespaceURI) {
      prefixed_name.SetPrefix(g_xlink_atom);
    }
    result.Append(' ');
    result.Append(prefixed_name.ToString());
  } else {
    if (attribute.NamespaceURI() == xmlns_names::kNamespaceURI) {
      if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
        prefixed_name.SetPrefix(g_xmlns_atom);
      // Account for the namespace attribute we're about to append.
      if (namespaces) {
        const AtomicString& lookup_key =
            (!attribute.Prefix()) ? g_empty_atom : attribute.LocalName();
        namespaces->Set(lookup_key, attribute.Value());
      }
    } else if (attribute.NamespaceURI() == xml_names::kNamespaceURI) {
      if (!attribute.Prefix())
        prefixed_name.SetPrefix(g_xml_atom);
    } else {
      if (attribute.NamespaceURI() == xlink_names::kNamespaceURI) {
        if (!attribute.Prefix())
          prefixed_name.SetPrefix(g_xlink_atom);
      }

      if (namespaces && ShouldAddNamespaceAttribute(attribute, element)) {
        if (!prefixed_name.Prefix()) {
          // This behavior is in process of being standardized. See
          // crbug.com/248044 and
          // https://www.w3.org/Bugs/Public/show_bug.cgi?id=24208
          String prefix_prefix("ns", 2u);
          for (unsigned i = attribute.NamespaceURI().Impl()->ExistingHash();;
               ++i) {
            AtomicString new_prefix(String(prefix_prefix + String::Number(i)));
            AtomicString found_uri = namespaces->at(new_prefix);
            if (found_uri == attribute.NamespaceURI() ||
                found_uri == g_null_atom) {
              // We already generated a prefix for this namespace.
              prefixed_name.SetPrefix(new_prefix);
              break;
            }
          }
        }
        DCHECK(prefixed_name.Prefix());
        AppendNamespace(result, prefixed_name.Prefix(),
                        attribute.NamespaceURI(), *namespaces);
      }
    }
    result.Append(' ');
    result.Append(prefixed_name.ToString());
  }

  result.Append('=');

  if (element.IsURLAttribute(attribute)) {
    AppendQuotedURLAttributeValue(result, element, attribute);
  } else {
    result.Append('"');
    AppendAttributeValue(result, attribute.Value(), document_is_html);
    result.Append('"');
  }
}

void MarkupFormatter::AppendCDATASection(StringBuilder& result,
                                         const String& section) {
  // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other
  // callers) should raise an exception if it includes "]]>".
  result.Append("<![CDATA[");
  result.Append(section);
  result.Append("]]>");
}

bool MarkupFormatter::ShouldAddNamespaceElement(const Element& element,
                                                Namespaces& namespaces) const {
  // Don't add namespace attribute if it is already defined for this elem.
  const AtomicString& prefix = element.prefix();
  if (prefix.IsEmpty()) {
    if (element.hasAttribute(g_xmlns_atom)) {
      namespaces.Set(g_empty_atom, element.namespaceURI());
      return false;
    }
    return true;
  }

  return !element.hasAttribute(WTF::g_xmlns_with_colon + prefix);
}

bool MarkupFormatter::ShouldAddNamespaceAttribute(
    const Attribute& attribute,
    const Element& element) const {
  // xmlns and xmlns:prefix attributes should be handled by another branch in
  // appendAttribute.
  DCHECK_NE(attribute.NamespaceURI(), xmlns_names::kNamespaceURI);

  // Attributes are in the null namespace by default.
  if (!attribute.NamespaceURI())
    return false;

  // Attributes without a prefix will need one generated for them, and an xmlns
  // attribute for that prefix.
  if (!attribute.Prefix())
    return true;

  return !element.hasAttribute(WTF::g_xmlns_with_colon + attribute.Prefix());
}

EntityMask MarkupFormatter::EntityMaskForText(const Text& text) const {
  if (!SerializeAsHTMLDocument(text))
    return kEntityMaskInPCDATA;

  // TODO(hajimehoshi): We need to switch EditingStrategy.
  const QualifiedName* parent_name = nullptr;
  if (text.parentElement())
    parent_name = &(text.parentElement())->TagQName();

  if (parent_name &&
      (*parent_name == scriptTag || *parent_name == styleTag ||
       *parent_name == xmpTag || *parent_name == iframeTag ||
       *parent_name == plaintextTag || *parent_name == noembedTag ||
       *parent_name == noframesTag ||
       (*parent_name == noscriptTag && text.GetDocument().GetFrame() &&
        text.GetDocument().CanExecuteScripts(kNotAboutToExecuteScript))))
    return kEntityMaskInCDATA;
  return kEntityMaskInHTMLPCDATA;
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not listed in spec will close with a
// separate end tag.
// 4. Other elements self-close.
bool MarkupFormatter::ShouldSelfClose(const Element& element) const {
  if (SerializeAsHTMLDocument(element))
    return false;
  if (element.HasChildren())
    return false;
  if (element.IsHTMLElement() && !ElementCannotHaveEndTag(element))
    return false;
  return true;
}

bool MarkupFormatter::SerializeAsHTMLDocument(const Node& node) const {
  if (serialization_type_ == SerializationType::kForcedXML)
    return false;
  return node.GetDocument().IsHTMLDocument();
}

}  // namespace blink
