/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "third_party/blink/renderer/core/dom/dom_implementation.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/context_features.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/html_view_source_document.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_document.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/text_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/plugins/plugin_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

DOMImplementation::DOMImplementation(Document& document)
    : document_(document) {}

DocumentType* DOMImplementation::createDocumentType(
    const AtomicString& qualified_name,
    const String& public_id,
    const String& system_id,
    ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!Document::ParseQualifiedName(qualified_name, prefix, local_name,
                                    exception_state))
    return nullptr;

  return DocumentType::Create(document_, qualified_name, public_id, system_id);
}

XMLDocument* DOMImplementation::createDocument(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    DocumentType* doctype,
    ExceptionState& exception_state) {
  XMLDocument* doc = nullptr;
  DocumentInit init =
      DocumentInit::Create().WithContextDocument(document_->ContextDocument());
  if (namespace_uri == svg_names::kNamespaceURI) {
    doc = XMLDocument::CreateSVG(init);
  } else if (namespace_uri == HTMLNames::xhtmlNamespaceURI) {
    doc = XMLDocument::CreateXHTML(
        init.WithRegistrationContext(document_->RegistrationContext()));
  } else {
    doc = XMLDocument::Create(init);
  }

  doc->SetSecurityOrigin(document_->GetMutableSecurityOrigin());
  doc->SetContextFeatures(document_->GetContextFeatures());

  Node* document_element = nullptr;
  if (!qualified_name.IsEmpty()) {
    document_element =
        doc->createElementNS(namespace_uri, qualified_name, exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  if (doctype)
    doc->AppendChild(doctype);
  if (document_element)
    doc->AppendChild(document_element);

  return doc;
}

bool DOMImplementation::IsXMLMIMEType(const String& mime_type) {
  if (EqualIgnoringASCIICase(mime_type, "text/xml") ||
      EqualIgnoringASCIICase(mime_type, "application/xml") ||
      EqualIgnoringASCIICase(mime_type, "text/xsl"))
    return true;

  // Per RFCs 3023 and 2045, an XML MIME type is of the form:
  // ^[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+/[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+\+xml$

  int length = mime_type.length();
  if (length < 7)
    return false;

  if (mime_type[0] == '/' || mime_type[length - 5] == '/' ||
      !mime_type.EndsWithIgnoringASCIICase("+xml"))
    return false;

  bool has_slash = false;
  for (int i = 0; i < length - 4; ++i) {
    UChar ch = mime_type[i];
    if (ch >= '0' && ch <= '9')
      continue;
    if (ch >= 'a' && ch <= 'z')
      continue;
    if (ch >= 'A' && ch <= 'Z')
      continue;
    switch (ch) {
      case '_':
      case '-':
      case '+':
      case '~':
      case '!':
      case '$':
      case '^':
      case '{':
      case '}':
      case '|':
      case '.':
      case '%':
      case '\'':
      case '`':
      case '#':
      case '&':
      case '*':
        continue;
      case '/':
        if (has_slash)
          return false;
        has_slash = true;
        continue;
      default:
        return false;
    }
  }

  return true;
}

bool DOMImplementation::IsJSONMIMEType(const String& mime_type) {
  if (mime_type.StartsWithIgnoringASCIICase("application/json"))
    return true;
  if (mime_type.StartsWithIgnoringASCIICase("application/")) {
    size_t subtype = mime_type.FindIgnoringASCIICase("+json", 12);
    if (subtype != kNotFound) {
      // Just check that a parameter wasn't matched.
      size_t parameter_marker = mime_type.Find(";");
      if (parameter_marker == kNotFound) {
        unsigned end_subtype = static_cast<unsigned>(subtype) + 5;
        return end_subtype == mime_type.length() ||
               IsASCIISpace(mime_type[end_subtype]);
      }
      return parameter_marker > subtype;
    }
  }
  return false;
}

static bool IsTextPlainType(const String& mime_type) {
  return mime_type.StartsWithIgnoringASCIICase("text/") &&
         !(EqualIgnoringASCIICase(mime_type, "text/html") ||
           EqualIgnoringASCIICase(mime_type, "text/xml") ||
           EqualIgnoringASCIICase(mime_type, "text/xsl"));
}

bool DOMImplementation::IsTextMIMEType(const String& mime_type) {
  return MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
         IsJSONMIMEType(mime_type) || IsTextPlainType(mime_type);
}

Document* DOMImplementation::createHTMLDocument(const String& title) {
  DocumentInit init =
      DocumentInit::Create()
          .WithContextDocument(document_->ContextDocument())
          .WithRegistrationContext(document_->RegistrationContext());
  HTMLDocument* d = HTMLDocument::Create(init);
  d->open();
  d->write("<!doctype html><html><head></head><body></body></html>");
  if (!title.IsNull()) {
    HTMLHeadElement* head_element = d->head();
    DCHECK(head_element);
    HTMLTitleElement* title_element = HTMLTitleElement::Create(*d);
    head_element->AppendChild(title_element);
    title_element->AppendChild(d->createTextNode(title), ASSERT_NO_EXCEPTION);
  }
  d->SetSecurityOrigin(document_->GetMutableSecurityOrigin());
  d->SetContextFeatures(document_->GetContextFeatures());
  return d;
}

Document* DOMImplementation::createDocument(const String& type,
                                            const DocumentInit& init,
                                            bool in_view_source_mode) {
  if (in_view_source_mode)
    return HTMLViewSourceDocument::Create(init, type);

  // Plugins cannot take HTML and XHTML from us, and we don't even need to
  // initialize the plugin database for those.
  if (type == "text/html")
    return HTMLDocument::Create(init);
  if (type == "application/xhtml+xml")
    return XMLDocument::CreateXHTML(init);

  PluginData* plugin_data = nullptr;
  if (init.GetFrame() && init.GetFrame()->GetPage() &&
      init.GetFrame()->Loader().AllowPlugins(kNotAboutToInstantiatePlugin)) {
    // If the document is being created for the main frame,
    // init.frame()->tree().top()->securityContext() returns nullptr.
    // For that reason, the origin must be retrieved directly from init.url().
    if (init.GetFrame()->IsMainFrame()) {
      scoped_refptr<const SecurityOrigin> origin =
          SecurityOrigin::Create(init.Url());
      plugin_data = init.GetFrame()->GetPage()->GetPluginData(origin.get());
    } else {
      plugin_data =
          init.GetFrame()->GetPage()->GetPluginData(init.GetFrame()
                                                        ->Tree()
                                                        .Top()
                                                        .GetSecurityContext()
                                                        ->GetSecurityOrigin());
    }
  }

  // PDF is one image type for which a plugin can override built-in support.
  // We do not want QuickTime to take over all image types, obviously.
  if ((type == "application/pdf" || type == "text/pdf") && plugin_data &&
      plugin_data->SupportsMimeType(type)) {
    return PluginDocument::Create(
        init, plugin_data->PluginBackgroundColorForMimeType(type));
  }
  // multipart/x-mixed-replace is only supported for images.
  if (MIMETypeRegistry::IsSupportedImageResourceMIMEType(type) ||
      type == "multipart/x-mixed-replace") {
    return ImageDocument::Create(init);
  }

  // Check to see if the type can be played by our media player, if so create a
  // MediaDocument
  if (HTMLMediaElement::GetSupportsType(ContentType(type)))
    return MediaDocument::Create(init);

  // Everything else except text/plain can be overridden by plugins. In
  // particular, Adobe SVG Viewer should be used for SVG, if installed.
  // Disallowing plugins to use text/plain prevents plugins from hijacking a
  // fundamental type that the browser is expected to handle, and also serves as
  // an optimization to prevent loading the plugin database in the common case.
  if (type != "text/plain" && plugin_data &&
      plugin_data->SupportsMimeType(type)) {
    return PluginDocument::Create(
        init, plugin_data->PluginBackgroundColorForMimeType(type));
  }
  if (IsTextMIMEType(type))
    return TextDocument::Create(init);
  if (type == "image/svg+xml")
    return XMLDocument::CreateSVG(init);
  if (IsXMLMIMEType(type))
    return XMLDocument::Create(init);

  return HTMLDocument::Create(init);
}

void DOMImplementation::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
