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
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

namespace {

struct EntityDescription {
  UChar entity;
  const std::string& reference;
  EntityMask mask;
};

template <typename CharType>
inline void AppendCharactersReplacingEntitiesInternal(
    const StringView& source,
    base::span<const CharType> text,
    base::span<const EntityDescription> entities,
    EntityMask entity_mask,
    StringBuilder& result) {
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

// https://html.spec.whatwg.org/C/#attribute's-serialised-name
const AtomicString& ResolveAttributePrefixForHtml(
    const QualifiedName& attr_name) {
  if (attr_name.NamespaceURI() == xmlns_names::kNamespaceURI) {
    if (!attr_name.Prefix() && attr_name.LocalName() != g_xmlns_atom) {
      return g_xmlns_atom;
    }
  } else if (attr_name.NamespaceURI() == xml_names::kNamespaceURI) {
    return g_xml_atom;
  } else if (attr_name.NamespaceURI() == xlink_names::kNamespaceURI) {
    return g_xlink_atom;
  }
  return attr_name.Prefix();
}

const AtomicString& ResolveAttributePrefixForXml(
    const QualifiedName& attr_name) {
  if (attr_name.Prefix()) {
    return attr_name.Prefix();
  }
  const AtomicString& attribute_namespace = attr_name.NamespaceURI();
  if (attribute_namespace == xmlns_names::kNamespaceURI) {
    if (attr_name.LocalName() != g_xmlns_atom) {
      return g_xmlns_atom;
    }
  } else if (attribute_namespace == xml_names::kNamespaceURI) {
    return g_xml_atom;
  } else if (attribute_namespace == xlink_names::kNamespaceURI) {
    return g_xlink_atom;
  }
  return g_null_atom;
}

}  // namespace

void MarkupFormatter::AppendCharactersReplacingEntities(
    const StringView& source,
    EntityMask entity_mask,
    StringBuilder& result) {
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
      {uchar::kNoBreakSpace, nbsp_reference, kEntityNbsp},
      {'\t', tab_reference, kEntityTab},
      {'\n', line_feed_reference, kEntityLineFeed},
      {'\r', carriage_return_reference, kEntityCarriageReturn},
  };

  VisitCharacters(source, [&](auto chars) {
    AppendCharactersReplacingEntitiesInternal(source, chars, kEntityMaps,
                                              entity_mask, result);
  });
}

MarkupFormatter::MarkupFormatter(ResolveUrls resolve_urls_method,
                                 SerializationType serialization_type)
    : resolve_urls_method_(resolve_urls_method),
      serialization_type_(serialization_type) {}

String MarkupFormatter::ResolveUrlIfNeeded(const Element& element,
                                           const Attribute& attribute) const {
  String value = attribute.Value();
  switch (resolve_urls_method_) {
    case ResolveUrls::kAll:
      if (element.IsURLAttribute(attribute))
        return element.GetDocument().CompleteURL(value).GetString();
      break;

    case ResolveUrls::kNonLocal:
      if (element.IsURLAttribute(attribute) &&
          !element.GetDocument().Url().IsLocalFile())
        return element.GetDocument().CompleteURL(value).GetString();
      break;

    case ResolveUrls::kNone:
      break;
  }
  return value;
}

void MarkupFormatter::AppendStartMarkup(const Node& node,
                                        StringBuilder& result) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      NOTREACHED();
    case Node::kCommentNode:
      AppendComment(To<Comment>(node).data(), result);
      break;
    case Node::kDocumentNode:
      AppendXmlDeclaration(To<Document>(node), result);
      break;
    case Node::kDocumentFragmentNode:
      break;
    case Node::kDocumentTypeNode:
      AppendDocumentType(To<DocumentType>(node), result);
      break;
    case Node::kProcessingInstructionNode: {
      const auto& pi = To<ProcessingInstruction>(node);
      AppendProcessingInstruction(pi.target(), pi.data(), result);
      break;
    }
    case Node::kElementNode:
      NOTREACHED();
    case Node::kCdataSectionNode: {
      const auto& cdata = To<CDATASection>(node);
      if (SerializeAsHtml()) {
        AppendText(cdata, result);
      } else {
        AppendCdataSection(cdata.data(), result);
      }
      break;
    }
    case Node::kAttributeNode:
      NOTREACHED();
  }
}

void MarkupFormatter::AppendEndMarkup(const Element& element,
                                      StringBuilder& result) {
  AppendEndMarkup(element, element.prefix(), element.localName(), result);
}

