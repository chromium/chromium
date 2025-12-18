/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"

#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlversion.h>
#include <libxslt/xslt.h>

#include <algorithm>
#include <memory>
#include <type_traits>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/throw_on_dynamic_markup_insertion_count_incrementer.h"
#include "third_party/blink/renderer/core/dom/transform_source.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/image_loader.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/xml/document_xml_tree_viewer.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/core/xml/parser/shared_buffer_reader.h"
#include "third_party/blink/renderer/core/xml/parser/xhtml_subset.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser_scope.h"
#include "third_party/blink/renderer/core/xml/parser/xml_parser_input.h"
#include "third_party/blink/renderer/core/xml/xslt_processor.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// FIXME: HTMLConstructionSite has a limit of 512, should these match?
static const unsigned kMaxXMLTreeDepth = 5000;

static inline String ToString(base::span<const xmlChar> string) {
  return String::FromUTF8(string);
}

static inline String ToString(const xmlChar* string) {
  return String::FromUTF8(reinterpret_cast<const char*>(string));
}

static inline AtomicString ToAtomicString(base::span<const xmlChar> string) {
  return AtomicString::FromUTF8(string);
}

static inline AtomicString ToAtomicString(const xmlChar* string) {
  return AtomicString::FromUTF8(reinterpret_cast<const char*>(string));
}

static inline bool HasNoStyleInformation(Document* document) {
  if (document->SawElementsInKnownNamespaces() ||
      DocumentXSLT::HasTransformSourceDocument(*document))
    return false;

  if (!document->GetFrame() || !document->GetFrame()->GetPage())
    return false;

  if (!document->IsInMainFrame() || document->GetFrame()->IsInFencedFrameTree())
    return false;  // This document has style information from a parent.

  if (SVGImage::IsInSVGImage(document))
    return false;

  return true;
}

struct xmlSAX2Namespace {
  xmlChar* prefix;
  xmlChar* uri;

  void CloneTo(xmlSAX2Namespace& to_ns) const {
    to_ns.prefix = xmlStrdup(prefix);
    to_ns.uri = xmlStrdup(uri);
  }
  void Free() {
    xmlFree(prefix);
    xmlFree(uri);
  }
};
static_assert(std::is_trivial_v<xmlSAX2Namespace> &&
                  std::is_standard_layout_v<xmlSAX2Namespace>,
              "not castable");
static_assert(sizeof(xmlSAX2Namespace) == sizeof(xmlChar*) * 2);

struct xmlSAX2Attributes {
  xmlChar* localname;
  xmlChar* prefix;
  xmlChar* uri;
  xmlChar* value;
  xmlChar* end;

  base::span<const xmlChar> ValueSpan() const {
    // SAFETY: ValueLength() returns the distance between `end` and
    // `value`. libxml provides the attribute value as a sequence of xmlChars
    // that start at `value` and end at `end`.
    return UNSAFE_BUFFERS(base::span(value, ValueLength()));
  }

  size_t ValueLength() const { return static_cast<size_t>(end - value); }

  void CloneTo(xmlSAX2Attributes& to_attr) const {
    to_attr.localname = xmlStrdup(localname);
    to_attr.prefix = xmlStrdup(prefix);
    to_attr.uri = xmlStrdup(uri);

    const size_t value_length = ValueLength();
    to_attr.value = xmlStrndup(value, base::checked_cast<int>(value_length));
    // SAFETY: ValueLength() returns the distance between `end` and
    // `value`. libxml provides the attribute value as a sequence of xmlChars
    // that start at `value` and end at `end`.
    to_attr.end = UNSAFE_BUFFERS(to_attr.value + value_length);
  }
  void Free() {
    xmlFree(localname);
    xmlFree(prefix);
    xmlFree(uri);
    xmlFree(value);
  }
};
static_assert(std::is_trivial_v<xmlSAX2Attributes> &&
                  std::is_standard_layout_v<xmlSAX2Attributes>,
              "not castable");
static_assert(sizeof(xmlSAX2Attributes) == sizeof(xmlChar*) * 5);

class PendingStartElementNSCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingStartElementNSCallback(const AtomicString& local_name,
                                const AtomicString& prefix,
                                const AtomicString& uri,
                                base::span<const xmlSAX2Namespace> namespaces,
                                base::span<const xmlSAX2Attributes> attributes,
                                int defaulted_count,
                                TextPosition text_position)
      : PendingCallback(text_position),
        local_name_(local_name),
        prefix_(prefix),
        uri_(uri),
        defaulted_count_(defaulted_count) {
    namespaces_ = base::HeapArray<xmlSAX2Namespace>::Uninit(namespaces.size());
    for (size_t i = 0; i < namespaces.size(); ++i) {
      namespaces[i].CloneTo(namespaces_[i]);
    }
    attributes_ = base::HeapArray<xmlSAX2Attributes>::Uninit(attributes.size());
    for (size_t i = 0; i < attributes.size(); ++i) {
      attributes[i].CloneTo(attributes_[i]);
    }
  }

  ~PendingStartElementNSCallback() override {
    for (size_t i = 0; i < namespaces_.size(); ++i) {
      namespaces_[i].Free();
    }
    for (size_t i = 0; i < attributes_.size(); ++i) {
      attributes_[i].Free();
    }
  }

  void Call(XMLDocumentParser* parser) override {
    parser->StartElementNs(local_name_, prefix_, uri_, namespaces_, attributes_,
                           defaulted_count_);
  }

 private:
  AtomicString local_name_;
  AtomicString prefix_;
  AtomicString uri_;
  base::HeapArray<xmlSAX2Namespace> namespaces_;
  base::HeapArray<xmlSAX2Attributes> attributes_;
  int defaulted_count_;
};

class PendingEndElementNSCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  explicit PendingEndElementNSCallback(TextPosition script_start_position,
                                       TextPosition text_position)
      : PendingCallback(text_position),
        script_start_position_(script_start_position) {}

  void Call(XMLDocumentParser* parser) override {
    parser->SetScriptStartPosition(script_start_position_);
    parser->EndElementNs();
  }

 private:
  TextPosition script_start_position_;
};

class PendingCharactersCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingCharactersCallback(base::span<const xmlChar> chars,
                            TextPosition text_position)
      : PendingCallback(text_position),
        chars_(base::HeapArray<xmlChar>::CopiedFrom(chars)) {}

  void Call(XMLDocumentParser* parser) override { parser->Characters(chars_); }

 private:
  base::HeapArray<xmlChar> chars_;
};

class PendingProcessingInstructionCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingProcessingInstructionCallback(const String& target,
                                       const String& data,
                                       TextPosition text_position)
      : PendingCallback(text_position), target_(target), data_(data) {}

  void Call(XMLDocumentParser* parser) override {
    parser->GetProcessingInstruction(target_, data_);
  }

 private:
  String target_;
  String data_;
};

class PendingCDATABlockCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  explicit PendingCDATABlockCallback(const String& text,
                                     TextPosition text_position)
      : PendingCallback(text_position), text_(text) {}

  void Call(XMLDocumentParser* parser) override { parser->CdataBlock(text_); }

 private:
  String text_;
};

class PendingCommentCallback final : public XMLDocumentParser::PendingCallback {
 public:
  explicit PendingCommentCallback(const String& text,
                                  TextPosition text_position)
      : PendingCallback(text_position), text_(text) {}

  void Call(XMLDocumentParser* parser) override { parser->Comment(text_); }

 private:
  String text_;
};

class PendingInternalSubsetCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingInternalSubsetCallback(const String& name,
                                const String& external_id,
                                const String& system_id,
                                TextPosition text_position)
      : PendingCallback(text_position),
        name_(name),
        external_id_(external_id),
        system_id_(system_id) {}

  void Call(XMLDocumentParser* parser) override {
    parser->InternalSubset(name_, external_id_, system_id_);
  }

 private:
  String name_;
  String external_id_;
  String system_id_;
};

class PendingErrorCallback final : public XMLDocumentParser::PendingCallback {
 public:
  PendingErrorCallback(XMLErrors::ErrorType type,
                       const xmlChar* message,
                       TextPosition text_position)
      : PendingCallback(text_position),
        type_(type),
        message_(xmlStrdup(message)) {}

