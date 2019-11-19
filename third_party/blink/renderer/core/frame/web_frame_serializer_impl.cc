/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

// How we handle the base tag better.
// Current status:
// At now the normal way we use to handling base tag is
// a) For those links which have corresponding local saved files, such as
// savable CSS, JavaScript files, they will be written to relative URLs which
// point to local saved file. Why those links can not be resolved as absolute
// file URLs, because if they are resolved as absolute URLs, after moving the
// file location from one directory to another directory, the file URLs will
// be dead links.
// b) For those links which have not corresponding local saved files, such as
// links in A, AREA tags, they will be resolved as absolute URLs.
// c) We comment all base tags when serialzing DOM for the page.
// FireFox also uses above way to handle base tag.
//
// Problem:
// This way can not handle the following situation:
// the base tag is written by JavaScript.
// For example. The page "www.yahoo.com" use
// "document.write('<base href="http://www.yahoo.com/"...');" to setup base URL
// of page when loading page. So when saving page as completed-HTML, we assume
// that we save "www.yahoo.com" to "c:\yahoo.htm". After then we load the saved
// completed-HTML page, then the JavaScript will insert a base tag
// <base href="http://www.yahoo.com/"...> to DOM, so all URLs which point to
// local saved resource files will be resolved as
// "http://www.yahoo.com/yahoo_files/...", which will cause all saved  resource
// files can not be loaded correctly. Also the page will be rendered ugly since
// all saved sub-resource files (such as CSS, JavaScript files) and sub-frame
// files can not be fetched.
// Now FireFox, IE and WebKit based Browser all have this problem.
//
// Solution:
// My solution is that we comment old base tag and write new base tag:
// <base href="." ...> after the previous commented base tag. In WebKit, it
// always uses the latest "href" attribute of base tag to set document's base
// URL. Based on this behavior, when we encounter a base tag, we comment it and
// write a new base tag <base href="."> after the previous commented base tag.
// The new added base tag can help engine to locate correct base URL for
// correctly loading local saved resource files. Also I think we need to inherit
// the base target value from document object when appending new base tag.
// If there are multiple base tags in original document, we will comment all old
// base tags and append new base tag after each old base tag because we do not
// know those old base tags are original content or added by JavaScript. If
// they are added by JavaScript, it means when loading saved page, the script(s)
// will still insert base tag(s) to DOM, so the new added base tag(s) can
// override the incorrect base URL and make sure we alway load correct local
// saved resource files.

#include "third_party/blink/renderer/core/frame/web_frame_serializer_impl.h"

#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/frame_serializer.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

// Generate the default base tag declaration.
String GenerateBaseTagDeclaration(const String& base_target) {
  // TODO(yosin) We should call |FrameSerializer::baseTagDeclarationOf()|.
  if (base_target.IsEmpty())
    return String("<base href=\".\">");
  String base_string = "<base href=\".\" target=\"" + base_target + "\">";
  return base_string;
}

}  // namespace

// Maximum length of data buffer which is used to temporary save generated
// html content data. This is a soft limit which might be passed if a very large
// contegious string is found in the html document.
static const unsigned kDataBufferCapacity = 65536;

WebFrameSerializerImpl::SerializeDomParam::SerializeDomParam(
    const KURL& url,
    const WTF::TextEncoding& text_encoding,
    Document* document)
    : url(url),
      text_encoding(text_encoding),
      document(document),
      is_html_document(document->IsHTMLDocument()),
      have_seen_doc_type(false),
      have_added_charset_declaration(false),
      skip_meta_element(nullptr),
      have_added_xml_processing_directive(false),
      have_added_contents_before_end(false) {}

