// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/document_xml_tree_viewer.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

void TransformDocumentToXMLTreeView(Document& document) {
  String script_string =
      UncompressResourceAsASCIIString(IDR_DOCUMENTXMLTREEVIEWER_JS);
  String css_string =
      UncompressResourceAsASCIIString(IDR_DOCUMENTXMLTREEVIEWER_CSS);

  v8::HandleScope handle_scope(V8PerIsolateData::MainThreadIsolate());

  document.GetFrame()->GetScriptController().ExecuteScriptInIsolatedWorld(
      IsolatedWorldId::kDocumentXMLTreeViewerWorldId,
      ScriptSourceCode(script_string, ScriptSourceLocationType::kInternal),
      KURL(), SanitizeScriptErrors::kSanitize);

  Element* element = document.getElementById("xml-viewer-style");
  if (element) {
    element->setTextContent(css_string);
  }
}

}  // namespace blink
