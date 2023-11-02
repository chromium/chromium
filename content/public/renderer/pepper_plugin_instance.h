// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_
#define CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_

#include <stdint.h>

#include <string>

#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/point_f.h"

class GURL;

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
class Rect;
}

namespace ppapi {
class PpapiPermissions;
class VarTracker;
}

namespace IPC {
struct ChannelHandle;
}

namespace blink {
class WebPluginContainer;
}

namespace v8 {
class Isolate;
}

namespace content {
class RenderFrame;

class PepperPluginInstance {
 public:
  // Return the PepperPluginInstance for the given |instance_id|. Will return
  // null if the instance is in the process of being deleted.
  static CONTENT_EXPORT PepperPluginInstance* Get(PP_Instance instance_id);

  virtual ~PepperPluginInstance() {}

  virtual content::RenderFrame* GetRenderFrame() = 0;

  virtual blink::WebPluginContainer* GetContainer() = 0;

  virtual v8::Isolate* GetIsolate() = 0;

  virtual ppapi::VarTracker* GetVarTracker() = 0;

  virtual const GURL& GetPluginURL() = 0;

  // Returns the location of this module.
  virtual base::FilePath GetModulePath() = 0;

  // Creates a PPB_ImageData given a Skia image.
  virtual PP_Resource CreateImage(gfx::ImageSkia* source_image,
                                  float scale) = 0;

  // Switches this instance with one that uses the out of process IPC proxy.
  virtual PP_ExternalPluginResult SwitchToOutOfProcessProxy(
      const base::FilePath& file_path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) = 0;

  // Set this to true if plugin thinks it will always be on top. This allows us
  // to use a more optimized painting path in some cases.
  virtual void SetAlwaysOnTop(bool on_top) = 0;

  // Returns true iff the plugin is a full-page plugin (i.e. not in an iframe
  // or embedded in a page).
  virtual bool IsFullPagePlugin() = 0;

  virtual bool IsRectTopmost(const gfx::Rect& rect) = 0;

  // Creates a pending PepperFileRefRendererHost. Returns 0 on failure.
  virtual int MakePendingFileRefRendererHost(const base::FilePath& path) = 0;

  // Sets a read-only property on the <embed> tag for this plugin instance.
  virtual void SetEmbedProperty(PP_Var key, PP_Var value) = 0;

  // Sets the selected text for this plugin.
  virtual void SetSelectedText(const std::u16string& selected_text) = 0;

  // Sets the link currently under the cursor.
  virtual void SetLinkUnderCursor(const std::string& url) = 0;

  // Sets the text input type for this plugin.
  virtual void SetTextInputType(ui::TextInputType type) = 0;

  // Posts a message to the JavaScript object for this instance.
  virtual void PostMessageToJavaScript(PP_Var message) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_
