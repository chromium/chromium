// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/paint/paint_canvas.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "content/common/content_export.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "gin/handle.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/ppp_find_private.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/shared_impl/ppb_instance_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_gamepad_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

struct PP_Point;

namespace blink {
class WebCoalescedInputEvent;
class WebInputEvent;
class WebMouseEvent;
class WebPluginContainer;
class WebURLResponse;
struct WebImeTextSpan;
struct WebCursorInfo;
struct WebURLError;
struct WebPrintParams;
}  // namespace blink

namespace cc {
class TextureLayer;
}

namespace gfx {
class Range;
class Rect;
}

namespace ppapi {
class Resource;
struct InputEventData;
struct PPP_Instance_Combined;
struct URLResponseInfoData;
class ScopedPPVar;
}

namespace printing {
class MetafileSkia;
}

namespace content {

class FullscreenContainer;
class MessageChannel;
class PepperAudioController;
class PepperGraphics2DHost;
class PluginInstanceThrottlerImpl;
class PluginModule;
class PluginObject;
class PPB_Graphics3D_Impl;
class RenderFrameImpl;

// Represents one time a plugin appears on one web page.
//
// Note: to get from a PP_Instance to a PepperPluginInstance*, use the
// ResourceTracker.
class CONTENT_EXPORT PepperPluginInstanceImpl
    : public base::RefCounted<PepperPluginInstanceImpl>,
      public PepperPluginInstance,
      public ppapi::PPB_Instance_Shared,
      public cc::TextureLayerClient,
      public RenderFrameObserver,
      public PluginInstanceThrottler::Observer {
 public:
  // Create and return a PepperPluginInstanceImpl object which supports the most
  // recent version of PPP_Instance possible by querying the given
  // get_plugin_interface function. If the plugin does not support any valid
  // PPP_Instance interface, returns NULL.
  static PepperPluginInstanceImpl* Create(RenderFrameImpl* render_frame,
                                          PluginModule* module,
                                          blink::WebPluginContainer* container,
                                          const GURL& plugin_url);

  // Return the PepperPluginInstanceImpl for the given |instance_id|. Will
  // return the instance even if it is in the process of being deleted.
  // Currently only used in tests.
  static PepperPluginInstanceImpl* GetForTesting(PP_Instance instance_id);

  RenderFrameImpl* render_frame() const { return render_frame_; }
  PluginModule* module() const { return module_.get(); }

  blink::WebPluginContainer* container() const { return container_; }

  PluginInstanceThrottlerImpl* throttler() const { return throttler_.get(); }

  // Returns the PP_Instance uniquely identifying this instance. Guaranteed
  // nonzero.
  PP_Instance pp_instance() const { return pp_instance_; }

  ppapi::thunk::ResourceCreationAPI& resource_creation() {
    return *resource_creation_.get();
  }

  MessageChannel* message_channel() { return message_channel_; }
  v8::Local<v8::Object> GetMessageChannelObject();
  // Called when |message_channel_| is destroyed as it may be destroyed prior to
  // the plugin being destroyed.
  void MessageChannelDestroyed();

  // Return the v8 context for the frame that the plugin is contained in. Care
  // should be taken to use the correct context for plugin<->JS interactions.
  // In cases where JS calls into the plugin, the caller's context should
  // typically be used. When calling from the plugin into JS, this context
  // should typically used.
  v8::Local<v8::Context> GetMainWorldContext();

  // Does some pre-destructor cleanup on the instance. This is necessary
  // because some cleanup depends on the plugin instance still existing (like
  // calling the plugin's DidDestroy function). This function is called from
  // the WebPlugin implementation when WebKit is about to remove the plugin.
  void Delete();

  // Returns true if Delete() has been called on this object.
  bool is_deleted() const;

  GURL document_url() const { return document_url_; }

  // Paints the current backing store to the web page.
  void Paint(cc::PaintCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  // Schedules a paint of the page for the given region. The coordinates are
  // relative to the top-left of the plugin. This does nothing if the plugin
  // has not yet been positioned. You can supply an empty gfx::Rect() to
  // invalidate the entire plugin.
  void InvalidateRect(const gfx::Rect& rect);

  // Schedules a scroll of the plugin.  This uses optimized scrolling only for
  // full-frame plugins, as otherwise there could be other elements on top.  The
  // slow path can also be triggered if there is an overlapping frame.
  void ScrollRect(int dx, int dy, const gfx::Rect& rect);

  // Commit the output to the screen.
  void CommitTransferableResource(const viz::TransferableResource& resource);

  // Passes the committed texture to |texture_layer_| and marks it as in use.
  void PassCommittedTextureToTextureLayer();

  // Callback when the compositor is finished consuming the committed texture.
  void FinishedConsumingCommittedTexture(
      const viz::TransferableResource& resource,
      scoped_refptr<PPB_Graphics3D_Impl> graphics_3d,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  // Called when the out-of-process plugin implementing this instance crashed.
  void InstanceCrashed();

  // PPB_Instance and PPB_Instance_Private implementation.
  bool full_frame() const { return full_frame_; }
  const ppapi::ViewData& view_data() const { return view_data_; }

  // PPP_Instance and PPP_Instance_Private.
  bool Initialize(const std::vector<std::string>& arg_names,
                  const std::vector<std::string>& arg_values,
                  bool full_frame,
                  std::unique_ptr<PluginInstanceThrottlerImpl> throttler);
  bool HandleDocumentLoad(const blink::WebURLResponse& response);
  bool HandleCoalescedInputEvent(const blink::WebCoalescedInputEvent& event,
                                 blink::WebCursorInfo* cursor_info);
  bool HandleInputEvent(const blink::WebInputEvent& event,
                        blink::WebCursorInfo* cursor_info);
  PP_Var GetInstanceObject(v8::Isolate* isolate);
  void ViewChanged(const gfx::Rect& window,
                   const gfx::Rect& clip,
                   const gfx::Rect& unobscured);

  // Handlers for composition events.
  bool HandleCompositionStart(const base::string16& text);
  bool HandleCompositionUpdate(
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      int selection_start,
      int selection_end);
  bool HandleCompositionEnd(const base::string16& text);
  bool HandleTextInput(const base::string16& text);

  // Gets the current text input status.
  ui::TextInputType text_input_type() const { return text_input_type_; }
  gfx::Rect GetCaretBounds() const;
  bool IsPluginAcceptingCompositionEvents() const;
  void GetSurroundingText(base::string16* text, gfx::Range* range) const;

  // Notifications about focus changes, see has_webkit_focus_ below.
  void SetWebKitFocus(bool has_focus);
  void SetContentAreaFocus(bool has_focus);

  // Notification about page visibility. The default is "visible".
  void PageVisibilityChanged(bool is_visible);

  // Notifications that the view has started painting. This message is used to
  // send Flush callbacks to the plugin for Graphics2D/3D.
  void ViewInitiatedPaint();

  // Tracks all live PluginObjects.
  void AddPluginObject(PluginObject* plugin_object);
  void RemovePluginObject(PluginObject* plugin_object);

  base::string16 GetSelectedText(bool html);
  base::string16 GetLinkAtPosition(const gfx::Point& point);
  void RequestSurroundingText(size_t desired_number_of_characters);
  bool StartFind(const std::string& search_text,
                 bool case_sensitive,
                 int identifier);
  void SelectFindResult(bool forward, int identifier);
  void StopFind();

  bool SupportsPrintInterface();
  int PrintBegin(const blink::WebPrintParams& print_params);
  void PrintPage(int page_number, cc::PaintCanvas* canvas);
  void PrintEnd();
  bool GetPrintPresetOptionsFromDocument(
      blink::WebPrintPresetOptions* preset_options);
  bool IsPdfPlugin();

  bool CanRotateView();
  void RotateView(blink::WebPlugin::RotationType type);

  // There are 2 implementations of the fullscreen interface
  // PPB_FlashFullscreen is used by Pepper Flash.
  // PPB_Fullscreen is intended for other applications including NaCl.
  // The two interface are mutually exclusive.

  // Implementation of PPB_FlashFullscreen.

  // Because going to fullscreen is asynchronous (but going out is not), there
  // are 3 states:
  // - normal            : fullscreen_container_ == NULL
  //                       flash_fullscreen_ == false
  // - fullscreen pending: fullscreen_container_ != NULL
  //                       flash_fullscreen_ == false
  // - fullscreen        : fullscreen_container_ != NULL
  //                       flash_fullscreen_ == true
  //
  // In normal state, events come from webkit and painting goes back to it.
  // In fullscreen state, events come from the fullscreen container, and
  // painting goes back to it.
  // In pending state, events from webkit are ignored, and as soon as we
  // receive events from the fullscreen container, we go to the fullscreen
  // state.
  bool FlashIsFullscreenOrPending();

  // Updates |flash_fullscreen_| and sends focus change notification if
  // necessary.
  void UpdateFlashFullscreenState(bool flash_fullscreen);

  FullscreenContainer* fullscreen_container() const {
    return fullscreen_container_;
  }

  // Implementation of PPB_Fullscreen.

  // Because going to/from fullscreen is asynchronous, there are 4 states:
  // - normal            : desired_fullscreen_state_ == false
  //                       view_data_.is_fullscreen == false
  // - fullscreen pending: desired_fullscreen_state_ == true
  //                       view_data_.is_fullscreen == false
  // - fullscreen        : desired_fullscreen_state_ == true
  //                       view_data_.is_fullscreen == true
  // - normal pending    : desired_fullscreen_state_ = false
  //                       view_data_.is_fullscreen = true
  bool IsFullscreenOrPending();

  bool flash_fullscreen() const { return flash_fullscreen_; }

  // Switches between fullscreen and normal mode. The transition is
  // asynchronous. WebKit will trigger corresponding ViewChanged calls.  Returns
  // true on success, false on failure (e.g. trying to enter fullscreen without
  // user activation or trying to set fullscreen when already in fullscreen
  // mode).
  bool SetFullscreen(bool fullscreen);

  // Send the message on to the plugin.
  void HandleMessage(ppapi::ScopedPPVar message);

  // Send the message synchronously to the plugin, and get a result. Returns
  // true if the plugin handled the message, false if it didn't. The plugin
  // won't handle the message if it has not registered a PPP_MessageHandler.
  bool HandleBlockingMessage(ppapi::ScopedPPVar message,
                             ppapi::ScopedPPVar* result);

  // Returns true if the plugin has transient user activation.
  bool HasTransientUserActivation() const;

  // A mouse lock request was pending and this reports success or failure.
  void OnLockMouseACK(bool succeeded);
  // A mouse lock was in place, but has been lost.
  void OnMouseLockLost();
  // A mouse lock is enabled and mouse events are being delivered.
  void HandleMouseLockedInputEvent(const blink::WebMouseEvent& event);

  // Simulates an input event to the plugin by passing it down to WebKit,
  // which sends it back up to the plugin as if it came from the user.
  void SimulateInputEvent(const ppapi::InputEventData& input_event);

  // Simulates an IME event at the level of RenderView which sends it back up to
  // the plugin as if it came from the user.
  bool SimulateIMEEvent(const ppapi::InputEventData& input_event);
  void SimulateImeSetCompositionEvent(const ppapi::InputEventData& input_event);

  // The document loader is valid when the plugin is "full-frame" and in this
  // case is non-NULL as long as the corresponding loader resource is alive.
  // This pointer is non-owning, so the loader must use set_document_loader to
  // clear itself when it is destroyed.
  blink::WebAssociatedURLLoaderClient* document_loader() const {
    return document_loader_;
  }
  void set_document_loader(blink::WebAssociatedURLLoaderClient* loader) {
    document_loader_ = loader;
  }

  void SetGraphics2DTransform(const float& scale,
                              const gfx::PointF& translation);

  // PluginInstance implementation
  RenderFrame* GetRenderFrame() override;
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
  bool FlashSetFullscreen(bool fullscreen, bool delay_report) override;
  bool IsRectTopmost(const gfx::Rect& rect) override;
  int32_t Navigate(const ppapi::URLRequestInfoData& request,
                   const char* target,
                   bool from_user_action) override;
  int MakePendingFileRefRendererHost(const base::FilePath& path) override;
  void SetEmbedProperty(PP_Var key, PP_Var value) override;
  void SetSelectedText(const base::string16& selected_text) override;
  void SetLinkUnderCursor(const std::string& url) override;
  void SetTextInputType(ui::TextInputType type) override;
  void PostMessageToJavaScript(PP_Var message) override;
  void SetCaretPosition(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SetSelectionBounds(const gfx::PointF& base,
                          const gfx::PointF& extent) override;
  bool CanEditText() override;
  bool HasEditableText() override;
  void ReplaceSelection(const std::string& text) override;
  void SelectAll() override;
  bool CanUndo() override;
  bool CanRedo() override;
  void Undo() override;
  void Redo() override;
  void HandleAccessibilityAction(
      const PP_PdfAccessibilityActionData& action_data) override;

  // PPB_Instance_API implementation.
  PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) override;
  PP_Bool IsFullFrame(PP_Instance instance) override;
  const ppapi::ViewData* GetViewData(PP_Instance instance) override;
  PP_Bool FlashIsFullscreen(PP_Instance instance) override;
  PP_Var GetWindowObject(PP_Instance instance) override;
  PP_Var GetOwnerElementObject(PP_Instance instance) override;
  PP_Var ExecuteScript(PP_Instance instance,
                       PP_Var script,
                       PP_Var* exception) override;
  uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance) override;
  uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance) override;
  PP_Var GetDefaultCharSet(PP_Instance instance) override;
  void SetPluginToHandleFindRequests(PP_Instance) override;
  void NumberOfFindResultsChanged(PP_Instance instance,
                                  int32_t total,
                                  PP_Bool final_result) override;
  void SelectedFindResultChanged(PP_Instance instance, int32_t index) override;
  void SetTickmarks(PP_Instance instance,
                    const PP_Rect* tickmarks,
                    uint32_t count) override;
  PP_Bool IsFullscreen(PP_Instance instance) override;
  PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) override;
  PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) override;
  ppapi::Resource* GetSingletonResource(PP_Instance instance,
                                        ppapi::SingletonResourceID id) override;
  int32_t RequestInputEvents(PP_Instance instance,
                             uint32_t event_classes) override;
  int32_t RequestFilteringInputEvents(PP_Instance instance,
                                      uint32_t event_classes) override;
  void ClearInputEventRequest(PP_Instance instance,
                              uint32_t event_classes) override;
  void PostMessage(PP_Instance instance, PP_Var message) override;
  int32_t RegisterMessageHandler(PP_Instance instance,
                                 void* user_data,
                                 const PPP_MessageHandler_0_2* handler,
                                 PP_Resource message_loop) override;
  void UnregisterMessageHandler(PP_Instance instance) override;
  PP_Bool SetCursor(PP_Instance instance,
                    PP_MouseCursor_Type type,
                    PP_Resource image,
                    const PP_Point* hot_spot) override;
  int32_t LockMouse(PP_Instance instance,
                    scoped_refptr<ppapi::TrackedCallback> callback) override;
  void UnlockMouse(PP_Instance instance) override;
  void SetTextInputType(PP_Instance instance, PP_TextInput_Type type) override;
  void UpdateCaretPosition(PP_Instance instance,
                           const PP_Rect& caret,
                           const PP_Rect& bounding_box) override;
  void CancelCompositionText(PP_Instance instance) override;
  void SelectionChanged(PP_Instance instance) override;
  void UpdateSurroundingText(PP_Instance instance,
                             const char* text,
                             uint32_t caret,
                             uint32_t anchor) override;
  PP_Var ResolveRelativeToDocument(PP_Instance instance,
                                   PP_Var relative,
                                   PP_URLComponents_Dev* components) override;
  PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) override;
  PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                    PP_Instance target) override;
  PP_Var GetDocumentURL(PP_Instance instance,
                        PP_URLComponents_Dev* components) override;
  PP_Var GetPluginInstanceURL(PP_Instance instance,
                              PP_URLComponents_Dev* components) override;
  PP_Var GetPluginReferrerURL(PP_Instance instance,
                              PP_URLComponents_Dev* components) override;

  // Reset this instance as proxied. Assigns the instance a new module, resets
  // cached interfaces to point to the out-of-process proxy and re-sends
  // DidCreate, DidChangeView, and HandleDocumentLoad (if necessary).
  // This should be used only when switching an in-process instance to an
  // external out-of-process instance.
  PP_ExternalPluginResult ResetAsProxied(scoped_refptr<PluginModule> module);

  // Checks whether this is a valid instance of the given module. After calling
  // ResetAsProxied above, a NaCl plugin instance's module changes, so external
  // hosts won't recognize it as a valid instance of the original module. This
  // method fixes that be checking that either module_ or original_module_ match
  // the given module.
  bool IsValidInstanceOf(PluginModule* module);

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* transferable_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override;

  // RenderFrameObserver
  void AccessibilityModeChanged(const ui::AXMode& mode) override;
  void OnDestruct() override;

  // PluginInstanceThrottler::Observer
  void OnThrottleStateChange() override;
  void OnHiddenForPlaceholder(bool hidden) override;

  PepperAudioController& audio_controller() {
    return *audio_controller_;
  }

  // Should be used only for logging.
  bool is_flash_plugin() const { return is_flash_plugin_; }

 private:
  friend class base::RefCounted<PepperPluginInstanceImpl>;
  friend class PpapiPluginInstanceTest;
  friend class PpapiUnittest;

  // Delete should be called by the WebPlugin before this destructor.
  ~PepperPluginInstanceImpl() override;

  // Class to record document load notifications and play them back once the
  // real document loader becomes available. Used only by external instances.
  class ExternalDocumentLoader : public blink::WebAssociatedURLLoaderClient {
   public:
    ExternalDocumentLoader();
    ~ExternalDocumentLoader() override;

    void ReplayReceivedData(WebAssociatedURLLoaderClient* document_loader);

    // blink::WebAssociatedURLLoaderClient implementation.
    void DidReceiveData(const char* data, int data_length) override;
    void DidFinishLoading() override;
    void DidFail(const blink::WebURLError& error) override;

   private:
    std::list<std::string> data_;
    bool finished_loading_;
    std::unique_ptr<blink::WebURLError> error_;

    DISALLOW_COPY_AND_ASSIGN(ExternalDocumentLoader);
  };

  // Implements PPB_Gamepad_API. This is just to avoid having an excessive
  // number of interfaces implemented by PepperPluginInstanceImpl.
  class GamepadImpl : public ppapi::thunk::PPB_Gamepad_API,
                      public ppapi::Resource {
   public:
    GamepadImpl();
    // Resource implementation.
    ppapi::thunk::PPB_Gamepad_API* AsPPB_Gamepad_API() override;
    void Sample(PP_Instance instance, PP_GamepadsSampleData* data) override;

   private:
    ~GamepadImpl() override;
  };

  // See the static Create functions above for creating PepperPluginInstanceImpl
  // objects. This constructor is private so that we can hide the
  // PPP_Instance_Combined details while still having 1 constructor to maintain
  // for member initialization.
  PepperPluginInstanceImpl(RenderFrameImpl* render_frame,
                           PluginModule* module,
                           ppapi::PPP_Instance_Combined* instance_interface,
                           blink::WebPluginContainer* container,
                           const GURL& plugin_url);

  bool LoadFindInterface();
  bool LoadInputEventInterface();
  bool LoadMouseLockInterface();
  bool LoadPdfInterface();
  bool LoadPrintInterface();
  bool LoadPrivateInterface();
  bool LoadTextInputInterface();

  // Update any transforms that should be applied to the texture layer.
  void UpdateLayerTransform();

  // Determines if we think the plugin has focus, both content area and webkit
  // (see has_webkit_focus_ below).
  bool PluginHasFocus() const;
  void SendFocusChangeNotification();

  void UpdateTouchEventRequest();
  void UpdateWheelEventRequest();

  void ScheduleAsyncDidChangeView();
  void SendAsyncDidChangeView();
  void SendDidChangeView();

  // Reports the current plugin geometry to the plugin by calling
  // DidChangeView.
  void ReportGeometry();

  // Queries the plugin for supported print formats and sets |format| to the
  // best format to use. Returns false if the plugin does not support any
  // print format that we can handle (we can handle only PDF).
  bool GetPreferredPrintOutputFormat(PP_PrintOutputFormat_Dev* format,
                                     const blink::WebPrintParams& params);

  // Updates the layer for compositing. This creates a layer and attaches to the
  // container if:
  // - we have a bound Graphics3D and the Graphics3D has a texture, OR
  //   we have a bound Graphics2D and are using software compositing
  // - we are not in Flash full-screen mode (or transitioning to it)
  // Otherwise it destroys the layer.
  // It does either operation lazily.
  // force_creation: Force UpdateLayer() to recreate the layer and attaches
  //   to the container. Set to true if the bound device has been changed.
  void UpdateLayer(bool force_creation);

  void DoSetCursor(std::unique_ptr<blink::WebCursorInfo> cursor);

  // Internal helper functions for HandleCompositionXXX().
  bool SendCompositionEventToPlugin(PP_InputEvent_Type type,
                                    const base::string16& text);
  bool SendCompositionEventWithImeTextSpanInformationToPlugin(
      PP_InputEvent_Type type,
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      int selection_start,
      int selection_end);

  // Internal helper function for XXXInputEvents().
  void RequestInputEventsHelper(uint32_t event_classes);

  // Checks if the security origin of the document containing this instance can
  // assess the security origin of the main frame document.
  bool CanAccessMainFrame() const;

  // Returns true if the WebView the plugin is in renders via the accelerated
  // compositing path.
  bool IsViewAccelerated();

  // Track, set and reset size attributes to control the size of the plugin
  // in and out of fullscreen mode.
  void KeepSizeAttributesBeforeFullscreen();
  void SetSizeAttributesForFullscreen();
  void ResetSizeAttributesAfterFullscreen();

  // Shared code between SetFullscreen() and FlashSetFullscreen().
  bool SetFullscreenCommon(bool fullscreen) const;

  bool IsMouseLocked();
  bool LockMouse(bool request_unadjusted_movement);
  MouseLockDispatcher* GetMouseLockDispatcher();
  MouseLockDispatcher::LockTarget* GetOrCreateLockTargetAdapter();
  void UnSetAndDeleteLockTargetAdapter();

  void DidDataFromWebURLResponse(const blink::WebURLResponse& response,
                                 int pending_host_id,
                                 const ppapi::URLResponseInfoData& data);

  // Converts the PP_Rect between DIP and Viewport.
  void ConvertRectToDIP(PP_Rect* rect) const;
  void ConvertDIPToViewport(gfx::Rect* rect) const;

  // Each time CommitTransferableResource() is called, this instance is given
  // ownership of a texture and gpu::Mailbox. This instance always needs to hold
  // on to the most recently committed texture, since UpdateLayer() might
  // require it. Since it is possible for a gpu::Mailbox to be passed to
  // |texture_layer_| more than once, a reference counting mechanism is
  // necessary to ensure that a texture isn't returned until all copies of
  // it have been released by texture_layer_.
  //
  // This method should be called each time a viz::TransferableResource is
  // passed to |texture_layer_|. It increments an internal reference count.
  void IncrementTextureReferenceCount(
      const viz::TransferableResource& resource);

  // This method should be called each time |texture_layer_| finishes consuming
  // a viz::TransferableResource. It decrements an internal reference count.
  // Returns whether the last reference was removed.
  bool DecrementTextureReferenceCount(
      const viz::TransferableResource& resource);

  // Whether a given viz::TransferableResource is in use by |texture_layer_|.
  bool IsTextureInUse(const viz::TransferableResource& resource) const;

  void HandleAccessibilityChange();

  RenderFrameImpl* render_frame_;
  scoped_refptr<PluginModule> module_;
  std::unique_ptr<ppapi::PPP_Instance_Combined> instance_interface_;
  // If this is the NaCl plugin, we create a new module when we switch to the
  // IPC-based PPAPI proxy. Store the original module and instance interface
  // so we can shut down properly.
  scoped_refptr<PluginModule> original_module_;
  std::unique_ptr<ppapi::PPP_Instance_Combined> original_instance_interface_;

  PP_Instance pp_instance_;

  // These are the scale and the translation that will be applied to the layer.
  gfx::PointF graphics2d_translation_;
  float graphics2d_scale_;

  // NULL until we have been initialized.
  blink::WebPluginContainer* container_;
  scoped_refptr<cc::TextureLayer> texture_layer_;
  bool layer_bound_to_fullscreen_;
  bool layer_is_hardware_;

  // Plugin URL.
  const GURL plugin_url_;

  GURL document_url_;

  // Used to track Flash-specific metrics.
  const bool is_flash_plugin_;

  // Set to true the first time the plugin is clicked. Used to collect metrics.
  bool has_been_clicked_;

  // Responsible for turning on throttling if Power Saver is on.
  std::unique_ptr<PluginInstanceThrottlerImpl> throttler_;

  // Indicates whether this is a full frame instance, which means it represents
  // an entire document rather than an embed tag.
  bool full_frame_;

  // Stores the current state of the plugin view.
  ppapi::ViewData view_data_;
  // The last state sent to the plugin. It is only valid after
  // |sent_initial_did_change_view_| is set to true.
  ppapi::ViewData last_sent_view_data_;
  // The current unobscured portion of the plugin.
  gfx::Rect unobscured_rect_;
  // The viewport coordinates to window coordinates ratio.
  float viewport_to_dip_scale_;

  // Indicates if we've ever sent a didChangeView to the plugin. This ensures we
  // always send an initial notification, even if the position and clip are the
  // same as the default values.
  bool sent_initial_did_change_view_;

  // The current device context for painting in 2D or 3D.
  scoped_refptr<PPB_Graphics3D_Impl> bound_graphics_3d_;
  PepperGraphics2DHost* bound_graphics_2d_platform_;

  // We track two types of focus, one from WebKit, which is the focus among
  // all elements of the page, one one from the browser, which is whether the
  // tab/window has focus. We tell the plugin it has focus only when both of
  // these values are set to true.
  bool has_webkit_focus_;
  bool has_content_area_focus_;

  // The id of the current find operation, or -1 if none is in process.
  int find_identifier_;

  // Helper object that creates resources.
  std::unique_ptr<ppapi::thunk::ResourceCreationAPI> resource_creation_;

  // The plugin-provided interfaces.
  // When adding PPP interfaces, make sure to reset them in ResetAsProxied.
  const PPP_Find_Private* plugin_find_interface_;
  const PPP_InputEvent* plugin_input_event_interface_;
  const PPP_MouseLock* plugin_mouse_lock_interface_;
  const PPP_Pdf* plugin_pdf_interface_;
  const PPP_Instance_Private* plugin_private_interface_;
  const PPP_TextInput_Dev* plugin_textinput_interface_;

  // Flags indicating whether we have asked this plugin instance for the
  // corresponding interfaces, so that we can ask only once.
  // When adding flags, make sure to reset them in ResetAsProxied.
  bool checked_for_plugin_input_event_interface_;
  bool checked_for_plugin_pdf_interface_;

  // This is only valid between a successful PrintBegin call and a PrintEnd
  // call.
  PP_PrintSettings_Dev current_print_settings_;

  // The entire document goes into one metafile. However, it is impossible to
  // know if a call to PrintPage() is the last call. Thus in PrintPage(), just
  // store the page number in |ranges_|. The hack is in PrintEnd(), where a
  // valid |metafile_| is preserved in PrintWebFrameHelper::PrintPages(). This
  // makes it possible to generate the entire PDF given the variables below:
  //
  // The metafile to save into, which is guaranteed to be valid between a
  // successful PrintBegin call and a PrintEnd call.
  printing::MetafileSkia* metafile_;
  // An array of page ranges.
  std::vector<PP_PrintPageNumberRange_Dev> ranges_;

  scoped_refptr<ppapi::Resource> gamepad_impl_;
  scoped_refptr<ppapi::Resource> uma_private_impl_;

  // The plugin print interface.
  const PPP_Printing_Dev* plugin_print_interface_;

  // The plugin 3D interface.
  const PPP_Graphics3D* plugin_graphics_3d_interface_;

  // Contains the cursor if it's set by the plugin.
  std::unique_ptr<blink::WebCursorInfo> cursor_;

  // Set to true if this plugin thinks it will always be on top. This allows us
  // to use a more optimized painting path in some cases.
  bool always_on_top_;

  // Implementation of PPB_FlashFullscreen.

  // Plugin container for fullscreen mode. NULL if not in fullscreen mode. Note:
  // there is a transition state where fullscreen_container_ is non-NULL but
  // flash_fullscreen_ is false (see above).
  FullscreenContainer* fullscreen_container_;

  // True if we are in "flash" fullscreen mode. False if we are in normal mode
  // or in transition to fullscreen. Normal fullscreen mode is indicated in
  // the ViewData.
  bool flash_fullscreen_;

  // Implementation of PPB_Fullscreen.

  // Since entering fullscreen mode is an asynchronous operation, we set this
  // variable to the desired state at the time we issue the fullscreen change
  // request. The plugin will receive a DidChangeView event when it goes
  // fullscreen.
  bool desired_fullscreen_state_;

  // WebKit does not resize the plugin when going into fullscreen mode, so we do
  // this here by modifying the various plugin attributes and then restoring
  // them on exit.
  blink::WebString width_before_fullscreen_;
  blink::WebString height_before_fullscreen_;
  blink::WebString border_before_fullscreen_;
  blink::WebString style_before_fullscreen_;
  gfx::Size screen_size_for_fullscreen_;

  // The MessageChannel used to implement bidirectional postMessage for the
  // instance.
  v8::Persistent<v8::Object> message_channel_object_;

  // A pointer to the MessageChannel underlying |message_channel_object_|. It is
  // only valid as long as |message_channel_object_| is alive.
  MessageChannel* message_channel_;

  // Bitmap for crashed plugin. Lazily initialized.
  cc::PaintImage sad_plugin_image_;

  typedef std::set<PluginObject*> PluginObjectSet;
  PluginObjectSet live_plugin_objects_;

  // Classes of events that the plugin has registered for, both for filtering
  // and not. The bits are PP_INPUTEVENT_CLASS_*.
  uint32_t input_event_mask_;
  uint32_t filtered_input_event_mask_;

  // Text composition status.
  struct TextInputCaretInfo {
    gfx::Rect caret;
    gfx::Rect caret_bounds;
  };
  base::Optional<TextInputCaretInfo> text_input_caret_info_;
  ui::TextInputType text_input_type_;

  // Text selection status.
  std::string surrounding_text_;
  size_t selection_caret_;
  size_t selection_anchor_;

  scoped_refptr<ppapi::TrackedCallback> lock_mouse_callback_;

  // Last mouse position from mouse event, used for calculating movements. Null
  // means no mouse event received yet. This value is updated by
  // |CreateInputEventData|.
  std::unique_ptr<gfx::PointF> last_mouse_position_;

  // We store the arguments so we can re-send them if we are reset to talk to
  // NaCl via the IPC NaCl proxy.
  std::vector<std::string> argn_;
  std::vector<std::string> argv_;

  // Non-owning pointer to the document loader, if any.
  blink::WebAssociatedURLLoaderClient* document_loader_;
  // State for deferring document loads. Used only by external instances.
  blink::WebURLResponse external_document_response_;
  std::unique_ptr<ExternalDocumentLoader> external_document_loader_;
  bool external_document_load_;

  // The link currently under the cursor.
  base::string16 link_under_cursor_;

  // We store the isolate at construction so that we can be sure to use the
  // Isolate in which this Instance was created when interacting with v8.
  v8::Isolate* isolate_;

  std::unique_ptr<MouseLockDispatcher::LockTarget> lock_target_;

  bool is_deleted_;

  // The text that is currently selected in the plugin.
  base::string16 selected_text_;

  // The most recently committed texture. This is kept around in case the layer
  // needs to be regenerated.
  viz::TransferableResource committed_texture_;

  // The Graphics3D that produced the most recently committed texture.
  scoped_refptr<PPB_Graphics3D_Impl> committed_texture_graphics_3d_;

  gpu::SyncToken committed_texture_consumed_sync_token_;

  // Holds the number of references |texture_layer_| has to any given
  // gpu::Mailbox.
  // We expect there to be no more than 10 textures in use at a time. A
  // std::vector will have better performance than a std::map.
  using MailboxRefCount = std::pair<gpu::Mailbox, int>;
  std::vector<MailboxRefCount> texture_ref_counts_;

  bool initialized_;
  bool created_in_process_instance_;

  // The controller for all active audios of this pepper instance.
  std::unique_ptr<PepperAudioController> audio_controller_;

  // We use a weak ptr factory for scheduling DidChangeView events so that we
  // can tell whether updates are pending and consolidate them. When there's
  // already a weak ptr pending (HasWeakPtrs is true), code should update the
  // view_data_ but not send updates. This also allows us to cancel scheduled
  // view change events.
  base::WeakPtrFactory<PepperPluginInstanceImpl> view_change_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<PepperPluginInstanceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperPluginInstanceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_IMPL_H_
