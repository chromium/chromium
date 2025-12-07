// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_XML_PARSER_XML_DOCUMENT_PARSER_RS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_XML_PARSER_XML_DOCUMENT_PARSER_RS_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/script/xml_parser_script_runner.h"
#include "third_party/blink/renderer/core/script/xml_parser_script_runner_host.h"
#include "third_party/blink/renderer/core/xml/parser/xml_errors.h"
#include "third_party/blink/renderer/core/xml/parser/xml_ffi.rs.h"
#include "third_party/blink/renderer/core/xml/parser/xml_ffi_callbacks.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class ContainerNode;
class Document;
class DocumentFragment;
class Element;
class LocalFrameView;
class Text;

class XMLDocumentParserRs final : public ScriptableDocumentParser,
                                  public XMLParserScriptRunnerHost,
                                  public xml_ffi::XmlCallbacks {
 public:
  explicit XMLDocumentParserRs(Document&, LocalFrameView* = nullptr);
  XMLDocumentParserRs(DocumentFragment*, Element*, ParserContentPolicy);
  void Trace(Visitor*) const override;

  // Exposed for callbacks:
  void HandleError(XMLErrors::ErrorType, const char* message, TextPosition);

  // TODO(https://crbug.com/441911594): This class might need implement
  // ParseDocumentFragment() which is called
  // from DocumentFragment::ParseXML - but may be unused?
  // At least no tests so far seem to have exercised this.

  // Used by the XMLHttpRequest to check if the responseXML was well formed.
  bool WellFormed() const override;
  OrdinalNumber LineNumber() const override;
  TextPosition GetTextPosition() const override;

  // XmlCallbacks
  void StartDocument(rust::Str version,
                     rust::Str encoding,
                     xml_ffi::StandaloneInfo standaloneInfo) override;
  void ProcessingInstruction(rust::Str target, rust::Str data) override;
  void StartElementNs(rust::Str local_name,
                      bool null_prefix,
                      rust::Str prefix,
                      bool null_ns,
                      rust::Str ns,
                      xml_ffi::AttributesIterator& attributes,
                      xml_ffi::NamespacesIterator& namespaces) override;

  void EndElementNs(rust::Str local_name,
                    rust::Str prefix,
                    rust::Str ns) override;
  void Characters(rust::Str characters) override;
  void CData(rust::Str data) override;
  void Comment(rust::Str data) override;
  void DocType(rust::Str name_rs,
               rust::Str public_id_rs,
               rust::Str system_id_rs) override;
  void EndDocument() override;

 private:
  void ProcessEvents();

  // From DocumentParser
  void insert(const String&) override { NOTREACHED(); }
  void Append(const String&) override;
  void Finish() override;
  void ExecuteScriptsWaitingForResources() final;
  bool IsWaitingForScripts() const override;
  void StopParsing() override;
  void Detach() override;
  OrdinalNumber ColumnNumber() const;

  void DidAddPendingParserBlockingStylesheet() final;
  void DidLoadAllPendingParserBlockingStylesheets() final;

  void EndInternal();

  // XMLParserScriptRunnerHost
  void NotifyScriptExecuted() override;

  void PauseParsing();
  void ResumeParsing();

 private:
  void PushCurrentNode(ContainerNode*);
  void PopCurrentNode();
  void ClearCurrentNodeStack();

  void CreateLeafTextNodeIfNeeded();
  bool UpdateLeafTextNode();

  void InsertErrorMessageBlock();

  void CheckIfBlockingStyleSheetAdded();

  Member<XMLParserScriptRunner> script_runner_;
  TextPosition script_start_position_;
  bool saw_error_ = false;
  bool saw_css_ = false;

  XMLErrors xml_errors_;
  Member<Document> document_;

  StringBuilder buffered_text_;

  Member<ContainerNode> current_node_;
  // In fragment parsing, track a parent element that has reset the default
  // namespace, in order not to apply the surrounding element's default
  // namespace when fixing-up fragment element's namespace information.
  Member<ContainerNode> ancestor_resetting_namespace_ = nullptr;
  HeapVector<Member<ContainerNode>> current_node_stack_;
  Member<Text> leaf_text_node_;

  rust::Box<xml_ffi::XmlReadState> read_state_;

  const bool parsing_fragment_;
  bool saw_first_element_ = false;
  bool saw_end_document_ = false;

  bool parser_paused_ = false;
  bool requesting_script_ = false;
  bool waiting_for_stylesheets_ = false;
  bool added_pending_parser_blocking_stylesheet_ = false;
  bool finish_called_ = false;

  AtomicString default_namespace_uri_;
  typedef HashMap<AtomicString, AtomicString> PrefixForNamespaceMap;
  PrefixForNamespaceMap prefix_to_namespace_map_;
};

HashMap<String, String> ParseAttributesRust(const String&, bool& attrs_ok);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_XML_PARSER_XML_DOCUMENT_PARSER_RS_H_