String WebFrameSerializerImpl::PreActionBeforeSerializeOpenTag(
    const Element* element,
    SerializeDomParam* param,
    bool* need_skip) {
  StringBuilder result;

  *need_skip = false;
  if (param->is_html_document) {
    // Skip the open tag of original META tag which declare charset since we
    // have overrided the META which have correct charset declaration after
    // serializing open tag of HEAD element.
    DCHECK(element);
    auto* meta = DynamicTo<HTMLMetaElement>(element);
    if (meta && meta->ComputeEncoding().IsValid()) {
      // Found META tag declared charset, we need to skip it when
      // serializing DOM.
      param->skip_meta_element = element;
      *need_skip = true;
    } else if (IsA<HTMLHtmlElement>(element)) {
      // Check something before processing the open tag of HEAD element.
      // First we add doc type declaration if original document has it.
      if (!param->have_seen_doc_type) {
        param->have_seen_doc_type = true;
        result.Append(CreateMarkup(param->document->doctype()));
      }

      // Add MOTW declaration before html tag.
      // See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
      result.Append(
          WebFrameSerializer::GenerateMarkOfTheWebDeclaration(param->url));
    } else if (IsA<HTMLBaseElement>(*element)) {
      // Comment the BASE tag when serializing dom.
      result.Append("<!--");
    }
  } else {
    // Write XML declaration.
    if (!param->have_added_xml_processing_directive) {
      param->have_added_xml_processing_directive = true;
      // Get encoding info.
      String xml_encoding = param->document->xmlEncoding();
      if (xml_encoding.IsEmpty())
        xml_encoding = param->document->EncodingName();
      if (xml_encoding.IsEmpty())
        xml_encoding = UTF8Encoding().GetName();
      result.Append("<?xml version=\"");
      result.Append(param->document->xmlVersion());
      result.Append("\" encoding=\"");
      result.Append(xml_encoding);
      if (param->document->xmlStandalone())
        result.Append("\" standalone=\"yes");
      result.Append("\"?>\n");
    }
    // Add doc type declaration if original document has it.
    if (!param->have_seen_doc_type) {
      param->have_seen_doc_type = true;
      result.Append(CreateMarkup(param->document->doctype()));
    }
  }
  return result.ToString();
}

String WebFrameSerializerImpl::PostActionAfterSerializeOpenTag(
    const Element* element,
    SerializeDomParam* param) {
  StringBuilder result;

  param->have_added_contents_before_end = false;
  if (!param->is_html_document)
    return result.ToString();
  // Check after processing the open tag of HEAD element
  if (!param->have_added_charset_declaration &&
      IsA<HTMLHeadElement>(*element)) {
    param->have_added_charset_declaration = true;
    // Check meta element. WebKit only pre-parse the first 512 bytes of the
    // document. If the whole <HEAD> is larger and meta is the end of head
    // part, then this kind of html documents aren't decoded correctly
    // because of this issue. So when we serialize the DOM, we need to make
    // sure the meta will in first child of head tag.
    // See http://bugs.webkit.org/show_bug.cgi?id=16621.
    // First we generate new content for writing correct META element.
    result.Append(WebFrameSerializer::GenerateMetaCharsetDeclaration(
        String(param->text_encoding.GetName())));

    param->have_added_contents_before_end = true;
    // Will search each META which has charset declaration, and skip them all
    // in PreActionBeforeSerializeOpenTag.
  }

  return result.ToString();
}

String WebFrameSerializerImpl::PreActionBeforeSerializeEndTag(
    const Element* element,
    SerializeDomParam* param,
    bool* need_skip) {
  String result;

  *need_skip = false;
  if (!param->is_html_document)
    return result;
  // Skip the end tag of original META tag which declare charset.
  // Need not to check whether it's META tag since we guarantee
  // skipMetaElement is definitely META tag if it's not 0.
  if (param->skip_meta_element == element) {
    *need_skip = true;
  }

  return result;
}

// After we finish serializing end tag of a element, we give the target
// element a chance to do some post work to add some additional data.
String WebFrameSerializerImpl::PostActionAfterSerializeEndTag(
    const Element* element,
    SerializeDomParam* param) {
  StringBuilder result;

  if (!param->is_html_document)
    return result.ToString();
  // Comment the BASE tag when serializing DOM.
  if (IsA<HTMLBaseElement>(*element)) {
    result.Append("-->");
    // Append a new base tag declaration.
    result.Append(GenerateBaseTagDeclaration(param->document->BaseTarget()));
  }

  return result.ToString();
}

void WebFrameSerializerImpl::SaveHTMLContentToBuffer(const String& result,
                                                     SerializeDomParam* param) {
  data_buffer_.Append(result);
  EncodeAndFlushBuffer(WebFrameSerializerClient::kCurrentFrameIsNotFinished,
                       param, kDoNotForceFlush);
}