  ~PendingErrorCallback() override { xmlFree(message_); }

  void Call(XMLDocumentParser* parser) override {
    parser->HandleError(type_, reinterpret_cast<char*>(message_),
                        GetTextPosition());
  }

 private:
  XMLErrors::ErrorType type_;
  xmlChar* message_;
};

void XMLDocumentParser::PushCurrentNode(ContainerNode* n) {
  DCHECK(n);
  DCHECK(current_node_);
  current_node_stack_.push_back(current_node_);
  current_node_ = n;
  if (current_node_stack_.size() > kMaxXMLTreeDepth)
    HandleError(XMLErrors::kErrorTypeFatal, "Excessive node nesting.",
                GetTextPosition());
}

void XMLDocumentParser::PopCurrentNode() {
  if (!current_node_)
    return;
  DCHECK(current_node_stack_.size());
  current_node_ = current_node_stack_.back();
  current_node_stack_.pop_back();
}

void XMLDocumentParser::ClearCurrentNodeStack() {
  current_node_ = nullptr;
  leaf_text_node_ = nullptr;
  ancestor_resetting_namespace_ = nullptr;

  if (current_node_stack_.size()) {  // Aborted parsing.
    current_node_stack_.clear();
  }
}

void XMLDocumentParser::Append(const String& input_source) {
  const SegmentedString source(input_source);
  if (saw_xsl_transform_ || !saw_first_element_)
    original_source_for_transform_.Append(source);

  if (IsStopped() || saw_xsl_transform_)
    return;

  if (parser_paused_) {
    pending_src_.Append(source);
    return;
  }

  DoWrite(source.ToString());
}

void XMLDocumentParser::HandleError(XMLErrors::ErrorType type,
                                    const char* formatted_message,
                                    TextPosition position) {
  xml_errors_.HandleError(type, formatted_message, position);
  if (type != XMLErrors::kErrorTypeWarning)
    saw_error_ = true;
  if (type == XMLErrors::kErrorTypeFatal)
    StopParsing();
}

void XMLDocumentParser::CreateLeafTextNodeIfNeeded() {
  if (leaf_text_node_)
    return;

  DCHECK_EQ(buffered_text_.size(), 0u);
  leaf_text_node_ = Text::Create(current_node_->GetDocument(), "");
  current_node_->ParserAppendChild(leaf_text_node_.Get());
}

bool XMLDocumentParser::UpdateLeafTextNode() {
  if (IsStopped())
    return false;

  if (!leaf_text_node_)
    return true;

  leaf_text_node_->ParserAppendData(ToString(buffered_text_));
  buffered_text_.clear();
  leaf_text_node_ = nullptr;

  // Synchronous event handlers executed by appendData() might detach this
  // parser.
  // TODO(358407357): it's possible that no synchronous event handlers can run
  // here, so this could just be `return true`.
  return !IsStopped();
}

void XMLDocumentParser::Detach() {
  if (script_runner_)
    script_runner_->Detach();
  script_runner_ = nullptr;

  ClearCurrentNodeStack();
  ScriptableDocumentParser::Detach();
}

void XMLDocumentParser::end() {
  TRACE_EVENT0("blink", "XMLDocumentParser::end");
  // XMLDocumentParserLibxml2 will do bad things to the document if doEnd() is
  // called.  I don't believe XMLDocumentParserQt needs doEnd called in the
  // fragment case.
  DCHECK(!parsing_fragment_);

  DoEnd();

  // doEnd() call above can detach the parser and null out its document.
  // In that case, we just bail out.
  if (IsDetached())
    return;

  // doEnd() could process a script tag, thus pausing parsing.
  if (parser_paused_)
    return;

  // StopParsing() calls InsertErrorMessageBlock() if there was a parsing
  // error. Avoid showing the error message block twice.
  // TODO(crbug.com/898775): Rationalize this.
  if (saw_error_ && !IsStopped()) {
    InsertErrorMessageBlock();
    // InsertErrorMessageBlock() may detach the document
    if (IsDetached())
      return;
  } else {
    UpdateLeafTextNode();
  }

  if (IsParsing())
    PrepareToStopParsing();
  GetDocument()->SetReadyState(Document::kInteractive);
  ClearCurrentNodeStack();
  GetDocument()->FinishedParsing();
}

void XMLDocumentParser::Finish() {
  // FIXME: We should DCHECK(!m_parserStopped) here, since it does not
  // makes sense to call any methods on DocumentParser once it's been stopped.
  // However, FrameLoader::stop calls DocumentParser::finish unconditionally.

  Flush();
  if (IsDetached())
    return;

  if (parser_paused_)
    finish_called_ = true;
  else
    end();
}

void XMLDocumentParser::InsertErrorMessageBlock() {
  xml_errors_.InsertErrorMessageBlock();
}

bool XMLDocumentParser::IsWaitingForScripts() const {
  return script_runner_ && script_runner_->HasParserBlockingScript();
}

void XMLDocumentParser::PauseParsing() {
  if (!parsing_fragment_)
    parser_paused_ = true;
}

bool XMLDocumentParser::ParseDocumentFragment(
    const String& chunk,
    DocumentFragment* fragment,
    Element* context_element,
    ParserContentPolicy parser_content_policy,
    ExceptionState& exception_state) {
  // TODO(https://crbug.com/441911594): Add a
  // CHECK(!RuntimeEnabledFeatures::XMLParsingRustEnabled()) here when there is
  // a Rust implementation for this.

  if (!chunk.length())
    return true;

  // FIXME: We need to implement the HTML5 XML Fragment parsing algorithm:
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-xhtml-syntax.html#xml-fragment-parsing-algorithm
  // For now we have a hack for script/style innerHTML support:
  if (context_element &&
      (context_element->HasLocalName(html_names::kScriptTag.LocalName()) ||
       context_element->HasLocalName(html_names::kStyleTag.LocalName()))) {
    fragment->ParserAppendChild(fragment->GetDocument().createTextNode(chunk));
    return true;
  }

  TryRethrowScope rethrow_scope(fragment->GetDocument().GetAgent().isolate(),
                                exception_state);
  auto* parser = MakeGarbageCollected<XMLDocumentParser>(
      fragment, context_element, parser_content_policy);
  bool well_formed = parser->AppendFragmentSource(chunk);

  // Do not call finish(). Current finish() and doEnd() implementations touch
  // the main Document/loader and can cause crashes in the fragment case.

  // Allows ~DocumentParser to assert it was detached before destruction.
  parser->Detach();
  // appendFragmentSource()'s wellFormed is more permissive than wellFormed().
  return well_formed;
}

static int g_global_descriptor = 0;

static int MatchFunc(const char*) {
  // Any use of libxml in the renderer process must:
  //
  // - have a XMLDocumentParserScope on the stack so the various callbacks know
  //   which blink::Document they are interacting with.
  // - only occur on the main thread, since the current document is not stored
  //   in a TLS variable.
  //
  // These conditionals are enforced by a CHECK() rather than being used to
  // calculate the return value since this allows XML parsing to fail safe in
  // case these preconditions are violated.
  CHECK(XMLDocumentParserScope::current_document_ && IsMainThread());
  // Tell libxml to always use Blink's set of input callbacks.
  return 1;
}

static inline void SetAttributes(
    Element* element,
    Vector<Attribute, kAttributePrealloc>& attribute_vector,
    ParserContentPolicy parser_content_policy) {
  if (!ScriptingContentIsAllowed(parser_content_policy))
    element->StripScriptingAttributes(attribute_vector);
  element->ParserSetAttributes(attribute_vector);
}

static void SwitchEncoding(xmlParserCtxtPtr ctxt, bool is_8bit) {
  // Make sure we don't call xmlSwitchEncoding in an error state.
  if (ctxt->errNo != XML_ERR_OK) {
    return;
  }

  if (is_8bit) {
    xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_8859_1);
    return;
  }

  const UChar kBOM = 0xFEFF;
  const unsigned char bom_high_byte =
      *reinterpret_cast<const unsigned char*>(&kBOM);
  xmlSwitchEncoding(ctxt, bom_high_byte == 0xFF ? XML_CHAR_ENCODING_UTF16LE
                                                : XML_CHAR_ENCODING_UTF16BE);
}

