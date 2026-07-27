// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/document_xml_tree_viewer.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_microtasks_scope.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {
namespace {

constexpr char kXMLViewerStyleElementId[] = "xml-viewer-style";
constexpr char kXMLViewerShadowRootGlobal[] = "__xmlViewerShadowRoot";

String XMLTreeViewerScriptWithCall(bool use_shadow_root) {
  String script_string =
      UncompressResourceAsASCIIString(IDR_DOCUMENTXMLTREEVIEWER_JS);
  if (use_shadow_root) {
    return StrCat({script_string, "\nprepareWebKitXMLViewer(self.",
                   kXMLViewerShadowRootGlobal, ");\ndelete self.",
                   kXMLViewerShadowRootGlobal, ";\n"});
  }
  return StrCat({script_string, "\nprepareWebKitXMLViewer();\n"});
}

void SetXMLViewerStyle(Element* style_element) {
  if (style_element) {
    style_element->setTextContent(
        UncompressResourceAsASCIIString(IDR_DOCUMENTXMLTREEVIEWER_CSS));
  }
}

bool SetShadowRootGlobal(Document& document, ShadowRoot& shadow_root) {
  LocalFrame* frame = document.GetFrame();
  if (!frame) {
    return false;
  }

  v8::Isolate* isolate = document.GetAgent().isolate();
  DOMWrapperWorld& world = *DOMWrapperWorld::EnsureIsolatedWorld(
      isolate, IsolatedWorldId::kDocumentXMLTreeViewerWorldId);
  ScriptState* script_state = ToScriptState(frame, world);
  if (!script_state || !script_state->ContextIsValid()) {
    return false;
  }

  ScriptState::Scope script_state_scope(script_state);
  V8DoNotRunMicrotasksScope microtasks_scope(script_state);
  v8::Local<v8::Context> context = script_state->GetContext();
  return context->Global()
      ->Set(context, V8String(isolate, kXMLViewerShadowRootGlobal),
            ToV8Traits<ShadowRoot>::ToV8(script_state, &shadow_root))
      .FromMaybe(false);
}

void RunXMLTreeViewerScript(Document& document, bool use_shadow_root) {
  v8::HandleScope handle_scope(document.GetAgent().isolate());

  ClassicScript::CreateUnspecifiedScript(
      XMLTreeViewerScriptWithCall(use_shadow_root),
      ScriptSourceLocationType::kInternal)
      ->RunScriptInIsolatedWorldAndReturnValue(
          document.domWindow(), IsolatedWorldId::kDocumentXMLTreeViewerWorldId);
}

}  // namespace

void TransformDocumentToXMLTreeView(Document& document,
                                    bool preserve_document_element) {
  if (!preserve_document_element) {
    RunXMLTreeViewerScript(document, /*use_shadow_root=*/false);
    SetXMLViewerStyle(
        document.getElementById(AtomicString(kXMLViewerStyleElementId)));
    return;
  }

  Element* document_element = document.documentElement();
  if (!document_element) {
    return;
  }

  ShadowRoot& shadow_root = document_element->EnsureUserAgentShadowRoot();
  shadow_root.RemoveChildren();
  if (!SetShadowRootGlobal(document, shadow_root)) {
    return;
  }

  RunXMLTreeViewerScript(document, /*use_shadow_root=*/true);
  SetXMLViewerStyle(
      shadow_root.getElementById(AtomicString(kXMLViewerStyleElementId)));
}

}  // namespace blink