void WebFrameSerializerImpl::EncodeAndFlushBuffer(
    WebFrameSerializerClient::FrameSerializationStatus status,
    SerializeDomParam* param,
    FlushOption flush_option) {
  // Data buffer is not full nor do we want to force flush.
  if (flush_option != kForceFlush &&
      data_buffer_.length() <= kDataBufferCapacity)
    return;

  String content = data_buffer_.ToString();
  data_buffer_.Clear();

  std::string encoded_content =
      param->text_encoding.Encode(content, WTF::kEntitiesForUnencodables);

  // Send result to the client.
  client_->DidSerializeDataForFrame(
      WebVector<char>(encoded_content.c_str(), encoded_content.length()),
      status);
}

// TODO(yosin): We should utilize |MarkupFormatter| here to share code,
// especially escaping attribute values, done by |WebEntities| |m_htmlEntities|
// and |m_xmlEntities|.
void WebFrameSerializerImpl::AppendAttribute(StringBuilder& result,
                                             bool is_html_document,
                                             const String& attr_name,
                                             const String& attr_value) {
  result.Append(' ');
  result.Append(attr_name);
  result.Append("=\"");
  if (is_html_document)
    result.Append(html_entities_.ConvertEntitiesInString(attr_value));
  else
    result.Append(xml_entities_.ConvertEntitiesInString(attr_value));
  result.Append('\"');
}

void WebFrameSerializerImpl::OpenTagToString(Element* element,
                                             SerializeDomParam* param) {
  bool need_skip;
  StringBuilder result;
  // Do pre action for open tag.
  result.Append(PreActionBeforeSerializeOpenTag(element, param, &need_skip));
  if (need_skip)
    return;
  // Add open tag
  result.Append('<');
  result.Append(element->nodeName().DeprecatedLower());

  // Find out if we need to do frame-specific link rewriting.
  WebFrame* frame = nullptr;
  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(element)) {
    frame = WebFrame::FromFrame(frame_owner_element->ContentFrame());
  }
  WebString rewritten_frame_link;
  bool should_rewrite_frame_src =
      frame && delegate_->RewriteFrameSource(frame, &rewritten_frame_link);
  bool did_rewrite_frame_src = false;

  // Go through all attributes and serialize them.
  for (const auto& it : element->Attributes()) {
    const QualifiedName& attr_name = it.GetName();
    String attr_value = it.Value();

    // Skip srcdoc attribute if we will emit src attribute (for frames).
    if (should_rewrite_frame_src && attr_name == html_names::kSrcdocAttr)
      continue;

    // Rewrite the attribute value if requested.
    if (element->HasLegalLinkAttribute(attr_name)) {
      // For links start with "javascript:", we do not change it.
      if (!attr_value.StartsWithIgnoringASCIICase("javascript:")) {
        // Get the absolute link.
        KURL complete_url = param->document->CompleteURL(attr_value);

        // Check whether we have a local file to link to.
        WebString rewritten_url;
        if (should_rewrite_frame_src) {
          attr_value = rewritten_frame_link;
          did_rewrite_frame_src = true;
        } else if (delegate_->RewriteLink(complete_url, &rewritten_url)) {
          attr_value = rewritten_url;
        } else {
          attr_value = complete_url;
        }
      }
    }

    AppendAttribute(result, param->is_html_document, attr_name.ToString(),
                    attr_value);
  }

  // For frames where link rewriting was requested, ensure that src attribute
  // is written even if the original document didn't have that attribute
  // (mainly needed for iframes with srcdoc, but with no src attribute).
  if (should_rewrite_frame_src && !did_rewrite_frame_src &&
      IsA<HTMLIFrameElement>(element)) {
    AppendAttribute(result, param->is_html_document,
                    html_names::kSrcAttr.ToString(), rewritten_frame_link);
  }

  // Do post action for open tag.
  String added_contents = PostActionAfterSerializeOpenTag(element, param);
  // Complete the open tag for element when it has child/children.
  if (element->HasChildren() || param->have_added_contents_before_end)
    result.Append('>');
  // Append the added contents generate in  post action of open tag.
  result.Append(added_contents);
  // Save the result to data buffer.
  SaveHTMLContentToBuffer(result.ToString(), param);
}