static void ParseChunk(xmlParserCtxtPtr ctxt, const String& chunk) {
  // Reset the encoding for each chunk to reflect if it is Latin-1 or UTF-16.
  SwitchEncoding(ctxt, chunk.Is8Bit());
  auto byte_span = base::as_chars(chunk.RawByteSpan());
  xmlParseChunk(ctxt, byte_span.data(),
                base::checked_cast<int>(byte_span.size()), 0);
}

static void FinishParsing(xmlParserCtxtPtr ctxt) {
  xmlParseChunk(ctxt, nullptr, 0, 1);
}

#define xmlParseChunk \
#error "Use parseChunk instead to select the correct encoding."

static bool IsLibxmlDefaultCatalogFile(const String& url_string) {
  // On non-Windows platforms libxml with catalogs enabled asks for
  // this URL, the "XML_XML_DEFAULT_CATALOG", on initialization.
  if (url_string == "file:///etc/xml/catalog")
    return true;

  // On Windows, libxml with catalogs enabled computes a URL relative
  // to where its DLL resides.
  if (url_string.StartsWithIgnoringASCIICase("file:///") &&
      url_string.EndsWithIgnoringASCIICase("/etc/catalog"))
    return true;
  return false;
}

static bool ShouldAllowExternalLoad(const KURL& url) {
  String url_string = url.GetString();

  // libxml should not be configured with catalogs enabled, so it
  // should not be asking to load default catalogs.
  CHECK(!IsLibxmlDefaultCatalogFile(url));

  // The most common DTD. There isn't much point in hammering www.w3c.org by
  // requesting this URL for every XHTML document.
  if (url_string.StartsWithIgnoringASCIICase("http://www.w3.org/TR/xhtml"))
    return false;

  // Similarly, there isn't much point in requesting the SVG DTD.
  if (url_string.StartsWithIgnoringASCIICase("http://www.w3.org/Graphics/SVG"))
    return false;

  // The libxml doesn't give us a lot of context for deciding whether to allow
  // this request. In the worst case, this load could be for an external
  // entity and the resulting document could simply read the retrieved
  // content. If we had more context, we could potentially allow the parser to
  // load a DTD. As things stand, we take the conservative route and allow
  // same-origin requests only.
  auto* current_context =
      XMLDocumentParserScope::current_document_->GetExecutionContext();
  if (!current_context->GetSecurityOrigin()->CanRequest(url)) {
    // FIXME: This is copy/pasted. We should probably build console logging into
    // canRequest().
    if (!url.IsNull()) {
      String message = StrCat({"Unsafe attempt to load URL ",
                               url.ElidedString(), " from frame with URL ",
                               current_context->Url().ElidedString(),
                               ". Domains, protocols and ports must match.\n"});
      current_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError, message));
    }
    return false;
  }

  return true;
}

static void* OpenFunc(const char* uri) {
  Document* document = XMLDocumentParserScope::current_document_;
  DCHECK(document);
  CHECK(IsMainThread());

  KURL url(NullURL(), uri);

  // If the document has no ExecutionContext, it's detached. Detached documents
  // aren't allowed to fetch.
  if (!document->GetExecutionContext())
    return &g_global_descriptor;

  if (!ShouldAllowExternalLoad(url))
    return &g_global_descriptor;

  KURL final_url;
  scoped_refptr<const SharedBuffer> data;

  {
    XMLDocumentParserScope scope(nullptr);
    // FIXME: We should restore the original global error handler as well.
    ResourceLoaderOptions options(
        document->GetExecutionContext()->GetCurrentWorld());
    options.initiator_info.name = fetch_initiator_type_names::kXml;
    FetchParameters params(ResourceRequest(url), options);
    params.MutableResourceRequest().SetMode(
        network::mojom::RequestMode::kSameOrigin);
    Resource* resource =
        RawResource::FetchSynchronously(params, document->Fetcher());

    if (!AllowedByNosniff::MimeTypeAsXMLExternalEntity(
            document->GetExecutionContext(), resource->GetResponse())) {
      return &g_global_descriptor;
    }

    if (!resource->ErrorOccurred()) {
      data = resource->ResourceBuffer();
      final_url = resource->GetResponse().CurrentRequestUrl();
    }
  }

  // We have to check the URL again after the load to catch redirects.
  // See <https://bugs.webkit.org/show_bug.cgi?id=21963>.
  if (!ShouldAllowExternalLoad(final_url))
    return &g_global_descriptor;

  UseCounter::Count(XMLDocumentParserScope::current_document_,
                    WebFeature::kXMLExternalResourceLoad);

  return new SharedBufferReader(data);
}

static int ReadFunc(void* context, char* buffer, int len) {
  // Do 0-byte reads in case of a null descriptor
  if (context == &g_global_descriptor)
    return 0;

  SharedBufferReader* data = static_cast<SharedBufferReader*>(context);
  // SAFETY: libxml provides `buffer` that points to at least `len` bytes.
  auto buffer_span =
      UNSAFE_BUFFERS(base::span(buffer, base::checked_cast<size_t>(len)));
  return base::checked_cast<int>(data->ReadData(buffer_span));
}

static int WriteFunc(void*, const char*, int) {
  // Always just do 0-byte writes
  return 0;
}

static int CloseFunc(void* context) {
  if (context != &g_global_descriptor) {
    SharedBufferReader* data = static_cast<SharedBufferReader*>(context);
    delete data;
  }
  return 0;
}

static void ErrorFunc(void*, const char*, ...) {
  // FIXME: It would be nice to display error messages somewhere.
}

static void EnsureLibXMLInitialized() {
  static bool did_init = false;
  if (did_init)
    return;

  xmlInitParser();
  xmlRegisterInputCallbacks(MatchFunc, OpenFunc, ReadFunc, CloseFunc);
  xmlRegisterOutputCallbacks(MatchFunc, OpenFunc, WriteFunc, CloseFunc);
  did_init = true;
}

scoped_refptr<XMLParserContext> XMLParserContext::CreateStringParser(
    xmlSAXHandlerPtr handlers,
    void* user_data) {
  EnsureLibXMLInitialized();
  xmlParserCtxtPtr parser =
      xmlCreatePushParserCtxt(handlers, nullptr, nullptr, 0, nullptr);

  int32_t options = XML_PARSE_HUGE | XML_PARSE_NOENT;

  // See https://crbug.com/455813733: We choose to prevent network loads of
  // external entities and DTDs here, but not in xmlReadMemory of
  // XmlDocPtrForString and in XSLTStyleSheet::Parse in order not to overlap
  // with XSLT deprecation.
  if (RuntimeEnabledFeatures::XMLNoExternalEntitiesEnabled()) {
    options |= XML_PARSE_NO_XXE;
  }

  xmlCtxtUseOptions(parser, options);
  parser->_private = user_data;
  return base::AdoptRef(new XMLParserContext(parser));
}

// Chunk should be encoded in UTF-8
scoped_refptr<XMLParserContext> XMLParserContext::CreateMemoryParser(
    xmlSAXHandlerPtr handlers,
    void* user_data,
    const std::string& chunk) {
  EnsureLibXMLInitialized();

  // appendFragmentSource() checks that the length doesn't overflow an int.
  xmlParserCtxtPtr parser = xmlCreateMemoryParserCtxt(
      chunk.c_str(), base::checked_cast<int>(chunk.length()));

  if (!parser)
    return nullptr;

  // Copy the sax handler
  UNSAFE_TODO(memcpy(parser->sax, handlers, sizeof(xmlSAXHandler)));

  // Set parser options.
  // XML_PARSE_NODICT: default dictionary option.
  // XML_PARSE_NOENT: force entities substitutions.
  // XML_PARSE_HUGE: don't impose arbitrary limits on document size.
  int32_t options = XML_PARSE_NODICT | XML_PARSE_NOENT | XML_PARSE_HUGE;

  // See https://crbug.com/455813733: We choose to prevent network loads of
  // external entities and DTDs here, but not in xmlReadMemory of
  // XmlDocPtrForString and in XSLTStyleSheet::Parse in order not to overlap
  // with XSLT deprecation.
  if (RuntimeEnabledFeatures::XMLNoExternalEntitiesEnabled()) {
    options |= XML_PARSE_NO_XXE;
  }

  xmlCtxtUseOptions(parser, options);

  parser->_private = user_data;

  return base::AdoptRef(new XMLParserContext(parser));
}

