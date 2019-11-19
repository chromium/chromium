// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_
#define CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_

#include <stdint.h>

#include <string>

#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/point_f.h"

class GURL;
struct PP_PdfAccessibilityActionData;

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
struct URLRequestInfoData;
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

  // Switches between fullscreen and normal mode. If |delay_report| is set to
  // false, it may report the new state through DidChangeView immediately. If
  // true, it will delay it. When called from the plugin, delay_report should
  // be true to avoid re-entrancy. Returns true if the switch will be carried
  // out, because of this call or because a switch was pending already anyway.
  // Returns false if the switch will not be carried out because fullscreen mode
  // is disallowed by a preference.
  virtual bool FlashSetFullscreen(bool fullscreen, bool delay_report) = 0;

  virtual bool IsRectTopmost(const gfx::Rect& rect) = 0;

  virtual int32_t Navigate(const ppapi::URLRequestInfoData& request,
                           const char* target,
                           bool from_user_action) = 0;

  // Creates a pending PepperFileRefRendererHost. Returns 0 on failure.
  virtual int MakePendingFileRefRendererHost(const base::FilePath& path) = 0;

  // Sets a read-only property on the <embed> tag for this plugin instance.
  virtual void SetEmbedProperty(PP_Var key, PP_Var value) = 0;

  // Sets the selected text for this plugin.
  virtual void SetSelectedText(const base::string16& selected_text) = 0;

  // Sets the link currently under the cursor.
  virtual void SetLinkUnderCursor(const std::string& url) = 0;

  // Sets the text input type for this plugin.
  virtual void SetTextInputType(ui::TextInputType type) = 0;

  // Posts a message to the JavaScript object for this instance.
  virtual void PostMessageToJavaScript(PP_Var message) = 0;

  // Sets the current mouse caret position.
  virtual void SetCaretPosition(const gfx::PointF& position) = 0;

  // Sends notification that the selection extent has been modified.
  virtual void MoveRangeSelectionExtent(const gfx::PointF& extent) = 0;

  // Sends notification of the base and extent of the current selection.
  // The extent provided maybe modified by subsequent calls to
  // MoveRangeSelectionExtent.
  virtual void SetSelectionBounds(const gfx::PointF& base,
                                  const gfx::PointF& extent) = 0;

  // Returns true if the plugin text can be edited.
  virtual bool CanEditText() = 0;

  // Returns true if the plugin has editable text. i.e. The editable text field
  // is non-empty. Assumes CanEditText() returns true.
  virtual bool HasEditableText() = 0;

  // Replaces the plugin's selected text, if any, with |text|. Assumes
  // CanEditText() returns true.
  virtual void ReplaceSelection(const std::string& text) = 0;

  // Issues a select all command.
  virtual void SelectAll() = 0;

  // Returns true if the plugin can undo/redo.
  virtual bool CanUndo() = 0;
  virtual bool CanRedo() = 0;

  // Issues undo and redo commands.
  virtual void Undo() = 0;
  virtual void Redo() = 0;

  // Forwards Accessibility actions to plugin.
  virtual void HandleAccessibilityAction(
      const PP_PdfAccessibilityActionData& action_data) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_
