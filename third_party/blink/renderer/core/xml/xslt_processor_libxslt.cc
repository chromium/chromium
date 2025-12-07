/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@webkit.org>
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

#include "third_party/blink/renderer/core/xml/xslt_processor.h"

#include <libxslt/imports.h>
#include <libxslt/security.h>
#include <libxslt/variables.h>
#include <libxslt/xsltutils.h>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/transform_source.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser_scope.h"
#include "third_party/blink/renderer/core/xml/xsl_style_sheet.h"
#include "third_party/blink/renderer/core/xml/xslt_extensions.h"
#include "third_party/blink/renderer/core/xml/xslt_unicode_sort.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

namespace {

constexpr int kDoubleXsltMaxVars = 30000;

}

namespace blink {

void XSLTProcessor::GenericErrorFunc(void*, const char*, ...) {
  // It would be nice to do something with this error message.
}

void XSLTProcessor::ParseErrorFunc(void* user_data, const xmlError* error) {
  FrameConsole* console = static_cast<FrameConsole*>(user_data);
  if (!console)
    return;

  mojom::ConsoleMessageLevel level;
  switch (error->level) {
    case XML_ERR_NONE:
      level = mojom::ConsoleMessageLevel::kVerbose;
      break;
    case XML_ERR_WARNING:
      level = mojom::ConsoleMessageLevel::kWarning;
      break;
    case XML_ERR_ERROR:
    case XML_ERR_FATAL:
    default:
      level = mojom::ConsoleMessageLevel::kError;
      break;
  }

  console->AddMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kXml, level, error->message,
      MakeGarbageCollected<SourceLocation>(error->file, String(), error->line,
                                           0, nullptr)));
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here,
// thus we have globals.
static XSLTProcessor* g_global_processor = nullptr;
static ResourceFetcher* g_global_resource_fetcher = nullptr;

static xmlDocPtr DocLoaderFunc(const xmlChar* uri,
                               xmlDictPtr,
                               int options,
                               void* ctxt,
                               xsltLoadType type) {
  if (!g_global_processor)
    return nullptr;

  switch (type) {
    case XSLT_LOAD_DOCUMENT: {
      XMLDocumentParserScope scope(
          g_global_processor->XslStylesheet()->OwnerDocument());

      xsltTransformContextPtr context = (xsltTransformContextPtr)ctxt;
      xmlChar* base = xmlNodeGetBase(context->document->doc, context->node);
      KURL url(KURL(reinterpret_cast<const char*>(base)),
               reinterpret_cast<const char*>(uri));
      xmlFree(base);

      ResourceLoaderOptions fetch_options(nullptr /* world */);
      fetch_options.initiator_info.name = fetch_initiator_type_names::kXml;
      FetchParameters params(ResourceRequest(url), fetch_options);
      params.MutableResourceRequest().SetMode(
          network::mojom::RequestMode::kSameOrigin);
      Resource* resource =
          RawResource::FetchSynchronously(params, g_global_resource_fetcher);
      if (!g_global_processor)
        return nullptr;
      scoped_refptr<const SharedBuffer> data = resource->ResourceBuffer();
      if (!data)
        return nullptr;

      FrameConsole* console = nullptr;
      LocalFrame* frame =
          g_global_processor->XslStylesheet()->OwnerDocument()->GetFrame();
      if (frame)
        console = &frame->Console();
      xmlSetStructuredErrorFunc(console, XSLTProcessor::ParseErrorFunc);
      xmlSetGenericErrorFunc(console, XSLTProcessor::GenericErrorFunc);

      xmlDocPtr doc = nullptr;

      // We don't specify an encoding here. Neither Gecko nor WinIE respects
      // the encoding specified in the HTTP headers.
      xmlParserCtxtPtr ctx = xmlCreatePushParserCtxt(
          nullptr, nullptr, nullptr, 0, reinterpret_cast<const char*>(uri));
      if (ctx && !xmlCtxtUseOptions(ctx, options)) {
        size_t offset = 0;
        for (const auto& span : *data) {
          bool final_chunk = offset + span.size() == data->size();
          // Stop parsing chunks if xmlParseChunk returns an error.
          if (xmlParseChunk(ctx, span.data(), static_cast<int>(span.size()),
                            final_chunk))
            break;
          offset += span.size();
        }

        if (ctx->wellFormed)
          doc = ctx->myDoc;
      }

      xmlFreeParserCtxt(ctx);
      xmlSetStructuredErrorFunc(nullptr, nullptr);
      xmlSetGenericErrorFunc(nullptr, nullptr);

      return doc;
    }
    case XSLT_LOAD_STYLESHEET:
      return g_global_processor->XslStylesheet()->LocateStylesheetSubResource(
          ((xsltStylesheetPtr)ctxt)->doc, uri);
    default:
      break;
  }

  return nullptr;
}