// --------------------------------

bool XMLDocumentParser::SupportsXMLVersion(const String& version) {
  return version == "1.0";
}

XMLDocumentParser::XMLDocumentParser(Document& document,
                                     LocalFrameView* frame_view)
    : ScriptableDocumentParser(document),
      context_(nullptr),
      current_node_(&document),
      is_currently_parsing8_bit_chunk_(false),
      saw_error_(false),
      saw_css_(false),
      saw_xsl_transform_(false),
      saw_first_element_(false),
      is_xhtml_document_(false),
      parser_paused_(false),
      requesting_script_(false),
      finish_called_(false),
      xml_errors_(&document),
      document_(&document),
      script_runner_(frame_view
                         ? MakeGarbageCollected<XMLParserScriptRunner>(this)
                         : nullptr),  // Don't execute scripts for
                                      // documents without frames.
      script_start_position_(TextPosition::BelowRangePosition()),
      parsing_fragment_(false) {
  CHECK(!RuntimeEnabledFeatures::XMLParsingRustEnabled());
  // This is XML being used as a document resource.
  if (frame_view && IsA<XMLDocument>(document))
    UseCounter::Count(document, WebFeature::kXMLDocument);
}

XMLDocumentParser::XMLDocumentParser(DocumentFragment* fragment,
                                     Element* parent_element,
                                     ParserContentPolicy parser_content_policy)
    : ScriptableDocumentParser(fragment->GetDocument(), parser_content_policy),
      context_(nullptr),
      current_node_(fragment),
      is_currently_parsing8_bit_chunk_(false),
      saw_error_(false),
      saw_css_(false),
      saw_xsl_transform_(false),
      saw_first_element_(false),
      is_xhtml_document_(false),
      parser_paused_(false),
      requesting_script_(false),
      finish_called_(false),
      xml_errors_(&fragment->GetDocument()),
      document_(&fragment->GetDocument()),
      script_runner_(nullptr),  // Don't execute scripts for document fragments.
      script_start_position_(TextPosition::BelowRangePosition()),
      parsing_fragment_(true) {
  // TODO(https://crbug.com/441911594): Add a
  // CHECK(!RuntimeEnabledFeatures::XMLParsingRustEnabled()) here when there is
  // a Rust implementation for ParseDocumentFagment() and this is no longer
  // reached.

  // Step 2 of
  // https://html.spec.whatwg.org/C/#xml-fragment-parsing-algorithm
  // The following code collects prefix-namespace mapping in scope on
  // |parent_element|.
  HeapVector<Member<Element>> elem_stack;
  for (; parent_element; parent_element = parent_element->parentElement())
    elem_stack.push_back(parent_element);

  if (elem_stack.empty())
    return;

  for (; !elem_stack.empty(); elem_stack.pop_back()) {
    Element* element = elem_stack.back();
    // According to https://dom.spec.whatwg.org/#locate-a-namespace, a namespace
    // from the element name should have higher priority. So we check xmlns
    // attributes first, then overwrite the map with the namespace of the
    // element name.
    AttributeCollection attributes = element->Attributes();
    for (auto& attribute : attributes) {
      if (attribute.LocalName() == g_xmlns_atom)
        default_namespace_uri_ = attribute.Value();
      else if (attribute.Prefix() == g_xmlns_atom)
        prefix_to_namespace_map_.Set(attribute.LocalName(), attribute.Value());
    }
    if (element->namespaceURI().IsNull())
      continue;
    if (element->prefix().empty())
      default_namespace_uri_ = element->namespaceURI();
    else
      prefix_to_namespace_map_.Set(element->prefix(), element->namespaceURI());
  }
}

XMLParserContext::~XMLParserContext() {
  if (context_->myDoc)
    xmlFreeDoc(context_->myDoc);
  xmlFreeParserCtxt(context_);
}

XMLDocumentParser::~XMLDocumentParser() = default;

void XMLDocumentParser::Trace(Visitor* visitor) const {
  visitor->Trace(current_node_);
  visitor->Trace(current_node_stack_);
  visitor->Trace(ancestor_resetting_namespace_);
  visitor->Trace(leaf_text_node_);
  visitor->Trace(xml_errors_);
  visitor->Trace(document_);
  visitor->Trace(script_runner_);
  ScriptableDocumentParser::Trace(visitor);
  XMLParserScriptRunnerHost::Trace(visitor);
}

void XMLDocumentParser::DoWrite(const String& parse_string) {
  TRACE_EVENT0("blink", "XMLDocumentParser::doWrite");
  DCHECK(!IsDetached());
  if (!context_)
    InitializeParserContext();

  // Protect the libxml context from deletion during a callback
  scoped_refptr<XMLParserContext> context = context_;

  // libXML throws an error if you try to switch the encoding for an empty
  // string.
  if (parse_string.length()) {
    XMLDocumentParserScope scope(GetDocument());
    base::AutoReset<bool> encoding_scope(&is_currently_parsing8_bit_chunk_,
                                         parse_string.Is8Bit());
    ParseChunk(context->Context(), parse_string);

    // JavaScript (which may be run under the parseChunk callstack) may
    // cause the parser to be stopped or detached.
    if (IsStopped())
      return;
  }

  // FIXME: Why is this here? And why is it after we process the passed
  // source?
  if (GetDocument()->SawDecodingError()) {
    // If the decoder saw an error, report it as fatal (stops parsing)
    TextPosition position(
        OrdinalNumber::FromOneBasedInt(context->Context()->input->line),
        OrdinalNumber::FromOneBasedInt(context->Context()->input->col));
    HandleError(XMLErrors::kErrorTypeFatal, "Encoding error", position);
  }
}

static inline bool HandleNamespaceAttributes(
    Vector<Attribute, kAttributePrealloc>& prefixed_attributes,
    base::span<const xmlSAX2Namespace> namespaces,
    bool& encountered_namespace_reset,
    ExceptionState& exception_state) {
  for (const auto& ns : namespaces) {
    AtomicString namespace_q_name = g_xmlns_atom;
    AtomicString namespace_uri = ToAtomicString(ns.uri);
    if (ns.prefix) {
      namespace_q_name =
          AtomicString(StrCat({g_xmlns_with_colon, ToAtomicString(ns.prefix)}));
    }
    std::optional<QualifiedName> parsed_name = Element::ParseAttributeName(
        xmlns_names::kNamespaceURI, namespace_q_name, exception_state);
    if (!parsed_name) {
      DCHECK(exception_state.HadException());
      return false;
    }
    if (parsed_name->LocalName() == g_xmlns_atom) {
      encountered_namespace_reset = namespace_uri.empty();
    }
    prefixed_attributes.push_back(Attribute(*parsed_name, namespace_uri));
  }
  return true;
}

