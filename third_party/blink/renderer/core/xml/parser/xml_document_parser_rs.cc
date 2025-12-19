/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2025 The Chromium Authors
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

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser_rs.h"

#include "base/notimplemented.h"
#include "base/strings/string_view_rust.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/throw_on_dynamic_markup_insertion_count_incrementer.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/xml/document_xml_tree_viewer.h"
#include "third_party/blink/renderer/core/xml/parser/xhtml_subset.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {
inline bool HasNoStyleInformation(Document* document) {
  if (document->SawElementsInKnownNamespaces()) {
    return false;
  }

  if (!document->GetFrame() || !document->GetFrame()->GetPage()) {
    return false;
  }

  if (!document->IsInMainFrame() ||
      document->GetFrame()->IsInFencedFrameTree()) {
    return false;  // This document has style information from a parent.
  }

  if (SVGImage::IsInSVGImage(document)) {
    return false;
  }

  return true;
}

String RustStrToWtfString(rust::Str str) {
  return String::FromUTF8(base::RustStrToStringView(str));
}

AtomicString RustStrToAtomicString(rust::Str str) {
  return AtomicString(RustStrToWtfString(str));
}

bool HandleNamespaceAttributes(
    Vector<Attribute, kAttributePrealloc>& prefixed_attributes,
    xml_ffi::NamespacesIterator& namespaces,
    bool& encountered_namespace_reset,
    ExceptionState& exception_state) {
  rust::String prefix;
  rust::String uri;

  while (namespaces_next(namespaces, prefix, uri)) {
    AtomicString namespace_q_name = g_xmlns_atom;
    if (prefix.length()) {
      namespace_q_name = AtomicString(
          StrCat({g_xmlns_with_colon, RustStrToWtfString(prefix)}));
    }
    std::optional<QualifiedName> parsed_name = Element::ParseAttributeName(
        xmlns_names::kNamespaceURI, namespace_q_name, exception_state);
    if (!parsed_name) {
      DCHECK(exception_state.HadException());
      return false;
    }
    if (parsed_name->LocalName() == g_xmlns_atom) {
      encountered_namespace_reset = uri.empty();
    }
    prefixed_attributes.push_back(
        Attribute(std::move(*parsed_name), RustStrToAtomicString(uri)));
  }
  return true;
}

bool CollectElementAttributes(
    Vector<Attribute, kAttributePrealloc>& prefixed_attributes,
    xml_ffi::AttributesIterator& attributes,
    ExceptionState& exception_state) {
  rust::String local_name;
  rust::String prefix;
  rust::String ns;
  rust::String value;
  while (attributes_next(attributes, local_name, ns, prefix, value)) {
    AtomicString attr_q_name =
        prefix.empty() ? RustStrToAtomicString(local_name)
                       : AtomicString(StrCat({RustStrToWtfString(prefix), ":",
                                              RustStrToWtfString(local_name)}));
    AtomicString attr_ns = RustStrToAtomicString(ns);
    std::optional<QualifiedName> parsed_name =
        Element::ParseAttributeName(attr_ns, attr_q_name, exception_state);
    if (!parsed_name) {
      DCHECK(exception_state.HadException());
      return false;
    }

    prefixed_attributes.push_back(
        Attribute(std::move(*parsed_name), RustStrToAtomicString(value)));
  }
  return true;
}

void SetAttributes(Element* element,
                   Vector<Attribute, kAttributePrealloc>& attribute_vector,
                   ParserContentPolicy parser_content_policy) {
  if (!ScriptingContentIsAllowed(parser_content_policy)) {
    element->StripScriptingAttributes(attribute_vector);
  }
  element->ParserSetAttributes(attribute_vector);
}

}  // namespace

// FIXME: HTMLConstructionSite has a limit of 512, should these match?
static const unsigned kMaxXMLTreeDepth = 5000;