static inline void SetXSLTLoadCallBack(xsltDocLoaderFunc func,
                                       XSLTProcessor* processor,
                                       ResourceFetcher* fetcher) {
  xsltSetLoaderFunc(func);
  g_global_processor = processor;
  g_global_resource_fetcher = fetcher;
}

static int WriteToStringBuilder(void* context, const char* buffer, int len) {
  if (!len)
    return 0;

  // SAFETY: libxslt provides `len` bytes pointed to by `buffer`.
  auto source_buffer =
      UNSAFE_BUFFERS(base::span(buffer, base::checked_cast<size_t>(len)));

  StringBuffer<UChar> string_buffer(len);
  unicode::ConversionResult result = unicode::ConvertUtf8ToUtf16(
      base::as_bytes(source_buffer), string_buffer.Span());
  CHECK(result.status == unicode::kConversionOK ||
        result.status == unicode::kSourceExhausted);

  StringBuilder& result_output = *static_cast<StringBuilder*>(context);
  result_output.Append(result.converted);
  return base::checked_cast<int>(result.consumed);
}

static bool SaveResultToString(xmlDocPtr result_doc,
                               xsltStylesheetPtr sheet,
                               String& result_string) {
  xmlOutputBufferPtr output_buf = xmlAllocOutputBuffer(nullptr);
  if (!output_buf)
    return false;

  StringBuilder result_builder;
  output_buf->context = &result_builder;
  output_buf->writecallback = WriteToStringBuilder;

  int retval = xsltSaveResultTo(output_buf, result_doc, sheet);
  xmlOutputBufferClose(output_buf);
  if (retval < 0)
    return false;

  // Workaround for <http://bugzilla.gnome.org/show_bug.cgi?id=495668>:
  // libxslt appends an extra line feed to the result.
  if (result_builder.length() > 0 &&
      result_builder[result_builder.length() - 1] == '\n')
    result_builder.Resize(result_builder.length() - 1);

  result_string = result_builder.ToString();

  return true;
}

static char* AllocateParameterValue(const String& value) {
  StringUtf8Adaptor utf8(value);
  auto parameter_value = base::HeapArray<char>::Uninit(
      base::CheckAdd(utf8.size(), 1u).ValueOrDie());
  parameter_value.copy_prefix_from(base::span(utf8));
  parameter_value[utf8.size()] = '\0';
  return std::move(parameter_value).leak().data();
}

static Vector<char*> XsltParamArrayFromParameterMap(
    XSLTProcessor::ParameterMap& parameters) {
  if (parameters.empty())
    return {};

  base::CheckedNumeric<wtf_size_t> size = parameters.size();
  size *= 2;
  ++size;

  Vector<char*> parameter_array(size.ValueOrDie());

  unsigned index = 0;
  for (auto& parameter : parameters) {
    parameter_array[index++] = AllocateParameterValue(parameter.key);
    parameter_array[index++] = AllocateParameterValue(parameter.value);
  }
  parameter_array[index] = nullptr;
  return parameter_array;
}

static void FreeXsltParamArray(Vector<char*>& params) {
  if (params.empty()) {
    return;
  }

  for (auto& param : params) {
    base::HeapArray<char>::DeleteLeakedData(param);
  }
  params.clear();
}

static xsltStylesheetPtr XsltStylesheetPointer(
    Document* document,
    Member<XSLStyleSheet>& cached_stylesheet,
    Node* stylesheet_root_node) {
  if (!cached_stylesheet && stylesheet_root_node) {
    // When using importStylesheet, we will use the given document as the
    // imported stylesheet's owner.
    cached_stylesheet = MakeGarbageCollected<XSLStyleSheet>(
        stylesheet_root_node->parentNode()
            ? &stylesheet_root_node->parentNode()->GetDocument()
            : document,
        stylesheet_root_node,
        stylesheet_root_node->GetDocument().Url().GetString(),
        stylesheet_root_node->GetDocument().Url(),
        false);  // FIXME: Should we use baseURL here?

    // According to Mozilla documentation, the node must be a Document node,
    // an xsl:stylesheet or xsl:transform element. But we just use text
    // content regardless of node type.
    cached_stylesheet->ParseString(CreateMarkup(stylesheet_root_node));
  }

  if (!cached_stylesheet || !cached_stylesheet->GetDocument())
    return nullptr;

  return cached_stylesheet->CompileStyleSheet();
}

static inline xmlDocPtr XmlDocPtrFromNode(Node* source_node,
                                          bool& should_delete) {
  Document* owner_document = &source_node->GetDocument();
  bool source_is_document = (source_node == owner_document);

  xmlDocPtr source_doc = nullptr;
  if (source_is_document && owner_document->GetTransformSource())
    source_doc =
        (xmlDocPtr)owner_document->GetTransformSource()->PlatformSource();
  if (!source_doc) {
    source_doc = (xmlDocPtr)XmlDocPtrForString(
        owner_document, CreateMarkup(source_node),
        source_is_document ? owner_document->Url().GetString() : String());
    should_delete = source_doc;
  }
  return source_doc;
}