static inline bool HandleElementAttributes(
    Vector<Attribute, kAttributePrealloc>& prefixed_attributes,
    base::span<const xmlSAX2Attributes> attributes,
    const HashMap<AtomicString, AtomicString>& initial_prefix_to_namespace_map,
    ExceptionState& exception_state) {
  for (const auto& attr : attributes) {
    AtomicString attr_prefix = ToAtomicString(attr.prefix);
    AtomicString attr_uri;
    if (!attr_prefix.empty()) {
      // If provided, use the namespace URI from libxml2 because libxml2
      // updates its namespace table as it parses whereas the
      // initialPrefixToNamespaceMap is the initial map from namespace
      // prefixes to namespace URIs created by the XMLDocumentParser
      // constructor (in the case where we are parsing an XML fragment).
      if (attr.uri) {
        attr_uri = ToAtomicString(attr.uri);
      } else {
        const HashMap<AtomicString, AtomicString>::const_iterator it =
            initial_prefix_to_namespace_map.find(attr_prefix);
        if (it != initial_prefix_to_namespace_map.end()) {
          attr_uri = it->value;
        } else {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNamespaceError,
              StrCat({"Namespace prefix ", attr_prefix, " for attribute ",
                      ToString(attr.localname), " is not declared."}));
          return false;
        }
      }
    }
    AtomicString attr_q_name =
        attr_prefix.empty() ? ToAtomicString(attr.localname)
                            : AtomicString(StrCat({attr_prefix, ":",
                                                   ToString(attr.localname)}));

    std::optional<QualifiedName> parsed_name =
        Element::ParseAttributeName(attr_uri, attr_q_name, exception_state);
    if (!parsed_name) {
      return false;
    }
    prefixed_attributes.push_back(
        Attribute(std::move(*parsed_name), ToAtomicString(attr.ValueSpan())));
  }
  return true;
}

void XMLDocumentParser::StartElementNs(
    const AtomicString& local_name,
    const AtomicString& prefix,
    const AtomicString& uri,
    base::span<const xmlSAX2Namespace> namespaces,
    base::span<const xmlSAX2Attributes> attributes,
    int nb_defaulted) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    script_start_position_ = GetTextPosition();
    pending_callbacks_.push_back(
        std::make_unique<PendingStartElementNSCallback>(
            local_name, prefix, uri, namespaces, attributes, nb_defaulted,
            script_start_position_));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  bool is_first_element = !saw_first_element_;
  saw_first_element_ = true;

  Vector<Attribute, kAttributePrealloc> prefixed_attributes;
  bool encountered_namespace_reset = false;
  if (!HandleNamespaceAttributes(prefixed_attributes, namespaces,
                                 encountered_namespace_reset,
                                 IGNORE_EXCEPTION)) {
    StopParsing();
    return;
  }

  // Needed for fragment parsing. If the parser library reports an empty NS url,
  // resolve it against the initially preserved namespace hierarchy that is
  // built when creating an XMLDocumentParser with the fragment-parsing
  // constructor.
  AtomicString adjusted_uri = uri;
  if (parsing_fragment_ && adjusted_uri.IsNull()) {
    if (!prefix.IsNull()) {
      auto it = prefix_to_namespace_map_.find(prefix);
      if (it != prefix_to_namespace_map_.end())
        adjusted_uri = it->value;
    } else {
      adjusted_uri =
          encountered_namespace_reset || ancestor_resetting_namespace_
              ? g_null_atom
              : default_namespace_uri_;
    }
  }

  v8::Isolate* isolate = document_->GetAgent().isolate();
  v8::TryCatch try_catch(isolate);
  if (!HandleElementAttributes(prefixed_attributes, attributes,
                               prefix_to_namespace_map_,
                               parsing_fragment_ ? PassThroughException(isolate)
                                                 : IGNORE_EXCEPTION)) {
    StopParsing();
    if (parsing_fragment_) {
      DCHECK(try_catch.HasCaught());
      try_catch.ReThrow();
    }
    return;
  }

  AtomicString is;
  for (const auto& attr : prefixed_attributes) {
    if (attr.GetName() == html_names::kIsAttr) {
      is = attr.Value();
      break;
    }
  }

  QualifiedName q_name(prefix, local_name, adjusted_uri);
  if (!prefix.empty() && adjusted_uri.empty()) {
    q_name = QualifiedName(g_null_atom,
                           AtomicString(StrCat({prefix, ":", local_name})),
                           g_null_atom);
  }

  // If we are constructing a custom element, then we must run extra steps as
  // described in the HTML spec below. This is similar to the steps in
  // HTMLConstructionSite::CreateElement.
  // https://html.spec.whatwg.org/multipage/parsing.html#create-an-element-for-the-token
  // https://html.spec.whatwg.org/multipage/xhtml.html#parsing-xhtml-documents
  std::optional<CEReactionsScope> reactions;
  std::optional<ThrowOnDynamicMarkupInsertionCountIncrementer>
      throw_on_dynamic_markup_insertions;
  if (!parsing_fragment_) {
    if (HTMLConstructionSite::LookUpCustomElementDefinition(
            *document_, q_name, is, document_->customElementRegistry())) {
      throw_on_dynamic_markup_insertions.emplace(document_);
      document_->GetAgent().event_loop()->PerformMicrotaskCheckpoint();
      reactions.emplace(isolate);
    }
  }

  Element* new_element = current_node_->GetDocument().CreateElement(
      q_name,
      parsing_fragment_ ? CreateElementFlags::ByFragmentParser(document_)
                        : CreateElementFlags::ByParser(document_),
      is, /*registry*/ nullptr);
  // Check IsStopped() because custom element constructors may synchronously
  // trigger removal of the document and cancellation of this parser.
  if (IsStopped()) {
    return;
  }
  if (!new_element) {
    StopParsing();
    return;
  }

  SetAttributes(new_element, prefixed_attributes, GetParserContentPolicy());

  if (parsing_fragment_ && encountered_namespace_reset) {
    ancestor_resetting_namespace_ = new_element;
  }

  new_element->BeginParsingChildren();

  if (new_element->IsScriptElement())
    script_start_position_ = GetTextPosition();

  current_node_->ParserAppendChild(new_element);

  // Event handlers may synchronously trigger removal of the
  // document and cancellation of this parser.
  if (IsStopped()) {
    return;
  }

  if (auto* template_element = DynamicTo<HTMLTemplateElement>(*new_element))
    PushCurrentNode(template_element->content());
  else
    PushCurrentNode(new_element);

  // Note: |insertedByParser| will perform dispatching if this is an
  // HTMLHtmlElement.
  auto* html_html_element = DynamicTo<HTMLHtmlElement>(new_element);
  if (html_html_element && is_first_element) {
    html_html_element->InsertedByParser();
  } else if (!parsing_fragment_ && is_first_element &&
             GetDocument()->GetFrame()) {
    GetDocument()->GetFrame()->Loader().DispatchDocumentElementAvailable();
    GetDocument()->GetFrame()->Loader().RunScriptsAtDocumentElementAvailable();
    // runScriptsAtDocumentElementAvailable might have invalidated the document.
  }
}

void XMLDocumentParser::EndElementNs() {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(std::make_unique<PendingEndElementNSCallback>(
        script_start_position_, GetTextPosition()));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  ContainerNode* n = current_node_;
  auto* element = DynamicTo<Element>(n);
  if (!element) {
    PopCurrentNode();
    return;
  }

  if (ancestor_resetting_namespace_ == n) {
    ancestor_resetting_namespace_ = nullptr;
  }

  element->FinishParsingChildren();

  CheckIfBlockingStyleSheetAdded();

  if (element->IsScriptElement() &&
      !ScriptingContentIsAllowed(GetParserContentPolicy())) {
    PopCurrentNode();
    n->remove(IGNORE_EXCEPTION_FOR_TESTING);
    return;
  }

  if (!script_runner_) {
    PopCurrentNode();
    return;
  }

  // The element's parent may have already been removed from document.
  // Parsing continues in this case, but scripts aren't executed.
  if (!element->isConnected()) {
    PopCurrentNode();
    return;
  }

  if (element->IsScriptElement()) {
    requesting_script_ = true;
    script_runner_->ProcessScriptElement(*GetDocument(), element,
                                         script_start_position_);
    requesting_script_ = false;
  }

  // A parser-blocking script might be set and synchronously executed in
  // ProcessScriptElement() if the script was already ready, and in that case
  // IsWaitingForScripts() is false here.
  if (IsWaitingForScripts())
    PauseParsing();

  // JavaScript may have detached the parser
  if (!IsDetached())
    PopCurrentNode();
}

void XMLDocumentParser::NotifyScriptExecuted() {
  if (!IsDetached() && !requesting_script_)
    ResumeParsing();
}

void XMLDocumentParser::SetScriptStartPosition(TextPosition text_position) {
  script_start_position_ = text_position;
}

