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

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

struct EntityDescription {
  UChar entity;
  const std::string& reference;
  EntityMask mask;
};

template <typename CharType>
static inline void AppendCharactersReplacingEntitiesInternal(
    StringBuilder& result,
    const StringView& source,
    base::span<const CharType> text,
    base::span<const EntityDescription> entities,
    EntityMask entity_mask) {
  size_t position_after_last_entity = 0;
  // Avoid scanning the string in cases where the mask is empty, for example
  // scriptTag.innerHTML that use the kEntityMaskInCDATA mask.
  if (entity_mask) {
    for (size_t i = 0; i < text.size(); ++i) {
      const CharType c = text[i];
      for (size_t entity_index = 0; entity_index < entities.size();
           ++entity_index) {
        const auto& entity = entities[entity_index];
        if (c == entity.entity && entity.mask & entity_mask) {
          result.Append(text.subspan(position_after_last_entity,
                                     i - position_after_last_entity));
          result.Append(base::as_byte_span(entity.reference));
          position_after_last_entity = i + 1;
          break;
        }
      }
    }
  }
  // If we didn't find anything to replace use the fast path on StringBuilder
  // to avoid a copy. This optimizes cases like scriptTag.innerHTML or
  // p.innerHTML when the <p> contains a single Text.
  if (!position_after_last_entity) {
    result.Append(source);
    return;
  }
  result.Append(text.subspan(position_after_last_entity));
}

void MarkupFormatter::AppendCharactersReplacingEntities(
    StringBuilder& result,
    const StringView& source,
    EntityMask entity_mask) {
  DEFINE_STATIC_LOCAL(const std::string, amp_reference, ("&amp;"));
  DEFINE_STATIC_LOCAL(const std::string, lt_reference, ("&lt;"));
  DEFINE_STATIC_LOCAL(const std::string, gt_reference, ("&gt;"));
  DEFINE_STATIC_LOCAL(const std::string, quot_reference, ("&quot;"));
  DEFINE_STATIC_LOCAL(const std::string, nbsp_reference, ("&nbsp;"));
  DEFINE_STATIC_LOCAL(const std::string, tab_reference, ("&#9;"));
  DEFINE_STATIC_LOCAL(const std::string, line_feed_reference, ("&#10;"));
  DEFINE_STATIC_LOCAL(const std::string, carriage_return_reference, ("&#13;"));

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

  WTF::VisitCharacters(source, [&](auto chars) {
    AppendCharactersReplacingEntitiesInternal(result, source, chars,
                                              kEntityMaps, entity_mask);
  });
}

MarkupFormatter::MarkupFormatter(AbsoluteURLs resolve_urls_method,
                                 SerializationType serialization_type)
    : resolve_urls_method_(resolve_urls_method),
      serialization_type_(serialization_type) {}

String MarkupFormatter::ResolveURLIfNeeded(const Element& element,
                                           const Attribute& attribute) const {
  String value = attribute.Value();
  switch (resolve_urls_method_) {
    case kResolveAllURLs:
      if (element.IsURLAttribute(attribute))
        return element.GetDocument().CompleteURL(value).GetString();
      break;

    case kResolveNonLocalURLs:
      if (element.IsURLAttribute(attribute) &&
          !element.GetDocument().Url().IsLocalFile())
        return element.GetDocument().CompleteURL(value).GetString();
      break;

    case kDoNotResolveURLs:
      break;
  }
  return value;
}

