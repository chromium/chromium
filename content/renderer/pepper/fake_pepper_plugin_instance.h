// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_
#define CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_

#include <stdint.h>

#include <string>

#include "content/public/renderer/pepper_plugin_instance.h"
#include "url/gurl.h"

namespace content {

class FakePepperPluginInstance : public PepperPluginInstance {
 public:
  ~FakePepperPluginInstance() override;

  // PepperPluginInstance overrides.
  content::RenderFrame* GetRenderFrame() override;
  blink::WebPluginContainer* GetContainer() override;
  v8::Isolate* GetIsolate() override;
  ppapi::VarTracker* GetVarTracker() override;
  const GURL& GetPluginURL() override;
  base::FilePath GetModulePath() override;
  PP_Resource CreateImage(gfx::ImageSkia* source_image, float scale) override;
  PP_ExternalPluginResult SwitchToOutOfProcessProxy(
      const base::FilePath& file_path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) override;
  void SetAlwaysOnTop(bool on_top) override;
  bool IsFullPagePlugin() override;
  bool IsRectTopmost(const gfx::Rect& rect) override;
  int MakePendingFileRefRendererHost(const base::FilePath& path) override;
  void SetEmbedProperty(PP_Var key, PP_Var value) override;
  void SetSelectedText(const std::u16string& selected_text) override;
  void SetLinkUnderCursor(const std::string& url) override;
  void SetTextInputType(ui::TextInputType type) override;
  void PostMessageToJavaScript(PP_Var message) override;

 private:
  GURL gurl_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_