void XMLDocumentParser::Characters(base::span<const xmlChar> chars) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        std::make_unique<PendingCharactersCallback>(chars, GetTextPosition()));
    return;
  }

  CreateLeafTextNodeIfNeeded();
  buffered_text_.AppendSpan(chars);
}

void XMLDocumentParser::GetError(XMLErrors::ErrorType type,
                                 const char* message,
                                 va_list args) {
  if (IsStopped())
    return;

  char formatted_message[1024];
  UNSAFE_TODO(vsnprintf(formatted_message, sizeof(formatted_message) - 1,
                        message, args));

  if (parser_paused_) {
    pending_callbacks_.push_back(std::make_unique<PendingErrorCallback>(
        type, reinterpret_cast<const xmlChar*>(formatted_message),
        GetTextPosition()));
    return;
  }

  HandleError(type, formatted_message, GetTextPosition());
}

void XMLDocumentParser::GetProcessingInstruction(const String& target,
                                                 const String& data) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        std::make_unique<PendingProcessingInstructionCallback>(
            target, data, GetTextPosition()));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  // ### handle exceptions
  DummyExceptionStateForTesting exception_state;
  ProcessingInstruction* pi =
      current_node_->GetDocument().createProcessingInstruction(target, data,
                                                               exception_state);
  if (exception_state.HadException())
    return;

  current_node_->ParserAppendChild(pi);

  if (pi->IsCSS())
    saw_css_ = true;

  CheckIfBlockingStyleSheetAdded();

  saw_xsl_transform_ = !saw_first_element_ && pi->IsXSL();
  CHECK(!saw_xsl_transform_ || XSLTProcessor::XSLTEnabled());
  if (saw_xsl_transform_ &&
      !DocumentXSLT::HasTransformSourceDocument(*GetDocument())) {
    // This behavior is very tricky. We call stopParsing() here because we
    // want to stop processing the document until we're ready to apply the
    // transform, but we actually still want to be fed decoded string pieces
    // to accumulate in m_originalSourceForTransform. So, we call
    // stopParsing() here and check isStopped() in element callbacks.
    // FIXME: This contradicts the contract of DocumentParser.
    StopParsing();
  }
}

void XMLDocumentParser::CdataBlock(const String& text) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        std::make_unique<PendingCDATABlockCallback>(text, GetTextPosition()));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  current_node_->ParserAppendChild(
      CDATASection::Create(current_node_->GetDocument(), text));
}

void XMLDocumentParser::Comment(const String& text) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        std::make_unique<PendingCommentCallback>(text, GetTextPosition()));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  current_node_->ParserAppendChild(
      Comment::Create(current_node_->GetDocument(), text));
}

enum StandaloneInfo {
  kStandaloneUnspecified = -2,
  kNoXMlDeclaration,
  kStandaloneNo,
  kStandaloneYes
};

void XMLDocumentParser::StartDocument(const String& version,
                                      const String& encoding,
                                      int standalone) {
  StandaloneInfo standalone_info = static_cast<StandaloneInfo>(standalone);
  if (standalone_info == kNoXMlDeclaration) {
    GetDocument()->SetHasXMLDeclaration(false);
    return;
  }

  // Silently ignore XML version mismatch in the prologue.
  // https://www.w3.org/TR/xml/#sec-prolog-dtd note says:
  // "When an XML 1.0 processor encounters a document that specifies a 1.x
  // version number other than '1.0', it will process it as a 1.0 document. This
  // means that an XML 1.0 processor will accept 1.x documents provided they do
  // not use any non-1.0 features."
  if (!version.IsNull() && SupportsXMLVersion(version)) {
    GetDocument()->setXMLVersion(version, ASSERT_NO_EXCEPTION);
  }
  if (standalone != kStandaloneUnspecified)
    GetDocument()->setXMLStandalone(standalone_info == kStandaloneYes,
                                    ASSERT_NO_EXCEPTION);
  if (!encoding.IsNull())
    GetDocument()->SetXMLEncoding(encoding);
  GetDocument()->SetHasXMLDeclaration(true);
}

void XMLDocumentParser::EndDocument() {
  UpdateLeafTextNode();
}

void XMLDocumentParser::InternalSubset(const String& name,
                                       const String& external_id,
                                       const String& system_id) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        std::make_unique<PendingInternalSubsetCallback>(
            name, external_id, system_id, GetTextPosition()));
    return;
  }

  if (GetDocument()) {
    GetDocument()->ParserAppendChild(MakeGarbageCollected<DocumentType>(
        GetDocument(), name, external_id, system_id));
  }
}

static inline XMLDocumentParser* GetParser(void* closure) {
  xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
  return static_cast<XMLDocumentParser*>(ctxt->_private);
}

static void StartElementNsHandler(void* closure,
                                  const xmlChar* local_name,
                                  const xmlChar* prefix,
                                  const xmlChar* uri,
                                  int nb_namespaces,
                                  const xmlChar** libxml_namespaces,
                                  int nb_attributes,
                                  int nb_defaulted,
                                  const xmlChar** libxml_attributes) {
  // SAFETY: libxml provides `libxml_namespaces` which points to 2 const
  // xmlChar* for each 'nb_namespaces'. The xmlSAX2Namespace struct
  // encapsulates these two pointers.
  auto namespaces = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<const xmlSAX2Namespace*>(libxml_namespaces),
                 base::checked_cast<size_t>(nb_namespaces)));
  // SAFETY: libxml provides `libxml_attributes` which points to 5 const
  // xmlChar* for each 'nb_attributes' . The xmlSAX2Attributes struct
  // encapsulates these five pointers.
  auto attributes = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<const xmlSAX2Attributes*>(libxml_attributes),
                 base::checked_cast<size_t>(nb_attributes)));
  GetParser(closure)->StartElementNs(
      ToAtomicString(local_name), ToAtomicString(prefix), ToAtomicString(uri),
      namespaces, attributes, nb_defaulted);
}

static void EndElementNsHandler(void* closure,
                                const xmlChar*,
                                const xmlChar*,
                                const xmlChar*) {
  GetParser(closure)->EndElementNs();
}

static void CharactersHandler(void* closure, const xmlChar* chars, int length) {
  // SAFETY: libxml provides `chars` that point at `length` xmlChars.
  auto chars_span =
      UNSAFE_BUFFERS(base::span(chars, base::checked_cast<size_t>(length)));
  GetParser(closure)->Characters(chars_span);
}

static void ProcessingInstructionHandler(void* closure,
                                         const xmlChar* target,
                                         const xmlChar* data) {
  GetParser(closure)->GetProcessingInstruction(ToString(target),
                                               ToString(data));
}

static void CdataBlockHandler(void* closure, const xmlChar* text, int length) {
  // SAFETY: libxml provides `text` that point at `length` xmlChars.
  auto text_span =
      UNSAFE_BUFFERS(base::span(text, base::checked_cast<size_t>(length)));
  GetParser(closure)->CdataBlock(ToString(text_span));
}

static void CommentHandler(void* closure, const xmlChar* text) {
  GetParser(closure)->Comment(ToString(text));
}

PRINTF_FORMAT(2, 3)
static void WarningHandler(void* closure, const char* message, ...) {
  va_list args;
  va_start(args, message);
  GetParser(closure)->GetError(XMLErrors::kErrorTypeWarning, message, args);
  va_end(args);
}

PRINTF_FORMAT(2, 3)
static void NormalErrorHandler(void* closure, const char* message, ...) {
  va_list args;
  va_start(args, message);
  GetParser(closure)->GetError(XMLErrors::kErrorTypeNonFatal, message, args);
  va_end(args);
}