void MarkupFormatter::AppendStartMarkup(StringBuilder& result,
                                        const Node& node) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      NOTREACHED_IN_MIGRATION();
      break;
    case Node::kCommentNode:
      AppendComment(result, To<Comment>(node).data());
      break;
    case Node::kDocumentNode:
      AppendXMLDeclaration(result, To<Document>(node));
      break;
    case Node::kDocumentFragmentNode:
      break;
    case Node::kDocumentTypeNode:
      AppendDocumentType(result, To<DocumentType>(node));
      break;
    case Node::kProcessingInstructionNode:
      AppendProcessingInstruction(result,
                                  To<ProcessingInstruction>(node).target(),
                                  To<ProcessingInstruction>(node).data());
      break;
    case Node::kElementNode:
      NOTREACHED_IN_MIGRATION();
      break;
    case Node::kCdataSectionNode:
      AppendCDATASection(result, To<CDATASection>(node).data());
      break;
    case Node::kAttributeNode:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void MarkupFormatter::AppendEndMarkup(StringBuilder& result,
                                      const Element& element) {
  AppendEndMarkup(result, element, element.prefix(), element.localName());
}

void MarkupFormatter::AppendEndMarkup(StringBuilder& result,
                                      const Element& element,
                                      const AtomicString& prefix,
                                      const AtomicString& local_name) {
  if (ShouldSelfClose(element) ||
      (!element.HasChildren() && ElementCannotHaveEndTag(element)))
    return;

  result.Append("</");
  if (!prefix.empty()) {
    result.Append(prefix);
    result.Append(":");
  }
  result.Append(local_name);
  result.Append('>');
}

void MarkupFormatter::AppendAttributeValue(StringBuilder& result,
                                           const String& attribute,
                                           bool document_is_html,
                                           const Document& document) {
  if (attribute.Contains('<') || attribute.Contains('>')) {
    document.CountUse(mojom::blink::WebFeature::kAttributeValueContainsLtOrGt);
  }

  EntityMask entity_mask =
      document_is_html
          ? (RuntimeEnabledFeatures::EscapeLtGtInAttributesEnabled()
                 ? kEntityExperimentalMaskInHTMLAttributeValue
                 : kEntityMaskInHTMLAttributeValue)
          : kEntityMaskInAttributeValue;

  AppendCharactersReplacingEntities(result, attribute, entity_mask);
}

void MarkupFormatter::AppendAttribute(StringBuilder& result,
                                      const AtomicString& prefix,
                                      const AtomicString& local_name,
                                      const String& value,
                                      bool document_is_html,
                                      const Document& document) {
  result.Append(' ');
  if (!prefix.empty()) {
    result.Append(prefix);
    result.Append(':');
  }
  result.Append(local_name);
  result.Append("=\"");
  AppendAttributeValue(result, value, document_is_html, document);
  result.Append('"');
}

