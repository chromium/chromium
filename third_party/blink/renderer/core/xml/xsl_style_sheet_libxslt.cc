/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/xml/xsl_style_sheet.h"

#include <libxml/uri.h>
#include <libxslt/xsltutils.h>
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/transform_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/resource/xsl_style_sheet_resource.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser_scope.h"
#include "third_party/blink/renderer/core/xml/parser/xml_parser_input.h"
#include "third_party/blink/renderer/core/xml/xslt_processor.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

XSLStyleSheet::XSLStyleSheet(XSLStyleSheet* parent_style_sheet,
                             const String& original_url,
                             const KURL& final_url)
    : owner_node_(nullptr),
      original_url_(original_url),
      final_url_(final_url),
      is_disabled_(false),
      embedded_(false),
      // Child sheets get marked as processed when the libxslt engine has
      // finally seen them.
      processed_(false),
      stylesheet_doc_(nullptr),
      stylesheet_doc_taken_(false),
      compilation_failed_(false),
      parent_style_sheet_(parent_style_sheet),
      owner_document_(nullptr) {}

XSLStyleSheet::XSLStyleSheet(Node* parent_node,
                             const String& original_url,
                             const KURL& final_url,
                             bool embedded)
    : owner_node_(parent_node),
      original_url_(original_url),
      final_url_(final_url),
      is_disabled_(false),
      embedded_(embedded),
      processed_(true),  // The root sheet starts off processed.
      stylesheet_doc_(nullptr),
      stylesheet_doc_taken_(false),
      compilation_failed_(false),
      parent_style_sheet_(nullptr),
      owner_document_(nullptr) {}

XSLStyleSheet::XSLStyleSheet(Document* owner_document,
                             Node* style_sheet_root_node,
                             const String& original_url,
                             const KURL& final_url,
                             bool embedded)
    : owner_node_(style_sheet_root_node),
      original_url_(original_url),
      final_url_(final_url),
      is_disabled_(false),
      embedded_(embedded),
      processed_(true),  // The root sheet starts off processed.
      stylesheet_doc_(nullptr),
      stylesheet_doc_taken_(false),
      compilation_failed_(false),
      parent_style_sheet_(nullptr),
      owner_document_(owner_document) {}

XSLStyleSheet::~XSLStyleSheet() {
  if (!stylesheet_doc_taken_)
    xmlFreeDoc(stylesheet_doc_);
}

void XSLStyleSheet::CheckLoaded() {
  if (XSLStyleSheet* style_sheet = parentStyleSheet())
    style_sheet->CheckLoaded();
  if (ownerNode())
    ownerNode()->SheetLoaded();
}

xmlDocPtr XSLStyleSheet::GetDocument() {
  if (embedded_ && OwnerDocument() && OwnerDocument()->GetTransformSource())
    return (xmlDocPtr)OwnerDocument()->GetTransformSource()->PlatformSource();
  return stylesheet_doc_;
}

void XSLStyleSheet::ClearDocuments() {
  stylesheet_doc_ = nullptr;
  for (unsigned i = 0; i < children_.size(); ++i)
    children_.at(i)->ClearDocuments();
}

bool XSLStyleSheet::ParseString(const String& source) {
  // Parse in a single chunk into an xmlDocPtr
  if (!stylesheet_doc_taken_)
    xmlFreeDoc(stylesheet_doc_);
  stylesheet_doc_taken_ = false;

  FrameConsole* console = nullptr;
  if (LocalFrame* frame = OwnerDocument()->GetFrame())
    console = &frame->Console();

  XMLDocumentParserScope scope(OwnerDocument(), XSLTProcessor::GenericErrorFunc,
                               XSLTProcessor::ParseErrorFunc, console);
  XMLParserInput input(source);

  xmlParserCtxtPtr ctxt = xmlCreateMemoryParserCtxt(input.Data(), input.size());
  if (!ctxt)
    return 0;

  if (parent_style_sheet_) {
    // The XSL transform may leave the newly-transformed document
    // with references to the symbol dictionaries of the style sheet
    // and any of its children. XML document disposal can corrupt memory
    // if a document uses more than one symbol dictionary, so we
    // ensure that all child stylesheets use the same dictionaries as their
    // parents.
    xmlDictFree(ctxt->dict);
    ctxt->dict = parent_style_sheet_->stylesheet_doc_->dict;
    xmlDictReference(ctxt->dict);
  }

  stylesheet_doc_ =
      xmlCtxtReadMemory(ctxt, input.Data(), input.size(),
                        final_url_.GetString().Utf8().c_str(), input.Encoding(),
                        XML_PARSE_NOENT | XML_PARSE_DTDATTR |
                            XML_PARSE_NOWARNING | XML_PARSE_NOCDATA);

  xmlFreeParserCtxt(ctxt);
  LoadChildSheets();
  return stylesheet_doc_;
}