// Using a static entity and marking it XML_INTERNAL_PREDEFINED_ENTITY is a hack
// to avoid malloc/free. Using a global variable like this could cause trouble
// if libxml implementation details were to change
// TODO(https://crbug.com/344484975): The XML_INTERNAL_PREDEFINED_ENTITY is in
// fact overridden in GetXHTMLEntity() below for all uses, so it's not
// behaving as documented.
static xmlChar g_shared_xhtml_entity_result[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

static xmlEntityPtr SharedXHTMLEntity() {
  static xmlEntity entity;
  if (!entity.type) {
    entity.type = XML_ENTITY_DECL;
    entity.orig = g_shared_xhtml_entity_result;
    entity.content = g_shared_xhtml_entity_result;
    // TODO(https://crbug.com/344484975): The XML_INTERNAL_PREDEFINED_ENTITY
    // is in fact overridden in GetXHTMLEntity() below for all uses, so it's
    // not behaving as documented.  We should only set the value in one place.
    entity.etype = XML_INTERNAL_PREDEFINED_ENTITY;
  }
  return &entity;
}

template <size_t N>
static base::span<const char, N - 1> CopyToEntityBuffer(
    base::span<const char, N> expanded_entity_chars) {
  auto entity_buffer =
      base::as_writable_chars(base::span(g_shared_xhtml_entity_result));
  entity_buffer.template first<N>().copy_from(expanded_entity_chars);
  return entity_buffer.template first<N - 1>();
}

static base::span<const char> ConvertUTF16EntityToUTF8(
    const DecodedHTMLEntity& entity) {
  auto utf16_entity = base::span(entity.data).first(entity.length);
  auto entity_buffer =
      base::as_writable_bytes(base::span(g_shared_xhtml_entity_result));
  unicode::ConversionResult conversion_result =
      unicode::ConvertUtf16ToUtf8(utf16_entity, entity_buffer);
  if (conversion_result.status != unicode::kConversionOK) {
    return {};
  }

  DCHECK(!conversion_result.converted.empty());
  // Even though we must pass the length, libxml expects the entity string to be
  // null terminated.
  entity_buffer[conversion_result.converted.size()] = '\0';
  return base::as_chars(conversion_result.converted);
}

static xmlEntityPtr GetXHTMLEntity(const xmlChar* name) {
  std::optional<DecodedHTMLEntity> decoded_entity =
      DecodeNamedEntity(reinterpret_cast<const char*>(name));
  if (!decoded_entity) {
    return nullptr;
  }

  base::span<const char> entity_utf8;

  // Unlike the HTML parser, the XML parser parses the content of named
  // entities. So we need to escape '&' and '<'.
  if (decoded_entity->length == 1 && decoded_entity->data[0] == '&') {
    entity_utf8 = CopyToEntityBuffer(base::span_with_nul_from_cstring("&#38;"));
  } else if (decoded_entity->length == 1 && decoded_entity->data[0] == '<') {
    entity_utf8 = CopyToEntityBuffer(base::span_with_nul_from_cstring("&#60;"));
  } else if (decoded_entity->length == 2 && decoded_entity->data[0] == '<' &&
             decoded_entity->data[1] == 0x20D2) {
    entity_utf8 = CopyToEntityBuffer(
        base::span_with_nul_from_cstring("&#60;\xE2\x83\x92"));
  } else {
    entity_utf8 = ConvertUTF16EntityToUTF8(*decoded_entity);
    if (entity_utf8.empty()) {
      return nullptr;
    }
  }

  xmlEntityPtr entity = SharedXHTMLEntity();
  entity->length = static_cast<int>(entity_utf8.size());
  entity->name = name;
  return entity;
}

static xmlEntityPtr GetEntityHandler(void* closure, const xmlChar* name) {
  xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
  xmlEntityPtr ent = xmlGetPredefinedEntity(name);
  if (ent) {
    CHECK_EQ(ent->etype, XML_INTERNAL_PREDEFINED_ENTITY);
    return ent;
  }

  ent = xmlGetDocEntity(ctxt->myDoc, name);

  if (ent && ent->etype == XML_EXTERNAL_GENERAL_PARSED_ENTITY) {
    GetParser(closure)->DidSeeExternalEntity();
  }

  if (!ent && GetParser(closure)->IsXHTMLDocument()) {
    ent = GetXHTMLEntity(name);
    if (ent) {
      // TODO(https://crbug.com/344484975): This overrides the
      // XML_INTERNAL_PREDEFINED_ENTITY value set above for every single case.
      // We should figure out which one is correct and only set it to one,
      // rather than assigning one value and then always overriding it.
      ent->etype = XML_INTERNAL_GENERAL_ENTITY;
    }
  }

  return ent;
}

static void StartDocumentHandler(void* closure) {
  xmlParserCtxt* ctxt = static_cast<xmlParserCtxt*>(closure);
  XMLDocumentParser* parser = GetParser(closure);
  // Reset the encoding back to match that of the current data block (Latin-1 /
  // UTF-16), since libxml may switch encoding based on the XML declaration -
  // which it has now seen - causing the parse to fail. We could use the
  // XML_PARSE_IGNORE_ENC option to avoid this, but we're relying on populating
  // the 'xmlEncoding' property with the value it yields.
  SwitchEncoding(ctxt, parser->IsCurrentlyParsing8BitChunk());
  parser->StartDocument(ToString(ctxt->version), ToString(ctxt->encoding),
                        ctxt->standalone);
  xmlSAX2StartDocument(closure);
}

static void EndDocumentHandler(void* closure) {
  GetParser(closure)->EndDocument();
  xmlSAX2EndDocument(closure);
}

static void InternalSubsetHandler(void* closure,
                                  const xmlChar* name,
                                  const xmlChar* external_id,
                                  const xmlChar* system_id) {
  GetParser(closure)->InternalSubset(ToString(name), ToString(external_id),
                                     ToString(system_id));
  xmlSAX2InternalSubset(closure, name, external_id, system_id);
}

static void ExternalSubsetHandler(void* closure,
                                  const xmlChar*,
                                  const xmlChar* external_id,
                                  const xmlChar*) {
  // https://html.spec.whatwg.org/C/#parsing-xhtml-documents:named-character-references
  String ext_id = ToString(external_id);
  // Controls if we replace entities or not.
  if (MatchesXHTMLSubsetDTD(ext_id)) {
    GetParser(closure)->SetIsXHTMLDocument(true);
  }
}

static void IgnorableWhitespaceHandler(void*, const xmlChar*, int) {
  // Nothing to do, but we need this to work around a crasher.
  // http://bugzilla.gnome.org/show_bug.cgi?id=172255
  // http://bugs.webkit.org/show_bug.cgi?id=5792
}

void XMLDocumentParser::InitializeParserContext(const std::string& chunk) {
  xmlSAXHandler sax = {};

  // According to http://xmlsoft.org/html/libxml-tree.html#xmlSAXHandler and
  // http://xmlsoft.org/html/libxml-parser.html#fatalErrorSAXFunc the SAX
  // fatalError callback is unused; error gets all the errors. Use
  // normalErrorHandler for both the error and fatalError callbacks.
  sax.error = NormalErrorHandler;
  sax.fatalError = NormalErrorHandler;
  sax.characters = CharactersHandler;
  sax.processingInstruction = ProcessingInstructionHandler;
  sax.cdataBlock = CdataBlockHandler;
  sax.comment = CommentHandler;
  sax.warning = WarningHandler;
  sax.startElementNs = StartElementNsHandler;
  sax.endElementNs = EndElementNsHandler;
  sax.getEntity = GetEntityHandler;
  sax.startDocument = StartDocumentHandler;
  sax.endDocument = EndDocumentHandler;
  sax.internalSubset = InternalSubsetHandler;
  sax.externalSubset = ExternalSubsetHandler;
  sax.ignorableWhitespace = IgnorableWhitespaceHandler;
  sax.entityDecl = xmlSAX2EntityDecl;
  sax.initialized = XML_SAX2_MAGIC;
  saw_error_ = false;
  saw_css_ = false;
  saw_xsl_transform_ = false;
  saw_first_element_ = false;

  XMLDocumentParserScope scope(GetDocument());
  if (parsing_fragment_) {
    context_ = XMLParserContext::CreateMemoryParser(&sax, this, chunk);
  } else {
    context_ = XMLParserContext::CreateStringParser(&sax, this);
  }
}

void XMLDocumentParser::DoEnd() {
  if (!IsStopped()) {
    if (context_) {
      // Tell libxml we're done.
      {
        XMLDocumentParserScope scope(GetDocument());
        FinishParsing(Context());
      }

      context_ = nullptr;
    }
  }

  auto* window = GetDocument()->domWindow();

  // Don't issue the warning when we have moved from deprecation to removal.
  if (!RuntimeEnabledFeatures::XMLNoExternalEntitiesEnabled() && !saw_error_ &&
      !saw_xsl_transform_ && saw_external_entity_ && window) {
    GetDocument()->CountDeprecation(WebFeature::kXMLExternalResourceLoadEntitiesOnly);

    // The previous line counts this as a deprecation, but add an
    // explicit message here, due to crbug.com/40069336.
    window->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            ConsoleMessage::Source::kDeprecation,
            ConsoleMessage::Level::kWarning,
            "Externally loaded entities in XML parsing have been deprecated "
            "and will be removed from this browser soon. See "
            "https://chromestatus.com/feature/6734457763659776."),
        /*discard_duplicates=*/true);
  }

  bool xml_viewer_mode = !saw_error_ && !saw_css_ && !saw_xsl_transform_ &&
                         HasNoStyleInformation(GetDocument());
  if (xml_viewer_mode) {
    GetDocument()->SetIsViewSource(true);
    TransformDocumentToXMLTreeView(*GetDocument());
  } else if (saw_xsl_transform_) {
    xmlDocPtr doc = XmlDocPtrForString(
        GetDocument(), original_source_for_transform_.ToString(),
        GetDocument()->Url().GetString());
    GetDocument()->SetTransformSource(std::make_unique<TransformSource>(doc));
    DocumentParser::StopParsing();
  }
}