void MarkupFormatter::AppendText(StringBuilder& result, const Text& text) {
  AppendCharactersReplacingEntities(result, text.data(),
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
  if (!encoding.empty()) {
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
  if (n.name().empty())
    return;

  result.Append("<!DOCTYPE ");
  result.Append(n.name());
  if (!n.publicId().empty()) {
    result.Append(" PUBLIC \"");
    result.Append(n.publicId());
    result.Append('"');
    if (!n.systemId().empty()) {
      result.Append(" \"");
      result.Append(n.systemId());
      result.Append('"');
    }
  } else if (!n.systemId().empty()) {
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

void MarkupFormatter::AppendStartTagOpen(StringBuilder& result,
                                         const Element& element) {
  AppendStartTagOpen(result, element.prefix(), element.localName());
}

void MarkupFormatter::AppendStartTagOpen(StringBuilder& result,
                                         const AtomicString& prefix,
                                         const AtomicString& local_name) {
  result.Append('<');
  if (!prefix.empty()) {
    result.Append(prefix);
    result.Append(":");
  }
  result.Append(local_name);
}

void MarkupFormatter::AppendStartTagClose(StringBuilder& result,
                                          const Element& element) {
  if (ShouldSelfClose(element)) {
    if (element.IsHTMLElement())
      result.Append(' ');  // XHTML 1.0 <-> HTML compatibility.
    result.Append('/');
  }
  result.Append('>');
}

void MarkupFormatter::AppendAttributeAsHTML(StringBuilder& result,
                                            const Attribute& attribute,
                                            const String& value,
                                            const Document& document) {
  // https://html.spec.whatwg.org/C/#attribute's-serialised-name
  QualifiedName prefixed_name = attribute.GetName();
  if (attribute.NamespaceURI() == xmlns_names::kNamespaceURI) {
    if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
      prefixed_name.SetPrefix(g_xmlns_atom);
  } else if (attribute.NamespaceURI() == xml_names::kNamespaceURI) {
    prefixed_name.SetPrefix(g_xml_atom);
  } else if (attribute.NamespaceURI() == xlink_names::kNamespaceURI) {
    prefixed_name.SetPrefix(g_xlink_atom);
  }
  AppendAttribute(result, prefixed_name.Prefix(), prefixed_name.LocalName(),
                  value, true, document);
}

void MarkupFormatter::AppendAttributeAsXMLWithoutNamespace(
    StringBuilder& result,
    const Attribute& attribute,
    const String& value,
    const Document& document) {
  const AtomicString& attribute_namespace = attribute.NamespaceURI();
  AtomicString candidate_prefix = attribute.Prefix();
  if (attribute_namespace == xmlns_names::kNamespaceURI) {
    if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
      candidate_prefix = g_xmlns_atom;
  } else if (attribute_namespace == xml_names::kNamespaceURI) {
    if (!candidate_prefix)
      candidate_prefix = g_xml_atom;
  } else if (attribute_namespace == xlink_names::kNamespaceURI) {
    if (!candidate_prefix)
      candidate_prefix = g_xlink_atom;
  }
  AppendAttribute(result, candidate_prefix, attribute.LocalName(), value, false,
                  document);
}

void MarkupFormatter::AppendCDATASection(StringBuilder& result,
                                         const String& section) {
  // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other
  // callers) should raise an exception if it includes "]]>".
  result.Append("<![CDATA[");
  result.Append(section);
  result.Append("]]>");
}

EntityMask MarkupFormatter::EntityMaskForText(const Text& text) const {
  if (!SerializeAsHTML())
    return kEntityMaskInPCDATA;

  // TODO(hajimehoshi): We need to switch EditingStrategy.
  const QualifiedName* parent_name = nullptr;
  if (text.parentElement())
    parent_name = &(text.parentElement())->TagQName();

  if (parent_name) {
    // For a NOSCRIPT tag, escape the string unless there's an execution context
    // and scripting is enabled. Note that some documents (e.g. the one created
    // by DOMParser) are created with a script-enabled execution context, but no
    // DOMWindow. But per spec [1], they should behave as if they have no
    // execution context. So check for a DOMWindow here.
    // [1] https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html
    bool is_noscript_tag_with_script_enabled =
        *parent_name == html_names::kNoscriptTag &&
        text.GetExecutionContext() && text.GetDocument().domWindow() &&
        text.GetExecutionContext()->CanExecuteScripts(kNotAboutToExecuteScript);
    if (*parent_name == html_names::kScriptTag ||
        *parent_name == html_names::kStyleTag ||
        *parent_name == html_names::kXmpTag ||
        *parent_name == html_names::kIFrameTag ||
        *parent_name == html_names::kPlaintextTag ||
        *parent_name == html_names::kNoembedTag ||
        *parent_name == html_names::kNoframesTag ||
        is_noscript_tag_with_script_enabled) {
      return kEntityMaskInCDATA;
    }
  }
  return kEntityMaskInHTMLPCDATA;
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not listed in spec will close with a
// separate end tag.
// 4. Other elements self-close.
bool MarkupFormatter::ShouldSelfClose(const Element& element) const {
  if (SerializeAsHTML())
    return false;
  if (element.HasChildren())
    return false;
  if (element.IsHTMLElement() && !ElementCannotHaveEndTag(element))
    return false;
  return true;
}

bool MarkupFormatter::SerializeAsHTML() const {
  return serialization_type_ == SerializationType::kHTML;
}

}  // namespace blink