void MarkupFormatter::AppendEndMarkup(const Element& element,
                                      const AtomicString& prefix,
                                      const AtomicString& local_name,
                                      StringBuilder& result) {
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

void MarkupFormatter::AppendAttributeValue(const String& attribute,
                                           SerializationType type,
                                           StringBuilder& result) {
  EntityMask entity_mask = type == SerializationType::kHtml
                               ? kEntityMaskInHtmlAttributeValue
                               : kEntityMaskInAttributeValue;
  AppendCharactersReplacingEntities(attribute, entity_mask, result);
}

void MarkupFormatter::AppendAttribute(const AtomicString& prefix,
                                      const AtomicString& local_name,
                                      const String& value,
                                      SerializationType type,
                                      StringBuilder& result) {
  result.Append(' ');
  if (!prefix.empty()) {
    result.Append(prefix);
    result.Append(':');
  }
  result.Append(local_name);
  result.Append("=\"");
  AppendAttributeValue(value, type, result);
  result.Append('"');
}

void MarkupFormatter::AppendText(const Text& text, StringBuilder& result) {
  AppendCharactersReplacingEntities(text.data(), EntityMaskForText(text),
                                    result);
}

void MarkupFormatter::AppendComment(const String& comment,
                                    StringBuilder& result) {
  // FIXME: Comment content is not escaped, but XMLSerializer (and possibly
  // other callers) should raise an exception if it includes "-->".
  result.Append("<!--");
  result.Append(comment);
  result.Append("-->");
}

void MarkupFormatter::AppendXmlDeclaration(const Document& document,
                                           StringBuilder& result) {
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

void MarkupFormatter::AppendDocumentType(const DocumentType& n,
                                         StringBuilder& result) {
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

void MarkupFormatter::AppendProcessingInstruction(const String& target,
                                                  const String& data,
                                                  StringBuilder& result) {
  // FIXME: PI data is not escaped, but XMLSerializer (and possibly other
  // callers) this should raise an exception if it includes "?>".
  result.Append("<?");
  result.Append(target);
  result.Append(' ');
  result.Append(data);
  result.Append("?>");
}

void MarkupFormatter::AppendStartTagOpen(const Element& element,
                                         StringBuilder& result) {
  AppendStartTagOpen(element.prefix(), element.localName(), result);
}

void MarkupFormatter::AppendStartTagOpen(const AtomicString& prefix,
                                         const AtomicString& local_name,
                                         StringBuilder& result) {
  result.Append('<');
  if (!prefix.empty()) {
    result.Append(prefix);
    result.Append(":");
  }
  result.Append(local_name);
}

void MarkupFormatter::AppendStartTagClose(const Element& element,
                                          StringBuilder& result) {
  if (ShouldSelfClose(element)) {
    if (element.IsHTMLElement())
      result.Append(' ');  // XHTML 1.0 <-> HTML compatibility.
    result.Append('/');
  }
  result.Append('>');
}

void MarkupFormatter::AppendAttributeAsHtml(const Attribute& attribute,
                                            const String& value,
                                            StringBuilder& result) {
  const AtomicString& resolved_prefix =
      ResolveAttributePrefixForHtml(attribute.GetName());
  AppendAttribute(resolved_prefix, attribute.LocalName(), value,
                  SerializationType::kHtml, result);
}

void MarkupFormatter::AppendAttributeAsXmlWithoutNamespace(
    const Attribute& attribute,
    const String& value,
    StringBuilder& result) {
  const AtomicString& resolved_prefix =
      ResolveAttributePrefixForXml(attribute.GetName());
  AppendAttribute(resolved_prefix, attribute.LocalName(), value,
                  SerializationType::kXml, result);
}

void MarkupFormatter::AppendCdataSection(const String& section,
                                         StringBuilder& result) {
  // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other
  // callers) should raise an exception if it includes "]]>".
  result.Append("<![CDATA[");
  result.Append(section);
  result.Append("]]>");
}

EntityMask MarkupFormatter::EntityMaskForText(const Text& text) const {
  if (!SerializeAsHtml()) {
    return kEntityMaskInPcdata;
  }

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
      return kEntityMaskInCdata;
    }
  }
  return kEntityMaskInHtmlPcdata;
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not listed in spec will close with a
// separate end tag.
// 4. Other elements self-close.
bool MarkupFormatter::ShouldSelfClose(const Element& element) const {
  if (SerializeAsHtml()) {
    return false;
  }
  if (element.HasChildren())
    return false;
  if (element.IsHTMLElement() && !ElementCannotHaveEndTag(element))
    return false;
  return true;
}

bool MarkupFormatter::SerializeAsHtml() const {
  return serialization_type_ == SerializationType::kHtml;
}

}  // namespace blink
