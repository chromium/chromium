// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_webplugin_impl.h"

#include <stddef.h>
#include <cmath>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/message_channel.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/v8object_var.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_print_scaling_option.h"
#include "url/gurl.h"

using ppapi::V8ObjectVar;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPluginParams;
using blink::WebPoint;
using blink::WebPrintParams;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

struct PepperWebPluginImpl::InitData {
  scoped_refptr<PluginModule> module;
  RenderFrameImpl* render_frame;
  std::vector<std::string> arg_names;
  std::vector<std::string> arg_values;
  GURL url;
};

PepperWebPluginImpl::PepperWebPluginImpl(
    PluginModule* plugin_module,
    const WebPluginParams& params,
    RenderFrameImpl* render_frame,
    std::unique_ptr<PluginInstanceThrottlerImpl> throttler)
    : init_data_(new InitData()),
      full_frame_(params.load_manually),
      throttler_(std::move(throttler)),
      instance_object_(PP_MakeUndefined()),
      container_(nullptr) {
  DCHECK(plugin_module);
  init_data_->module = plugin_module;
  init_data_->render_frame = render_frame;
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    init_data_->arg_names.push_back(params.attribute_names[i].Utf8());
    init_data_->arg_values.push_back(params.attribute_values[i].Utf8());
  }
  init_data_->url = params.url;

  // Set subresource URL for crash reporting.
  static base::debug::CrashKeyString* subresource_url =
      base::debug::AllocateCrashKeyString("subresource_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(subresource_url, init_data_->url.spec());

  if (throttler_)
    throttler_->SetWebPlugin(this);
}

PepperWebPluginImpl::~PepperWebPluginImpl() {}

blink::WebPluginContainer* PepperWebPluginImpl::Container() const {
  return container_;
}

bool PepperWebPluginImpl::Initialize(WebPluginContainer* container) {
  DCHECK(container);
  DCHECK_EQ(this, container->Plugin());

  container_ = container;

  // The plugin delegate may have gone away.
  instance_ = init_data_->module->CreateInstance(
      init_data_->render_frame, container, init_data_->url);
  if (!instance_)
    return false;

  if (!instance_->Initialize(init_data_->arg_names, init_data_->arg_values,
                             full_frame_, std::move(throttler_))) {
    // If |container_| is nullptr, this object has already been synchronously
    // destroy()-ed during |instance_|'s Initialize call. In that case, we early
    // exit. We neither create a replacement plugin nor destroy() ourselves.
    if (!container_)
      return false;

    DCHECK(instance_);
    ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(instance_object_);
    instance_object_ = PP_MakeUndefined();
    instance_->Delete();
    instance_ = nullptr;

    blink::WebPlugin* replacement_plugin =
        GetContentClient()->renderer()->CreatePluginReplacement(
            init_data_->render_frame, init_data_->module->path());
    if (!replacement_plugin)
      return false;

    // The replacement plugin, if it exists, must never fail to initialize.
    container->SetPlugin(replacement_plugin);
    CHECK(replacement_plugin->Initialize(container));

    DCHECK(container->Plugin() == replacement_plugin);
    DCHECK(replacement_plugin->Container() == container);

    // Since the container now owns the replacement plugin instead of this
    // object, we must schedule ourselves for deletion.
    Destroy();

    return true;
  }

  init_data_.reset();
  return true;
}

void PepperWebPluginImpl::Destroy() {
  container_ = nullptr;

  if (instance_) {
    ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(instance_object_);
    instance_object_ = PP_MakeUndefined();
    instance_->Delete();
    instance_ = nullptr;
  }

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> PepperWebPluginImpl::V8ScriptableObject(
    v8::Isolate* isolate) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See e.g. crbug.com/503401.
  if (!instance_)
    return v8::Local<v8::Object>();
  // Call through the plugin to get its instance object. The plugin should pass
  // us a reference which we release in destroy().
  if (instance_object_.type == PP_VARTYPE_UNDEFINED)
    instance_object_ = instance_->GetInstanceObject(isolate);
  // GetInstanceObject talked to the plugin which may have removed the instance
  // from the DOM, in which case instance_ would be nullptr now.
  if (!instance_)
    return v8::Local<v8::Object>();

  scoped_refptr<V8ObjectVar> object_var(
      V8ObjectVar::FromPPVar(instance_object_));
  // If there's an InstanceObject, tell the Instance's MessageChannel to pass
  // any non-postMessage calls to it.
  if (object_var) {
    MessageChannel* message_channel = instance_->message_channel();
    if (message_channel)
      message_channel->SetPassthroughObject(object_var->GetHandle());
  }

  v8::Local<v8::Object> result = instance_->GetMessageChannelObject();
  return result;
}

void PepperWebPluginImpl::Paint(cc::PaintCanvas* canvas, const WebRect& rect) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_ && !instance_->FlashIsFullscreenOrPending())
    instance_->Paint(canvas, plugin_rect_, rect);
}

void PepperWebPluginImpl::UpdateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebRect& unobscured_rect,
    bool is_visible) {
  plugin_rect_ = window_rect;
  if (instance_ && !instance_->FlashIsFullscreenOrPending())
    instance_->ViewChanged(plugin_rect_, clip_rect, unobscured_rect);
}

void PepperWebPluginImpl::UpdateFocus(bool focused,
                                      blink::WebFocusType focus_type) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_)
    instance_->SetWebKitFocus(focused);
}

void PepperWebPluginImpl::UpdateVisibility(bool visible) {}

blink::WebInputEventResult PepperWebPluginImpl::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    blink::WebCursorInfo& cursor_info) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_ || instance_->FlashIsFullscreenOrPending())
    return blink::WebInputEventResult::kNotHandled;
  return instance_->HandleCoalescedInputEvent(coalesced_event, &cursor_info)
             ? blink::WebInputEventResult::kHandledApplication
             : blink::WebInputEventResult::kNotHandled;
}

