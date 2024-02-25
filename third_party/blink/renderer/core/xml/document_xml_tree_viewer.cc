// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/document_xml_tree_viewer.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

void TransformDocumentToXMLTreeView(Document& document) {
  String script_string =
      UncompressResourceAsASCIIString(IDR_DOCUMENTXMLTREEVIEWER_JS);
  String css_string =
      UncompressResourceAsASCIIString(IDR_DOCUMENTXMLTREEVIEWER_CSS);

  v8::HandleScope handle_scope(document.GetAgent().isolate());

  ClassicScript::CreateUnspecifiedScript(script_string,
                                         ScriptSourceLocationType::kInternal)
      ->RunScriptInIsolatedWorldAndReturnValue(
          document.domWindow(), IsolatedWorldId::kDocumentXMLTreeViewerWorldId);

  Element* element = document.getElementById(AtomicString("xml-viewer-style"));
  if (element) {
    element->setTextContent(css_string);
  }
}

}  // namespace blink