// Serialize end tag of an specified element.
void WebFrameSerializerImpl::EndTagToString(Element* element,
                                            SerializeDomParam* param) {
  bool need_skip;
  StringBuilder result;
  // Do pre action for end tag.
  result.Append(PreActionBeforeSerializeEndTag(element, param, &need_skip));
  if (need_skip)
    return;
  // Write end tag when element has child/children.
  if (element->HasChildren() || param->have_added_contents_before_end) {
    result.Append("</");
    result.Append(element->nodeName().DeprecatedLower());
    result.Append('>');
  } else {
    // Check whether we have to write end tag for empty element.
    if (param->is_html_document) {
      result.Append('>');
      // FIXME: This code is horribly wrong.  WebFrameSerializerImpl must die.
      auto* html_element = DynamicTo<HTMLElement>(element);
      if (!html_element || html_element->ShouldSerializeEndTag()) {
        // We need to write end tag when it is required.
        result.Append("</");
        result.Append(element->nodeName().DeprecatedLower());
        result.Append('>');
      }
    } else {
      // For xml base document.
      result.Append(" />");
    }
  }
  // Do post action for end tag.
  result.Append(PostActionAfterSerializeEndTag(element, param));
  // Save the result to data buffer.
  SaveHTMLContentToBuffer(result.ToString(), param);
}

void WebFrameSerializerImpl::BuildContentForNode(Node* node,
                                                 SerializeDomParam* param) {
  switch (node->getNodeType()) {
    case Node::kElementNode:
      // Process open tag of element.
      OpenTagToString(To<Element>(node), param);
      // Walk through the children nodes and process it.
      for (Node* child = node->firstChild(); child;
           child = child->nextSibling())
        BuildContentForNode(child, param);
      // Process end tag of element.
      EndTagToString(To<Element>(node), param);
      break;
    case Node::kTextNode:
      SaveHTMLContentToBuffer(CreateMarkup(node), param);
      break;
    case Node::kAttributeNode:
    case Node::kDocumentNode:
    case Node::kDocumentFragmentNode:
      // Should not exist.
      NOTREACHED();
      break;
    // Document type node can be in DOM?
    case Node::kDocumentTypeNode:
      param->have_seen_doc_type = true;
      FALLTHROUGH;
    default:
      // For other type node, call default action.
      SaveHTMLContentToBuffer(CreateMarkup(node), param);
      break;
  }
}

WebFrameSerializerImpl::WebFrameSerializerImpl(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate,
    bool save_with_empty_url)
    : client_(client),
      delegate_(delegate),
      save_with_empty_url_(save_with_empty_url),
      html_entities_(false),
      xml_entities_(true) {
  // Must specify available webframe.
  DCHECK(frame);
  specified_web_local_frame_impl_ = To<WebLocalFrameImpl>(frame);
  // Make sure we have non null client and delegate.
  DCHECK(client);
  DCHECK(delegate);

  DCHECK(data_buffer_.IsEmpty());
}

bool WebFrameSerializerImpl::Serialize() {
  bool did_serialization = false;

  Document* document =
      specified_web_local_frame_impl_->GetFrame()->GetDocument();
  const KURL& url =
      save_with_empty_url_ ? KURL("about:internet") : document->Url();

  if (url.IsValid()) {
    did_serialization = true;

    const WTF::TextEncoding& text_encoding =
        document->Encoding().IsValid() ? document->Encoding() : UTF8Encoding();
    if (text_encoding.IsNonByteBasedEncoding()) {
      const UChar kByteOrderMark = 0xFEFF;
      data_buffer_.Append(kByteOrderMark);
    }

    SerializeDomParam param(url, text_encoding, document);

    Element* document_element = document->documentElement();
    if (document_element)
      BuildContentForNode(document_element, &param);

    EncodeAndFlushBuffer(WebFrameSerializerClient::kCurrentFrameIsFinished,
                         &param, kForceFlush);
  } else {
    // Report empty contents for invalid URLs.
    client_->DidSerializeDataForFrame(
        WebVector<char>(), WebFrameSerializerClient::kCurrentFrameIsFinished);
  }

  DCHECK(data_buffer_.IsEmpty());
  return did_serialization;
}

}  // namespace blink