XMLDocumentParserRs::XMLDocumentParserRs(Document& document,
                                         LocalFrameView* frame_view)
    : ScriptableDocumentParser(document),
      script_runner_(frame_view
                         ? MakeGarbageCollected<XMLParserScriptRunner>(this)
                         : nullptr),  // Don't execute scripts for
                                      // documents without frames.
      script_start_position_(TextPosition::BelowRangePosition()),
      xml_errors_(&document),
      document_(&document),
      current_node_(&document),
      read_state_(xml_ffi::create_read_state(*this)),
      parsing_fragment_(false) {
  CHECK(RuntimeEnabledFeatures::XMLParsingRustEnabled() ||
        RuntimeEnabledFeatures::XMLRustForNonXsltEnabled());
  // This is XML being used as a document resource.
  if (frame_view && IsA<XMLDocument>(document)) {
    UseCounter::Count(document, WebFeature::kXMLDocument);
  }
}

XMLDocumentParserRs::XMLDocumentParserRs(
    DocumentFragment* fragment,
    Element* parent_element,
    ParserContentPolicy parser_content_policy)
    : ScriptableDocumentParser(fragment->GetDocument(), parser_content_policy),
      script_runner_(nullptr),
      script_start_position_(TextPosition::BelowRangePosition()),
      xml_errors_(&fragment->GetDocument()),
      document_(&fragment->GetDocument()),
      current_node_(fragment),
      read_state_(xml_ffi::create_read_state(*this)),
      parsing_fragment_(true) {
  CHECK(RuntimeEnabledFeatures::XMLParsingRustEnabled() ||
        RuntimeEnabledFeatures::XMLRustForNonXsltEnabled());
  // Step 2 of
  // https://html.spec.whatwg.org/C/#xml-fragment-parsing-algorithm
  // The following code collects prefix-namespace mapping in scope on
  // |parent_element|.
  HeapVector<Member<Element>> elem_stack;
  for (; parent_element; parent_element = parent_element->parentElement()) {
    elem_stack.push_back(parent_element);
  }

  if (elem_stack.empty()) {
    return;
  }

  for (; !elem_stack.empty(); elem_stack.pop_back()) {
    Element* element = elem_stack.back();
    // According to https://dom.spec.whatwg.org/#locate-a-namespace, a namespace
    // from the element name should have higher priority. So we check xmlns
    // attributes first, then overwrite the map with the namespace of the
    // element name.
    AttributeCollection attributes = element->Attributes();
    for (auto& attribute : attributes) {
      if (attribute.LocalName() == g_xmlns_atom) {
        default_namespace_uri_ = attribute.Value();
      } else if (attribute.Prefix() == g_xmlns_atom) {
        prefix_to_namespace_map_.Set(attribute.LocalName(), attribute.Value());
      }
    }
    if (element->namespaceURI().IsNull()) {
      continue;
    }
    if (element->prefix().empty()) {
      default_namespace_uri_ = element->namespaceURI();
    } else {
      prefix_to_namespace_map_.Set(element->prefix(), element->namespaceURI());
    }
  }
}

void XMLDocumentParserRs::HandleError(XMLErrors::ErrorType type,
                                      const char* formatted_message,
                                      TextPosition position) {
  xml_errors_.HandleError(type, formatted_message, position);
  if (type != XMLErrors::kErrorTypeWarning) {
    saw_error_ = true;
  }
  if (type == XMLErrors::kErrorTypeFatal) {
    StopParsing();
  }
}

void XMLDocumentParserRs::Append(const String& xml_string) {
  xml_ffi::append_to_source(*read_state_,
                            base::StringViewToRustSlice(xml_string.Utf8()));
  ProcessEvents();
}

void XMLDocumentParserRs::ProcessEvents() {
  while (!parser_paused_ && !IsStopped() && !saw_end_document_ &&
         !xml_ffi::saw_error(*read_state_)) {
    xml_ffi::process_next_event(*read_state_);
  }
  if (xml_ffi::saw_error(*read_state_)) {
    rust::String error_message = "";
    TextPosition position = TextPosition::BelowRangePosition();
    uint64_t row = 0, column = 0;
    if (xml_ffi::try_get_error_details(*read_state_, error_message, row,
                                       column)) {
      position = TextPosition(
          OrdinalNumber::FromZeroBasedInt(static_cast<int>(row)),
          OrdinalNumber::FromZeroBasedInt(static_cast<int>(column)));
    }
    HandleError(XMLErrors::kErrorTypeFatal, error_message.c_str(), position);
  }
}