xmlDocPtr XmlDocPtrForString(Document* document,
                             const String& source,
                             const String& url) {
  if (source.empty())
    return nullptr;

  // In situations where the XMLDocumentParserRs is used as the primary parser,
  // this might be the first call into libxml2.
  EnsureLibXMLInitialized();

  // Parse in a single chunk into an xmlDocPtr
  // FIXME: Hook up error handlers so that a failure to parse the main
  // document results in good error messages.
  XMLDocumentParserScope scope(document, ErrorFunc, nullptr);
  XMLParserInput input(source);
  return xmlReadMemory(input.Data(), input.size(), url.Latin1().c_str(),
                       input.Encoding(), XSLT_PARSE_OPTIONS | XML_PARSE_HUGE);
}

OrdinalNumber XMLDocumentParser::LineNumber() const {
  if (callback_)
    return callback_->LineNumber();
  return OrdinalNumber::FromOneBasedInt(Context() ? Context()->input->line : 1);
}

OrdinalNumber XMLDocumentParser::ColumnNumber() const {
  if (callback_)
    return callback_->ColumnNumber();
  return OrdinalNumber::FromOneBasedInt(Context() ? Context()->input->col : 1);
}

TextPosition XMLDocumentParser::GetTextPosition() const {
  return TextPosition(LineNumber(), ColumnNumber());
}

void XMLDocumentParser::StopParsing() {
  // See comment before InsertErrorMessageBlock() in XMLDocumentParser::end.
  if (saw_error_)
    InsertErrorMessageBlock();
  DocumentParser::StopParsing();
  if (Context())
    xmlStopParser(Context());
}

void XMLDocumentParser::ResumeParsing() {
  DCHECK(!IsDetached());
  DCHECK(parser_paused_);

  parser_paused_ = false;

  // First, execute any pending callbacks
  while (!pending_callbacks_.empty()) {
    callback_ = pending_callbacks_.TakeFirst();
    callback_->Call(this);

    // A callback paused the parser
    if (parser_paused_) {
      callback_.reset();
      return;
    }
  }
  callback_.reset();

  // Then, write any pending data
  SegmentedString rest = pending_src_;
  pending_src_.Clear();
  // There is normally only one string left, so toString() shouldn't copy.
  // In any case, the XML parser runs on the main thread and it's OK if
  // the passed string has more than one reference.
  Append(rest.ToString().Impl());

  if (IsDetached())
    return;

  // Finally, if finish() has been called and write() didn't result
  // in any further callbacks being queued, call end()
  if (finish_called_ && pending_callbacks_.empty())
    end();
}

bool XMLDocumentParser::AppendFragmentSource(const String& chunk) {
  DCHECK(!context_);
  DCHECK(parsing_fragment_);

  std::string chunk_as_utf8 = chunk.Utf8();

  // libxml2 takes an int for a length, and therefore can't handle XML chunks
  // larger than 2 GiB.
  if (chunk_as_utf8.length() > INT_MAX)
    return false;

  TRACE_EVENT0("blink", "XMLDocumentParser::appendFragmentSource");
  InitializeParserContext(chunk_as_utf8);
  xmlParseContent(Context());
  EndDocument();  // Close any open text nodes.

  // No error if the chunk is well formed or it is not but we have no error.
  return Context()->wellFormed || !xmlCtxtGetLastError(Context());
}

void XMLDocumentParser::DidAddPendingParserBlockingStylesheet() {
  if (!context_)
    return;
  added_pending_parser_blocking_stylesheet_ = true;
}

void XMLDocumentParser::DidLoadAllPendingParserBlockingStylesheets() {
  added_pending_parser_blocking_stylesheet_ = false;
  waiting_for_stylesheets_ = false;
}

void XMLDocumentParser::CheckIfBlockingStyleSheetAdded() {
  if (!added_pending_parser_blocking_stylesheet_)
    return;
  added_pending_parser_blocking_stylesheet_ = false;
  waiting_for_stylesheets_ = true;
  PauseParsing();
}

void XMLDocumentParser::ExecuteScriptsWaitingForResources() {
  if (!IsWaitingForScripts() && !waiting_for_stylesheets_ && parser_paused_ &&
      IsParsing()) {
    ResumeParsing();
  }
}

// --------------------------------

struct AttributeParseState {
  HashMap<String, String> attributes;
  bool got_attributes;
};

static void AttributesStartElementNsHandler(void* closure,
                                            const xmlChar* xml_local_name,
                                            const xmlChar* /*xmlPrefix*/,
                                            const xmlChar* /*xmlURI*/,
                                            int /*nbNamespaces*/,
                                            const xmlChar** /*namespaces*/,
                                            int nb_attributes,
                                            int /*nbDefaulted*/,
                                            const xmlChar** libxml_attributes) {
  if (UNSAFE_TODO(strcmp(reinterpret_cast<const char*>(xml_local_name),
                         "attrs")) != 0) {
    return;
  }

  xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
  AttributeParseState* state =
      static_cast<AttributeParseState*>(ctxt->_private);

  state->got_attributes = true;

  // SAFETY: libxml provides `libxml_attributes` which points to 5 const
  // xmlChar* for each 'nb_attributes' . The xmlSAX2Attributes struct
  // encapsulates these five pointers.
  auto attributes = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<const xmlSAX2Attributes*>(libxml_attributes),
                 base::checked_cast<size_t>(nb_attributes)));
  for (const auto& attr : attributes) {
    String attr_local_name = ToString(attr.localname);
    String attr_prefix = ToString(attr.prefix);
    String attr_q_name = attr_prefix.empty()
                             ? attr_local_name
                             : StrCat({attr_prefix, ":", attr_local_name});

    state->attributes.Set(attr_q_name, ToString(attr.ValueSpan()));
  }
}

HashMap<String, String> ParseAttributes(const String& string, bool& attrs_ok) {
  CHECK(!RuntimeEnabledFeatures::XMLParsingRustEnabled());
  AttributeParseState state;
  state.got_attributes = false;

  xmlSAXHandler sax = {};
  sax.startElementNs = AttributesStartElementNsHandler;
  sax.initialized = XML_SAX2_MAGIC;
  scoped_refptr<XMLParserContext> parser =
      XMLParserContext::CreateStringParser(&sax, &state);
  String parse_string =
      StrCat({"<?xml version=\"1.0\"?><attrs ", string, " />"});
  ParseChunk(parser->Context(), parse_string);
  FinishParsing(parser->Context());
  attrs_ok = state.got_attributes;
  return state.attributes;
}

#undef xmlParseChunk

}  // namespace blink