void PepperWebPluginImpl::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return;
  DCHECK(!instance_->document_loader());
  instance_->HandleDocumentLoad(response);
}

void PepperWebPluginImpl::DidReceiveData(const char* data, size_t data_length) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return;
  blink::WebAssociatedURLLoaderClient* document_loader =
      instance_->document_loader();
  if (document_loader)
    document_loader->DidReceiveData(data, data_length);
}

void PepperWebPluginImpl::DidFinishLoading() {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return;
  blink::WebAssociatedURLLoaderClient* document_loader =
      instance_->document_loader();
  if (document_loader)
    document_loader->DidFinishLoading();
}

void PepperWebPluginImpl::DidFailLoading(const blink::WebURLError& error) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return;
  blink::WebAssociatedURLLoaderClient* document_loader =
      instance_->document_loader();
  if (document_loader)
    document_loader->DidFail(error);
}

bool PepperWebPluginImpl::HasSelection() const {
  return !SelectionAsText().IsEmpty();
}

WebString PepperWebPluginImpl::SelectionAsText() const {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return WebString();
  return WebString::FromUTF16(instance_->GetSelectedText(false));
}

WebString PepperWebPluginImpl::SelectionAsMarkup() const {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return WebString();
  return WebString::FromUTF16(instance_->GetSelectedText(true));
}

bool PepperWebPluginImpl::CanEditText() const {
  return instance_ && instance_->CanEditText();
}

bool PepperWebPluginImpl::HasEditableText() const {
  return instance_ && instance_->HasEditableText();
}

bool PepperWebPluginImpl::CanUndo() const {
  return instance_ && instance_->CanUndo();
}

bool PepperWebPluginImpl::CanRedo() const {
  return instance_ && instance_->CanRedo();
}

bool PepperWebPluginImpl::ExecuteEditCommand(const blink::WebString& name) {
  return ExecuteEditCommand(name, WebString());
}

bool PepperWebPluginImpl::ExecuteEditCommand(const blink::WebString& name,
                                             const blink::WebString& value) {
  if (!instance_)
    return false;

  if (name == "Cut") {
    if (!HasSelection() || !CanEditText())
      return false;

    if (!clipboard_) {
      blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          clipboard_.BindNewPipeAndPassReceiver());
    }
    base::string16 markup;
    base::string16 text;
    if (instance_) {
      markup = instance_->GetSelectedText(true);
      text = instance_->GetSelectedText(false);
    }
    clipboard_->WriteHtml(markup, GURL());
    clipboard_->WriteText(text);
    clipboard_->CommitWrite();

    instance_->ReplaceSelection("");
    return true;
  }

  // If the clipboard contains something other than text (e.g. an image),
  // ClipboardHost::ReadText() returns an empty string. The empty string is
  // then pasted, replacing any selected text. This behavior is consistent with
  // that of HTML text form fields.
  if (name == "Paste" || name == "PasteAndMatchStyle") {
    if (!CanEditText())
      return false;

    if (!clipboard_) {
      blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          clipboard_.BindNewPipeAndPassReceiver());
    }
    base::string16 text;
    clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste, &text);

    instance_->ReplaceSelection(base::UTF16ToUTF8(text));
    return true;
  }

  if (name == "SelectAll") {
    if (!CanEditText())
      return false;

    instance_->SelectAll();
    return true;
  }

  if (name == "Undo") {
    if (!CanUndo())
      return false;

    instance_->Undo();
    return true;
  }

  if (name == "Redo") {
    if (!CanRedo())
      return false;

    instance_->Redo();
    return true;
  }

  return false;
}

WebURL PepperWebPluginImpl::LinkAtPosition(const WebPoint& position) const {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return GURL();
  return GURL(instance_->GetLinkAtPosition(position));
}

bool PepperWebPluginImpl::StartFind(const blink::WebString& search_text,
                                    bool case_sensitive,
                                    int identifier) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return false;
  return instance_->StartFind(search_text.Utf8(), case_sensitive, identifier);
}

void PepperWebPluginImpl::SelectFindResult(bool forward, int identifier) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_)
    instance_->SelectFindResult(forward, identifier);
}

void PepperWebPluginImpl::StopFind() {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_)
    instance_->StopFind();
}

bool PepperWebPluginImpl::SupportsPaginatedPrint() {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return false;
  return instance_->SupportsPrintInterface();
}

int PepperWebPluginImpl::PrintBegin(const WebPrintParams& print_params) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return 0;
  return instance_->PrintBegin(print_params);
}

void PepperWebPluginImpl::PrintPage(int page_number, cc::PaintCanvas* canvas) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_)
    instance_->PrintPage(page_number, canvas);
}

void PepperWebPluginImpl::PrintEnd() {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_)
    instance_->PrintEnd();
}

bool PepperWebPluginImpl::GetPrintPresetOptionsFromDocument(
    blink::WebPrintPresetOptions* preset_options) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return false;
  return instance_->GetPrintPresetOptionsFromDocument(preset_options);
}

bool PepperWebPluginImpl::IsPdfPlugin() {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return false;
  return instance_->IsPdfPlugin();
}

bool PepperWebPluginImpl::CanRotateView() {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (!instance_)
    return false;
  return instance_->CanRotateView();
}

void PepperWebPluginImpl::RotateView(RotationType type) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See: crbug.com/715747.
  if (instance_)
    instance_->RotateView(type);
}

bool PepperWebPluginImpl::IsPlaceholder() {
  return false;
}

}  // namespace content
