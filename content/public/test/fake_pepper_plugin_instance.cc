// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_pepper_plugin_instance.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace content {

FakePepperPluginInstance::~FakePepperPluginInstance() {}

content::RenderFrame* FakePepperPluginInstance::GetRenderFrame() {
  return nullptr;
}

blink::WebPluginContainer* FakePepperPluginInstance::GetContainer() {
  return nullptr;
}

v8::Isolate* FakePepperPluginInstance::GetIsolate() {
  return nullptr;
}

ppapi::VarTracker* FakePepperPluginInstance::GetVarTracker() {
  return nullptr;
}

const GURL& FakePepperPluginInstance::GetPluginURL() {
  return gurl_;
}

base::FilePath FakePepperPluginInstance::GetModulePath() {
  return base::FilePath();
}

PP_Resource FakePepperPluginInstance::CreateImage(gfx::ImageSkia* source_image,
                                                  float scale) {
  return 0;
}

PP_ExternalPluginResult FakePepperPluginInstance::SwitchToOutOfProcessProxy(
    const base::FilePath& file_path,
    ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id) {
  return PP_EXTERNAL_PLUGIN_FAILED;
}

void FakePepperPluginInstance::SetAlwaysOnTop(bool on_top) {}

bool FakePepperPluginInstance::IsFullPagePlugin() {
  return false;
}

bool FakePepperPluginInstance::FlashSetFullscreen(bool fullscreen,
                                                  bool delay_report) {
  return false;
}

bool FakePepperPluginInstance::IsRectTopmost(const gfx::Rect& rect) {
  return false;
}

int32_t FakePepperPluginInstance::Navigate(
    const ppapi::URLRequestInfoData& request,
    const char* target,
    bool from_user_action) {
  return PP_ERROR_FAILED;
}

int FakePepperPluginInstance::MakePendingFileRefRendererHost(
    const base::FilePath& path) {
  return 0;
}

void FakePepperPluginInstance::SetEmbedProperty(PP_Var key, PP_Var value) {}

void FakePepperPluginInstance::SetSelectedText(
    const base::string16& selected_text) {}

void FakePepperPluginInstance::SetLinkUnderCursor(const std::string& url) {}
void FakePepperPluginInstance::SetTextInputType(ui::TextInputType type) {}
void FakePepperPluginInstance::PostMessageToJavaScript(PP_Var message) {}

void FakePepperPluginInstance::SetCaretPosition(const gfx::PointF& position) {}

void FakePepperPluginInstance::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {}

void FakePepperPluginInstance::SetSelectionBounds(const gfx::PointF& base,
                                                  const gfx::PointF& extent) {}

bool FakePepperPluginInstance::CanEditText() {
  return false;
}

bool FakePepperPluginInstance::HasEditableText() {
  return false;
}

void FakePepperPluginInstance::ReplaceSelection(const std::string& text) {}

void FakePepperPluginInstance::SelectAll() {}

bool FakePepperPluginInstance::CanUndo() {
  return false;
}

bool FakePepperPluginInstance::CanRedo() {
  return false;
}

void FakePepperPluginInstance::Undo() {}

void FakePepperPluginInstance::Redo() {}

void FakePepperPluginInstance::HandleAccessibilityAction(
    const PP_PdfAccessibilityActionData& action_data) {}

}  // namespace content