void XMLDocumentParserRs::ClearCurrentNodeStack() {
  current_node_ = nullptr;
  leaf_text_node_ = nullptr;
  ancestor_resetting_namespace_ = nullptr;

  if (current_node_stack_.size()) {  // Aborted parsing.
    current_node_stack_.clear();
  }
}

void XMLDocumentParserRs::Trace(Visitor* visitor) const {
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

void XMLDocumentParserRs::StartDocument(
    rust::Str version,
    rust::Str encoding_rust,
    xml_ffi::StandaloneInfo standalone_info) {
  if (standalone_info == xml_ffi::StandaloneInfo::kNoXmlDeclaration) {
    GetDocument()->SetHasXMLDeclaration(false);
    return;
  }

  String xml_version = RustStrToWtfString(version);
  // Comment in XmlDocumentParser:
  // Silently ignore XML version mismatch in the prologue.
  // https://www.w3.org/TR/xml/#sec-prolog-dtd note says:
  // "When an XML 1.0 processor encounters a document that specifies a 1.x
  // version number other than '1.0', it will process it as a 1.0 document. This
  // means that an XML 1.0 processor will accept 1.x documents provided they do
  // not use any non-1.0 features."
  // See also: Document::setXMLVersion which throws when XML version is != 1.0.
  // See also:
  // https://github.com/kornelski/xml-rs/blob/main/src/reader/parser/inside_declaration.rs#L81
  // Which sets 1.0 and 1.1 and ignores other versions.
  if (xml_version == "1.0") {
    GetDocument()->setXMLVersion(xml_version, ASSERT_NO_EXCEPTION);
  }
  if (standalone_info >= xml_ffi::StandaloneInfo::kStandaloneNo) {
    GetDocument()->setXMLStandalone(
        standalone_info == xml_ffi::StandaloneInfo::kStandaloneYes,
        ASSERT_NO_EXCEPTION);
  }
  String encoding = RustStrToWtfString(encoding_rust);
  if (encoding.length()) {
    GetDocument()->SetXMLEncoding(encoding);
  }

  GetDocument()->SetHasXMLDeclaration(true);
}

void XMLDocumentParserRs::ProcessingInstruction(rust::Str target,
                                                rust::Str data) {
  if (!UpdateLeafTextNode()) {
    return;
  }

  // ASSERT_NO_EXCEPTION here as we expect the XML parser to produce an error if
  // the processing instruction would have had an invalid target name or would
  // have contained the closing sequence '?>'.
  class ProcessingInstruction* pi =
      current_node_->GetDocument().createProcessingInstruction(
          RustStrToWtfString(target), RustStrToWtfString(data),
          ASSERT_NO_EXCEPTION);

  // Insertion needs to be done first to determine is_css_ in
  // ProcessingInstruction.
  current_node_->ParserAppendChild(pi);

  if (pi->IsCSS()) {
    saw_css_ = true;
    CheckIfBlockingStyleSheetAdded();
  }
}

void XMLDocumentParserRs::StartElementNs(
    rust::Str local_name,
    bool has_prefix,
    rust::Str prefix,
    bool has_ns,
    rust::Str ns,
    xml_ffi::AttributesIterator& attributes,
    xml_ffi::NamespacesIterator& namespaces) {
  if (IsStopped()) {
    return;
  }

  if (!UpdateLeafTextNode()) {
    return;
  }

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
  // In fragment parsing, adjust namespace URI. If the parser library reports an
  // empty NS url, resolve it against the initially preserved namespace
  // hierarchy that is built when creating an XMLDocumentParser with the
  // fragment-parsing constructor.
  const AtomicString prefix_a(RustStrToWtfString(prefix));
  const AtomicString local_a(RustStrToWtfString(local_name));
  AtomicString adjusted_ns_uri(has_ns ? RustStrToWtfString(ns) : g_null_atom);
  if (parsing_fragment_ && adjusted_ns_uri.IsNull()) {
    if (has_prefix) {
      auto it = prefix_to_namespace_map_.find(prefix_a);
      if (it != prefix_to_namespace_map_.end()) {
        adjusted_ns_uri = it->value;
      }
    } else {
      adjusted_ns_uri =
          encountered_namespace_reset || ancestor_resetting_namespace_
              ? g_null_atom
              : default_namespace_uri_;
    }
  }

  v8::Isolate* isolate = document_->GetAgent().isolate();
  v8::TryCatch try_catch(isolate);
  if (!CollectElementAttributes(prefixed_attributes, attributes,
                                parsing_fragment_
                                    ? PassThroughException(isolate)
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

  QualifiedName q_name(has_prefix ? prefix_a : g_null_atom, local_a,
                       adjusted_ns_uri);
  if (!prefix_a.empty() && adjusted_ns_uri.empty()) {
    q_name =
        QualifiedName(g_null_atom,
                      AtomicString(StrCat({RustStrToWtfString(prefix), ":",
                                           RustStrToWtfString(local_name)})),
                      g_null_atom);
  }

  // Ported from XmlDocumentParser:
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

  if (!new_element) {
    StopParsing();
    return;
  }

  SetAttributes(new_element, prefixed_attributes, GetParserContentPolicy());

  if (parsing_fragment_ && encountered_namespace_reset) {
    ancestor_resetting_namespace_ = new_element;
  }

  new_element->BeginParsingChildren();

  if (new_element->IsScriptElement()) {
    script_start_position_ = GetTextPosition();
  }

  current_node_->ParserAppendChild(new_element);

  // Event handlers may synchronously trigger removal of the
  // document and cancellation of this parser.
  if (IsStopped()) {
    return;
  }

  if (auto* template_element = DynamicTo<HTMLTemplateElement>(*new_element)) {
    PushCurrentNode(template_element->content());
  } else {
    PushCurrentNode(new_element);
  }

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

void XMLDocumentParserRs::EndElementNs(rust::Str local_name,
                                       rust::Str prefix,
                                       rust::Str ns) {
  if (!UpdateLeafTextNode()) {
    return;
  }

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
  if (IsWaitingForScripts()) {
    PauseParsing();
  }

  // JavaScript may have detached the parser
  if (!IsDetached()) {
    PopCurrentNode();
  }
}

void XMLDocumentParserRs::Characters(rust::Str characters) {
  CreateLeafTextNodeIfNeeded();
  buffered_text_.Append(RustStrToWtfString(characters));
}

void XMLDocumentParserRs::CData(rust::Str data) {
  if (!UpdateLeafTextNode()) {
    return;
  }

  current_node_->ParserAppendChild(CDATASection::Create(
      current_node_->GetDocument(), RustStrToWtfString(data)));
}

void XMLDocumentParserRs::Comment(rust::Str comment) {
  if (!UpdateLeafTextNode()) {
    return;
  }

  class Comment* comment_node = Comment::Create(current_node_->GetDocument(),
                                                RustStrToWtfString(comment));
  current_node_->ParserAppendChild(comment_node);
}

void XMLDocumentParserRs::DocType(rust::Str name_rs,
                                  rust::Str public_id_rs,
                                  rust::Str system_id_rs) {
  if (!UpdateLeafTextNode()) {
    return;
  }

  String name = RustStrToWtfString(name_rs);
  String public_id = RustStrToWtfString(public_id_rs);
  String system_id = RustStrToWtfString(system_id_rs);

  // https://html.spec.whatwg.org/C/#parsing-xhtml-documents:named-character-references
  if (MatchesXHTMLSubsetDTD(public_id)) {
    // TODO(https://crbug.com/441911594): It's unfortunate we have to define
    // all the entities statically again. Needs investigation if upstream
    // API could be changed to externalize entity parsing.
    xml_ffi::add_xhtml_entities(*read_state_);
  }

  if (GetDocument()) {
    GetDocument()->ParserAppendChild(MakeGarbageCollected<DocumentType>(
        GetDocument(), name, public_id, system_id));
  }
}

void XMLDocumentParserRs::EndDocument() {
  UpdateLeafTextNode();
  bool xml_viewer_mode =
      !saw_error_ && !saw_css_ && HasNoStyleInformation(GetDocument());
  if (xml_viewer_mode) {
    GetDocument()->SetIsViewSource(true);
    TransformDocumentToXMLTreeView(*GetDocument());
  }
  // The XML crate keeps sending EndDocument as a next event if you keep
  // querying. Change state here to break out of the ProcessEvents loop.
  saw_end_document_ = true;
}

void XMLDocumentParserRs::PushCurrentNode(ContainerNode* n) {
  DCHECK(n);
  DCHECK(current_node_);
  current_node_stack_.push_back(current_node_);
  current_node_ = n;
  if (current_node_stack_.size() > kMaxXMLTreeDepth) {
    HandleError(XMLErrors::kErrorTypeFatal, "Excessive node nesting.",
                GetTextPosition());
  }
}

void XMLDocumentParserRs::PopCurrentNode() {
  if (!current_node_) {
    return;
  }
  DCHECK(current_node_stack_.size());
  current_node_ = current_node_stack_.back();
  current_node_stack_.pop_back();
}

void XMLDocumentParserRs::EndInternal() {
  DCHECK(!parsing_fragment_);

  if (IsDetached()) {
    return;
  }

  if (parser_paused_) {
    return;
  }

  // StopParsing() calls InsertErrorMessageBlock() if there was a parsing
  // error. Avoid showing the error message block twice.
  if (saw_error_ && !IsStopped()) {
    // TODO(https://crbug.com/441911594): This might not be reachable and
    // we can potentially fold this into the ProcessEvents() function directly.
    InsertErrorMessageBlock();
    if (IsDetached()) {
      return;
    }
  } else {
    UpdateLeafTextNode();
  }

  if (IsParsing()) {
    PrepareToStopParsing();
  }
  GetDocument()->SetReadyState(Document::kInteractive);
  ClearCurrentNodeStack();
  GetDocument()->FinishedParsing();
}

void XMLDocumentParserRs::Finish() {
  // FIXME: We should DCHECK(!m_parserStopped) here, since it does not
  // makes sense to call any methods on DocumentParser once it's been stopped.
  // However, FrameLoader::stop calls DocumentParser::finish unconditionally.
  // FIXME carried over from XmlDocumentParser.

  Flush();
  if (IsDetached()) {
    return;
  }

  if (parser_paused_) {
    finish_called_ = true;
  } else {
    EndInternal();
  }
}

void XMLDocumentParserRs::CreateLeafTextNodeIfNeeded() {
  if (leaf_text_node_) {
    return;
  }

  DCHECK_EQ(buffered_text_.length(), 0u);
  leaf_text_node_ = Text::Create(current_node_->GetDocument(), "");
  current_node_->ParserAppendChild(leaf_text_node_.Get());
}

bool XMLDocumentParserRs::UpdateLeafTextNode() {
  if (IsStopped()) {
    return false;
  }

  if (!leaf_text_node_) {
    return true;
  }

  leaf_text_node_->ParserAppendData(buffered_text_.ToString());
  buffered_text_.Clear();
  leaf_text_node_ = nullptr;

  // Synchronous event handlers executed by appendData() might detach this
  // parser.
  // TODO(358407357): it's possible that no synchronous event handlers can run
  // here, so this could just be `return true`.
  // TODO carried over from non Rust XMLDocumentParser.
  return !IsStopped();
}

void XMLDocumentParserRs::Detach() {
  if (script_runner_) {
    script_runner_->Detach();
  }
  script_runner_ = nullptr;

  ClearCurrentNodeStack();
  ScriptableDocumentParser::Detach();
}

bool XMLDocumentParserRs::WellFormed() const {
  return !xml_ffi::saw_error(*read_state_);
}

void XMLDocumentParserRs::InsertErrorMessageBlock() {
  xml_errors_.InsertErrorMessageBlock();
}

bool XMLDocumentParserRs::IsWaitingForScripts() const {
  return script_runner_ && script_runner_->HasParserBlockingScript();
}

void XMLDocumentParserRs::DidAddPendingParserBlockingStylesheet() {
  added_pending_parser_blocking_stylesheet_ = true;
}

void XMLDocumentParserRs::DidLoadAllPendingParserBlockingStylesheets() {
  added_pending_parser_blocking_stylesheet_ = false;
  waiting_for_stylesheets_ = false;
}

void XMLDocumentParserRs::CheckIfBlockingStyleSheetAdded() {
  if (!added_pending_parser_blocking_stylesheet_) {
    return;
  }
  added_pending_parser_blocking_stylesheet_ = false;
  waiting_for_stylesheets_ = true;
  PauseParsing();
}

void XMLDocumentParserRs::ExecuteScriptsWaitingForResources() {
  if (!IsWaitingForScripts() && !waiting_for_stylesheets_ && parser_paused_ &&
      IsParsing()) {
    ResumeParsing();
  }
}

OrdinalNumber XMLDocumentParserRs::LineNumber() const {
  uint64_t last_event_row_ = 0;
  uint64_t last_event_col_ = 0;
  bool got_position = xml_ffi::try_get_last_event_position(
      *read_state_, last_event_row_, last_event_col_);
  if (got_position) {
    return OrdinalNumber::FromZeroBasedInt(static_cast<int>(last_event_row_));
  } else {
    return OrdinalNumber::BeforeFirst();
  }
}

OrdinalNumber XMLDocumentParserRs::ColumnNumber() const {
  uint64_t last_event_row_ = 0;
  uint64_t last_event_col_ = 0;
  bool got_position = xml_ffi::try_get_last_event_position(
      *read_state_, last_event_row_, last_event_col_);
  if (got_position) {
    return OrdinalNumber::FromZeroBasedInt(static_cast<int>(last_event_col_));
  } else {
    return OrdinalNumber::BeforeFirst();
  }
}

TextPosition XMLDocumentParserRs::GetTextPosition() const {
  uint64_t last_event_row_ = 0;
  uint64_t last_event_col_ = 0;
  bool got_position = xml_ffi::try_get_last_event_position(
      *read_state_, last_event_row_, last_event_col_);
  if (got_position) {
    return TextPosition(
        OrdinalNumber::FromZeroBasedInt(static_cast<int>(last_event_row_)),
        OrdinalNumber::FromZeroBasedInt(static_cast<int>(last_event_col_)));
  } else {
    return TextPosition(OrdinalNumber::BeforeFirst(),
                        OrdinalNumber::BeforeFirst());
  }
}

void XMLDocumentParserRs::StopParsing() {
  // See comment before InsertErrorMessageBlock() in XMLDocumentParser::end.
  if (saw_error_) {
    InsertErrorMessageBlock();
  }
  DocumentParser::StopParsing();
}

void XMLDocumentParserRs::NotifyScriptExecuted() {
  if (!IsDetached() && !requesting_script_) {
    ResumeParsing();
  }
}

void XMLDocumentParserRs::PauseParsing() {
  DCHECK(!IsDetached());
  if (!parsing_fragment_) {
    parser_paused_ = true;
  }
}

void XMLDocumentParserRs::ResumeParsing() {
  DCHECK(!IsDetached());
  DCHECK(parser_paused_);
  parser_paused_ = false;
  ProcessEvents();

  if (finish_called_) {
    EndInternal();
  }
}

HashMap<String, String> ParseAttributesRust(const String& attrs_string,
                                            bool& attrs_ok) {
  CHECK(RuntimeEnabledFeatures::XMLParsingRustEnabled() ||
        RuntimeEnabledFeatures::XMLRustForNonXsltEnabled());
  rust::Vec<xml_ffi::AttributeNameValue> attributes = xml_ffi::parse_attributes(
      base::StringViewToRustSlice(attrs_string.Utf8()), attrs_ok);

  HashMap<String, String> retValues;
  for (auto& attr : attributes) {
    retValues.Set(RustStrToWtfString(attr.q_name),
                  RustStrToWtfString(attr.value));
  }
  return retValues;
}

}  // namespace blink