static inline String ResultMIMEType(xmlDocPtr result_doc,
                                    xsltStylesheetPtr sheet) {
  // There are three types of output we need to be able to deal with:
  // HTML (create an HTML document), XML (create an XML document),
  // and text (wrap in a <pre> and create an XML document).

  const xmlChar* result_type = nullptr;
  XSLT_GET_IMPORT_PTR(result_type, sheet, method);
  if (!result_type && result_doc->type == XML_HTML_DOCUMENT_NODE)
    result_type = (const xmlChar*)"html";

  if (xmlStrEqual(result_type, (const xmlChar*)"html"))
    return "text/html";
  if (xmlStrEqual(result_type, (const xmlChar*)"text"))
    return "text/plain";

  return "application/xml";
}

bool XSLTProcessor::TransformToString(Node* source_node,
                                      String& mime_type,
                                      String& result_string,
                                      String& result_encoding) {
  Document* owner_document = &source_node->GetDocument();

  SetXSLTLoadCallBack(DocLoaderFunc, this, owner_document->Fetcher());
  xsltStylesheetPtr sheet = XsltStylesheetPointer(document_.Get(), stylesheet_,
                                                  stylesheet_root_node_.Get());
  if (!sheet) {
    SetXSLTLoadCallBack(nullptr, nullptr, nullptr);
    stylesheet_ = nullptr;
    return false;
  }
  stylesheet_->ClearDocuments();

  xmlChar* orig_method = sheet->method;
  if (!orig_method && mime_type == "text/html")
    sheet->method = (xmlChar*)"html";

  bool success = false;
  bool should_free_source_doc = false;
  if (xmlDocPtr source_doc =
          XmlDocPtrFromNode(source_node, should_free_source_doc)) {
    // The XML declaration would prevent parsing the result as a fragment,
    // and it's not needed even for documents, as the result of this
    // function is always immediately parsed.
    sheet->omitXmlDeclaration = true;

    // Double the number of vars xslt uses internally before it is used in
    // xsltNewTransformContext. See http://crbug.com/796505
    DCHECK(xsltMaxVars == kDoubleXsltMaxVars ||
           xsltMaxVars == kDoubleXsltMaxVars / 2)
        << "We should be doubling xsltMaxVars' default value from libxslt with "
           "our new value. actual value: "
        << xsltMaxVars;
    xsltMaxVars = kDoubleXsltMaxVars;

    xsltTransformContextPtr transform_context =
        xsltNewTransformContext(sheet, source_doc);
    RegisterXSLTExtensions(transform_context);

    xsltSecurityPrefsPtr security_prefs = xsltNewSecurityPrefs();
    // Read permissions are checked by docLoaderFunc.
    CHECK_EQ(0, xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_WRITE_FILE,
                                     xsltSecurityForbid));
    CHECK_EQ(0,
             xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_CREATE_DIRECTORY,
                                  xsltSecurityForbid));
    CHECK_EQ(0, xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_WRITE_NETWORK,
                                     xsltSecurityForbid));
    CHECK_EQ(0, xsltSetCtxtSecurityPrefs(security_prefs, transform_context));

    // <http://bugs.webkit.org/show_bug.cgi?id=16077>: XSLT processor
    // <xsl:sort> algorithm only compares by code point.
    xsltSetCtxtSortFunc(transform_context, XsltUnicodeSortFunction);

    // This is a workaround for a bug in libxslt.
    // The bug has been fixed in version 1.1.13, so once we ship that this
    // can be removed.
    if (!transform_context->globalVars)
      transform_context->globalVars = xmlHashCreate(20);

    Vector<char*> params = XsltParamArrayFromParameterMap(parameters_);
    xsltQuoteUserParams(
        transform_context,
        static_cast<const char**>(static_cast<void*>(params.data())));
    xmlDocPtr result_doc = xsltApplyStylesheetUser(
        sheet, source_doc, nullptr, nullptr, nullptr, transform_context);

    xsltFreeTransformContext(transform_context);
    xsltFreeSecurityPrefs(security_prefs);
    FreeXsltParamArray(params);

    if (should_free_source_doc)
      xmlFreeDoc(source_doc);

    success = SaveResultToString(result_doc, sheet, result_string);
    if (success) {
      mime_type = ResultMIMEType(result_doc, sheet);
      result_encoding = (char*)result_doc->encoding;
    }
    xmlFreeDoc(result_doc);
  }

  sheet->method = orig_method;
  SetXSLTLoadCallBack(nullptr, nullptr, nullptr);
  xsltFreeStylesheet(sheet);
  stylesheet_ = nullptr;

  return success;
}

}  // namespace blink
