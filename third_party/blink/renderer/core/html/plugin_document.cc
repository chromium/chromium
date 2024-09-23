/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/plugin_document.h"

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/raw_data_document_parser.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

namespace blink {

// FIXME: Share more code with MediaDocumentParser.
class PluginDocumentParser : public RawDataDocumentParser {
 public:
  PluginDocumentParser(Document* document, Color background_color)
      : RawDataDocumentParser(document),
        embed_element_(nullptr),
        background_color_(background_color) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(embed_element_);
    RawDataDocumentParser::Trace(visitor);
  }

 private:
  void AppendBytes(base::span<const uint8_t>) override;
  void Finish() override;
  void StopParsing() override;

  void CreateDocumentStructure();

  WebPluginContainerImpl* GetPluginView() const;

  Member<HTMLEmbedElement> embed_element_;
  const Color background_color_;
};

void PluginDocumentParser::CreateDocumentStructure() {
  // TODO(dgozman): DocumentLoader might call Finish on a stopped parser.
  // See also comments for DocumentParser::{Detach,StopParsing}.
  if (IsStopped())
    return;
  if (embed_element_)
    return;

  // FIXME: Assert we have a loader to figure out why the original null checks
  // and assert were added for the security bug in
  // http://trac.webkit.org/changeset/87566
  DCHECK(GetDocument());
  CHECK(GetDocument()->Loader());

  LocalFrame* frame = GetDocument()->GetFrame();
  if (!frame)
    return;

  // FIXME: Why does this check settings?
  if (!frame->GetSettings() || !frame->Loader().AllowPlugins())
    return;

  auto* root_element = MakeGarbageCollected<HTMLHtmlElement>(*GetDocument());
  GetDocument()->AppendChild(root_element);
  root_element->InsertedByParser();
  if (IsStopped())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  auto* body = MakeGarbageCollected<HTMLBodyElement>(*GetDocument());
  body->SetInlineStyleProperty(CSSPropertyID::kHeight, 100.0,
                               CSSPrimitiveValue::UnitType::kPercentage);
  body->SetInlineStyleProperty(CSSPropertyID::kWidth, 100.0,
                               CSSPrimitiveValue::UnitType::kPercentage);
  body->SetInlineStyleProperty(CSSPropertyID::kOverflow, CSSValueID::kHidden);
  body->SetInlineStyleProperty(CSSPropertyID::kMargin, 0.0,
                               CSSPrimitiveValue::UnitType::kPixels);
  body->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                               *cssvalue::CSSColor::Create(background_color_));
  root_element->AppendChild(body);
  if (IsStopped()) {
    // Possibly detached by a mutation event listener installed in
    // runScriptsAtDocumentElementAvailable.
    return;
  }

  AtomicString hundred_percent("100%");
  AtomicString plugin("plugin");
  embed_element_ = MakeGarbageCollected<HTMLEmbedElement>(*GetDocument());
  embed_element_->setAttribute(html_names::kWidthAttr, hundred_percent);
  embed_element_->setAttribute(html_names::kHeightAttr, hundred_percent);
  embed_element_->setAttribute(html_names::kNameAttr, plugin);
  embed_element_->setAttribute(html_names::kIdAttr, plugin);
  embed_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString(GetDocument()->Url().GetString()));
  embed_element_->setAttribute(html_names::kTypeAttr,
                               GetDocument()->Loader()->MimeType());
  body->AppendChild(embed_element_);
  if (IsStopped()) {
    // Possibly detached by a mutation event listener installed in
    // runScriptsAtDocumentElementAvailable.
    return;
  }

  To<PluginDocument>(GetDocument())->SetPluginNode(embed_element_);

  GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kPlugin);

  // We need the plugin to load synchronously so we can get the
  // WebPluginContainerImpl below so flush the layout tasks now instead of
  // waiting on the timer.
  frame->View()->FlushAnyPendingPostLayoutTasks();
  // Focus the plugin here, as the line above is where the plugin is created.
  if (frame->IsMainFrame()) {
    embed_element_->Focus();
    if (IsStopped()) {
      // Possibly detached by a mutation event listener installed in
      // runScriptsAtDocumentElementAvailable.
      return;
    }
  }

  if (WebPluginContainerImpl* view = GetPluginView())
    view->DidReceiveResponse(GetDocument()->Loader()->GetResponse());
}

void PluginDocumentParser::AppendBytes(base::span<const uint8_t> data) {
  CreateDocumentStructure();
  if (IsStopped())
    return;
  if (data.empty()) {
    return;
  }
  if (WebPluginContainerImpl* view = GetPluginView()) {
    view->DidReceiveData(base::as_chars(data));
  }
}

void PluginDocumentParser::Finish() {
  CreateDocumentStructure();
  embed_element_ = nullptr;
  RawDataDocumentParser::Finish();
}

void PluginDocumentParser::StopParsing() {
  CreateDocumentStructure();
  RawDataDocumentParser::StopParsing();
}

WebPluginContainerImpl* PluginDocumentParser::GetPluginView() const {
  return To<PluginDocument>(GetDocument())->GetPluginView();
}

PluginDocument::PluginDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, {DocumentClass::kPlugin}),
      background_color_(
          GetFrame()->GetPluginData()->PluginBackgroundColorForMimeType(
              initializer.GetMimeType())) {
  SetCompatibilityMode(kNoQuirksMode);
  LockCompatibilityMode();
  GetExecutionContext()->GetScheduler()->RegisterStickyFeature(
      SchedulingPolicy::Feature::kContainsPlugins,
      {SchedulingPolicy::DisableBackForwardCache()});
}

DocumentParser* PluginDocument::CreateParser() {
  return MakeGarbageCollected<PluginDocumentParser>(this, background_color_);
}

WebPluginContainerImpl* PluginDocument::GetPluginView() {
  return plugin_node_ ? plugin_node_->OwnedPlugin() : nullptr;
}

void PluginDocument::Shutdown() {
  // Release the plugin node so that we don't have a circular reference.
  plugin_node_ = nullptr;
  HTMLDocument::Shutdown();
}

void PluginDocument::Trace(Visitor* visitor) const {
  visitor->Trace(plugin_node_);
  HTMLDocument::Trace(visitor);
}

}  // namespace blink