void XSLStyleSheet::LoadChildSheets() {
  if (!GetDocument())
    return;

  xmlNodePtr stylesheet_root = GetDocument()->children;

  // Top level children may include other things such as DTD nodes, we ignore
  // those.
  while (stylesheet_root && stylesheet_root->type != XML_ELEMENT_NODE)
    stylesheet_root = stylesheet_root->next;

  if (embedded_) {
    // We have to locate (by ID) the appropriate embedded stylesheet
    // element, so that we can walk the import/include list.
    xmlAttrPtr id_node = xmlGetID(
        GetDocument(), (const xmlChar*)(final_url_.GetString().Utf8().c_str()));
    if (!id_node)
      return;
    stylesheet_root = id_node->parent;
  } else {
    // FIXME: Need to handle an external URI with a # in it. This is a
    // pretty minor edge case, so we'll deal with it later.
  }

  if (stylesheet_root) {
    // Walk the children of the root element and look for import/include
    // elements. Imports must occur first.
    xmlNodePtr curr = stylesheet_root->children;
    while (curr) {
      if (curr->type != XML_ELEMENT_NODE) {
        curr = curr->next;
        continue;
      }
      if (IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "import")) {
        xmlChar* uri_ref =
            xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);
        LoadChildSheet(String::FromUTF8((const char*)uri_ref));
        xmlFree(uri_ref);
      } else {
        break;
      }
      curr = curr->next;
    }

    // Now handle includes.
    while (curr) {
      if (curr->type == XML_ELEMENT_NODE && IS_XSLT_ELEM(curr) &&
          IS_XSLT_NAME(curr, "include")) {
        xmlChar* uri_ref =
            xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);
        LoadChildSheet(String::FromUTF8((const char*)uri_ref));
        xmlFree(uri_ref);
      }
      curr = curr->next;
    }
  }
}

void XSLStyleSheet::LoadChildSheet(const String& href) {
  // Use parent styleheet's URL as the base URL
  KURL url(BaseURL(), href);

  // Check for a cycle in our import chain. If we encounter a stylesheet in
  // our parent chain with the same URL, then just bail.
  for (XSLStyleSheet* parent_sheet = parentStyleSheet(); parent_sheet;
       parent_sheet = parent_sheet->parentStyleSheet()) {
    if (url == parent_sheet->BaseURL())
      return;
  }

  const String& url_string = url.GetString();
  ResourceLoaderOptions fetch_options;
  fetch_options.initiator_info.name = fetch_initiator_type_names::kXml;
  FetchParameters params(
      ResourceRequest(OwnerDocument()->CompleteURL(url_string)), fetch_options);
  params.MutableResourceRequest().SetMode(
      network::mojom::RequestMode::kSameOrigin);
  XSLStyleSheetResource* resource = XSLStyleSheetResource::FetchSynchronously(
      params, OwnerDocument()->Fetcher());
  if (!resource->Sheet())
    return;

  XSLStyleSheet* style_sheet = MakeGarbageCollected<XSLStyleSheet>(
      this, url_string, resource->GetResponse().CurrentRequestUrl());
  children_.push_back(style_sheet);
  style_sheet->ParseString(resource->Sheet());
  CheckLoaded();
}

xsltStylesheetPtr XSLStyleSheet::CompileStyleSheet() {
  // FIXME: Hook up error reporting for the stylesheet compilation process.
  if (embedded_)
    return xsltLoadStylesheetPI(GetDocument());

  // Certain libxslt versions are corrupting the xmlDoc on compilation
  // failures - hence attempting to recompile after a failure is unsafe.
  if (compilation_failed_)
    return nullptr;

  // xsltParseStylesheetDoc makes the document part of the stylesheet
  // so we have to release our pointer to it.
  DCHECK(!stylesheet_doc_taken_);
  xsltStylesheetPtr result = xsltParseStylesheetDoc(stylesheet_doc_);
  if (result)
    stylesheet_doc_taken_ = true;
  else
    compilation_failed_ = true;
  return result;
}

Document* XSLStyleSheet::OwnerDocument() {
  for (XSLStyleSheet* style_sheet = this; style_sheet;
       style_sheet = style_sheet->parentStyleSheet()) {
    if (style_sheet->owner_document_)
      return style_sheet->owner_document_.Get();
    Node* node = style_sheet->ownerNode();
    if (node)
      return &node->GetDocument();
  }
  return nullptr;
}

xmlDocPtr XSLStyleSheet::LocateStylesheetSubResource(xmlDocPtr parent_doc,
                                                     const xmlChar* uri) {
  bool matched_parent = (parent_doc == GetDocument());
  for (unsigned i = 0; i < children_.size(); ++i) {
    XSLStyleSheet* child = children_.at(i).Get();
    if (matched_parent) {
      if (child->Processed())
        continue;  // libxslt has been given this sheet already.

      // Check the URI of the child stylesheet against the doc URI.
      // In order to ensure that libxml canonicalized both URLs, we get
      // the original href string from the import rule and canonicalize it
      // using libxml before comparing it with the URI argument.
      std::string import_href = child->href().Utf8();
      xmlChar* base = xmlNodeGetBase(parent_doc, (xmlNodePtr)parent_doc);
      xmlChar* child_uri =
          xmlBuildURI((const xmlChar*)import_href.c_str(), base);
      bool equal_ur_is = xmlStrEqual(uri, child_uri);
      xmlFree(base);
      xmlFree(child_uri);
      if (equal_ur_is) {
        child->MarkAsProcessed();
        return child->GetDocument();
      }
      continue;
    }
    xmlDocPtr result = child->LocateStylesheetSubResource(parent_doc, uri);
    if (result)
      return result;
  }

  return nullptr;
}

void XSLStyleSheet::MarkAsProcessed() {
  DCHECK(!processed_);
  DCHECK(!stylesheet_doc_taken_);
  processed_ = true;
  stylesheet_doc_taken_ = true;
}

void XSLStyleSheet::Trace(blink::Visitor* visitor) {
  visitor->Trace(owner_node_);
  visitor->Trace(children_);
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(owner_document_);
  StyleSheet::Trace(visitor);
}

}  // namespace blink
