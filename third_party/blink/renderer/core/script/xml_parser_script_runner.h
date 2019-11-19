// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_XML_PARSER_SCRIPT_RUNNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_XML_PARSER_SCRIPT_RUNNER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

class Element;
class XMLParserScriptRunnerHost;

// XMLParserScriptRunner implements the interaction between an XML parser
// (XMLDocumentParser) and <script> elements and their loading/execution.
//
// https://html.spec.whatwg.org/C/#parsing-xhtml-documents
class XMLParserScriptRunner final
    : public GarbageCollected<XMLParserScriptRunner>,
      public PendingScriptClient {
  USING_GARBAGE_COLLECTED_MIXIN(XMLParserScriptRunner);

 public:
  explicit XMLParserScriptRunner(XMLParserScriptRunnerHost*);
  ~XMLParserScriptRunner() override;

  bool HasParserBlockingScript() const { return parser_blocking_script_; }

  void ProcessScriptElement(Document&, Element*, TextPosition);
  void Detach();

  void Trace(Visitor*) override;

 private:
  // from PendingScriptClient
  void PendingScriptFinished(PendingScript*) override;

  // https://html.spec.whatwg.org/C/#pending-parsing-blocking-script
  // TODO(crbug/717643): Support module scripts, and turn this into
  // Member<>.
  Member<PendingScript> parser_blocking_script_;

  Member<XMLParserScriptRunnerHost> host_;

  // TODO(crbug/717643): Implement
  // https://html.spec.whatwg.org/C/#list-of-scripts-that-will-execute-when-the-document-has-finished-parsing
  DISALLOW_COPY_AND_ASSIGN(XMLParserScriptRunner);
};

}  // namespace blink

#endif
