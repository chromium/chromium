// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_module.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/nacl/common/buildflags.h"
#include "content/common/frame_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/host_dispatcher_wrapper.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_hung_plugin_filter.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/pepper/ppapi_preferences_builder.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/pepper/ppb_proxy_impl.h"
#include "content/renderer/pepper/ppb_var_deprecated_impl.h"
#include "content/renderer/pepper/ppb_video_decoder_impl.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_audio_output_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_audio_encoder.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_camera_capabilities_private.h"
#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_find_private.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_drm.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_flash_message_loop.h"
#include "ppapi/c/private/ppb_flash_print.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/ppb_opengles2_shared.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::InputEventData;
using ppapi::PpapiGlobals;
using ppapi::TimeTicksToPPTimeTicks;
using ppapi::TimeToPPTime;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_InputEvent_API;

namespace content {

namespace {

// Global tracking info for PPAPI plugins. This is lazily created before the
// first plugin is allocated, and leaked on shutdown.
//
// Note that we don't want a Singleton here since destroying this object will
// try to free some stuff that requires WebKit, and Singletons are destroyed
// after WebKit.
// TODO(raymes): I'm not sure if it is completely necessary to leak the
// HostGlobals. Figure out the shutdown sequence and find a way to do this
// more elegantly.
HostGlobals* host_globals = nullptr;

// Maintains all currently loaded plugin libs for validating PP_Module
// identifiers.
typedef std::set<PluginModule*> PluginModuleSet;

PluginModuleSet* GetLivePluginSet() {
  static base::NoDestructor<PluginModuleSet> live_plugin_libs;
  return live_plugin_libs.get();
}

class PowerSaverTestPluginDelegate : public PluginInstanceThrottler::Observer {
 public:
  explicit PowerSaverTestPluginDelegate(PluginInstanceThrottlerImpl* throttler)
      : throttler_(throttler) {
    throttler_->AddObserver(this);
    PostPowerSaverStatusToJavaScript("initial");
  }

  virtual ~PowerSaverTestPluginDelegate() { throttler_->RemoveObserver(this); }

  static void PostPowerSaverStatusToJavaScript(
      PepperPluginInstanceImpl* instance,
      const std::string& source) {
    DCHECK(instance);

    bool is_hidden_for_placeholder = false;
    bool is_peripheral = false;
    bool is_throttled = false;

    if (instance->throttler()) {
      PluginInstanceThrottlerImpl* throttler = instance->throttler();
      is_hidden_for_placeholder = throttler->IsHiddenForPlaceholder();
      is_peripheral = throttler->power_saver_enabled();
      is_throttled = throttler->IsThrottled();
    }

    // Refcounted by the returned PP_Var.
    ppapi::DictionaryVar* dictionary = new ppapi::DictionaryVar;
    dictionary->Set(ppapi::StringVar::StringToPPVar("source"),
                    ppapi::StringVar::StringToPPVar(source));
    dictionary->Set(ppapi::StringVar::StringToPPVar("isHiddenForPlaceholder"),
                    PP_MakeBool(PP_FromBool(is_hidden_for_placeholder)));
    dictionary->Set(ppapi::StringVar::StringToPPVar("isPeripheral"),
                    PP_MakeBool(PP_FromBool(is_peripheral)));
    dictionary->Set(ppapi::StringVar::StringToPPVar("isThrottled"),
                    PP_MakeBool(PP_FromBool(is_throttled)));

    instance->PostMessageToJavaScript(dictionary->GetPPVar());
  }

 private:
  void OnThrottleStateChange() override {
    PostPowerSaverStatusToJavaScript("throttleStatusChange");
  }

  void OnPeripheralStateChange() override {
    PostPowerSaverStatusToJavaScript("peripheralStatusChange");
  }

  void OnHiddenForPlaceholder(bool hidden) override {
    PostPowerSaverStatusToJavaScript("hiddenForPlaceholderStatusChange");
  }

  void OnThrottlerDestroyed() override { delete this; }

  void PostPowerSaverStatusToJavaScript(const std::string& source) {
    if (!throttler_->GetWebPlugin() || !throttler_->GetWebPlugin()->instance())
      return;
    PostPowerSaverStatusToJavaScript(throttler_->GetWebPlugin()->instance(),
                                     source);
  }

  // Non-owning pointer.
  PluginInstanceThrottlerImpl* const throttler_;
};

// PPB_Core --------------------------------------------------------------------

void AddRefResource(PP_Resource resource) {
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(resource);
}

PP_Time GetTime() { return TimeToPPTime(base::Time::Now()); }

PP_TimeTicks GetTickTime() {
  return TimeTicksToPPTimeTicks(base::TimeTicks::Now());
}

void CallOnMainThread(int delay_in_msec,
                      PP_CompletionCallback callback,
                      int32_t result) {
  if (callback.func) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostDelayedTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data, result),
        base::TimeDelta::FromMilliseconds(delay_in_msec));
  }
}

PP_Bool IsMainThread() {
  return PP_FromBool(PpapiGlobals::Get()
                         ->GetMainThreadMessageLoop()
                         ->BelongsToCurrentThread());
}

const PPB_Core core_interface = {&AddRefResource,   &ReleaseResource,
                                 &GetTime,          &GetTickTime,
                                 &CallOnMainThread, &IsMainThread};

// PPB_Testing -----------------------------------------------------------------

PP_Bool ReadImageData(PP_Resource device_context_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  EnterResource<PPB_Graphics2D_API> enter(device_context_2d, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(enter.object()->ReadImageData(image, top_left));
}

void RunMessageLoop(PP_Instance instance) {
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).Run();
}

void QuitMessageLoop(PP_Instance instance) {
  base::RunLoop::QuitCurrentDeprecated();
}

uint32_t GetLiveObjectsForInstance(PP_Instance instance_id) {
  return HostGlobals::Get()->GetResourceTracker()->GetLiveObjectsForInstance(
      instance_id);
}

PP_Bool IsOutOfProcess() { return PP_FALSE; }

void PostPowerSaverStatus(PP_Instance instance_id) {
  PepperPluginInstanceImpl* plugin_instance =
      host_globals->GetInstance(instance_id);
  if (!plugin_instance)
    return;

  PowerSaverTestPluginDelegate::PostPowerSaverStatusToJavaScript(
      plugin_instance, "getPowerSaverStatusResponse");
}

void SubscribeToPowerSaverNotifications(PP_Instance instance_id) {
  PepperPluginInstanceImpl* plugin_instance =
      host_globals->GetInstance(instance_id);
  if (!plugin_instance)
    return;

  if (plugin_instance->throttler()) {
    // Manages its own lifetime.
    new PowerSaverTestPluginDelegate(plugin_instance->throttler());
  } else {
    // Just send an initial status. This status will never be updated.
    PowerSaverTestPluginDelegate::PostPowerSaverStatusToJavaScript(
        plugin_instance, "initial");
  }
}

void SimulateInputEvent(PP_Instance instance, PP_Resource input_event) {
  PepperPluginInstanceImpl* plugin_instance =
      host_globals->GetInstance(instance);
  if (!plugin_instance)
    return;

  EnterResource<PPB_InputEvent_API> enter(input_event, false);
  if (enter.failed())
    return;

  const InputEventData& input_event_data = enter.object()->GetInputEventData();
  plugin_instance->SimulateInputEvent(input_event_data);
}

PP_Var GetDocumentURL(PP_Instance instance, PP_URLComponents_Dev* components) {
  PepperPluginInstanceImpl* plugin_instance =
      host_globals->GetInstance(instance);
  if (!plugin_instance)
    return PP_MakeUndefined();
  return plugin_instance->GetDocumentURL(instance, components);
}

uint32_t GetLiveVars(PP_Var live_vars[], uint32_t array_size) {
  std::vector<PP_Var> vars =
      PpapiGlobals::Get()->GetVarTracker()->GetLiveVars();
  for (size_t i = 0u;
       i < std::min(static_cast<size_t>(array_size), vars.size());
       ++i)
    live_vars[i] = vars[i];
  return vars.size();
}

void SetMinimumArrayBufferSizeForShmem(PP_Instance /*instance*/,
                                       uint32_t /*threshold*/) {
  // Does nothing. Not needed in-process.
}

void RunV8GC(PP_Instance pp_instance) {
  PepperPluginInstanceImpl* instance =
      content::PepperPluginInstanceImpl::GetForTesting(pp_instance);
  instance->GetIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

const PPB_Testing_Private testing_interface = {
    &ReadImageData,
    &RunMessageLoop,
    &QuitMessageLoop,
    &GetLiveObjectsForInstance,
    &IsOutOfProcess,
    &PostPowerSaverStatus,
    &SubscribeToPowerSaverNotifications,
    &SimulateInputEvent,
    &GetDocumentURL,
    &GetLiveVars,
    &SetMinimumArrayBufferSizeForShmem,
    &RunV8GC};

// GetInterface ----------------------------------------------------------------

const void* InternalGetInterface(const char* name) {
// TODO(brettw) put these in a hash map for better performance.
#define PROXIED_IFACE(iface_str, iface_struct) \
  if (strcmp(name, iface_str) == 0)            \
    return ppapi::thunk::Get##iface_struct##_Thunk();

#include "ppapi/thunk/interfaces_ppb_private.h"
#include "ppapi/thunk/interfaces_ppb_private_flash.h"
#include "ppapi/thunk/interfaces_ppb_private_no_permissions.h"
#include "ppapi/thunk/interfaces_ppb_private_pdf.h"
#include "ppapi/thunk/interfaces_ppb_public_dev.h"
#include "ppapi/thunk/interfaces_ppb_public_dev_channel.h"
#include "ppapi/thunk/interfaces_ppb_public_socket.h"
#include "ppapi/thunk/interfaces_ppb_public_stable.h"

#undef PROXIED_IFACE

#define LEGACY_IFACE(iface_str, function_name) \
  if (strcmp(name, iface_str) == 0)            \
    return function_name;

#include "ppapi/thunk/interfaces_legacy.h"

#undef LEGACY_IFACE

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    if (strcmp(name, PPB_TESTING_PRIVATE_INTERFACE) == 0)
      return &testing_interface;
  }
  return nullptr;
}

const void* GetInterface(const char* name) {
  // All interfaces should be used on the main thread.
  CHECK(IsMainThread());

  return InternalGetInterface(name);
}

// Gets the PPAPI entry points from the given library and places them into the
// given structure. Returns true on success.
bool LoadEntryPointsFromLibrary(const base::NativeLibrary& library,
                                PepperPluginInfo::EntryPoints* entry_points) {
  entry_points->get_interface =
      reinterpret_cast<PepperPluginInfo::GetInterfaceFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_GetInterface"));
  if (!entry_points->get_interface) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return false;
  }

  entry_points->initialize_module =
      reinterpret_cast<PepperPluginInfo::PPP_InitializeModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_InitializeModule"));
  if (!entry_points->initialize_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return false;
  }

  // It's okay for PPP_ShutdownModule to not be defined and shutdown_module to
  // be NULL.
  entry_points->shutdown_module =
      reinterpret_cast<PepperPluginInfo::PPP_ShutdownModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_ShutdownModule"));

  return true;
}

void CreateHostForInProcessModule(RenderFrameImpl* render_frame,
                                  PluginModule* module,
                                  const WebPluginInfo& webplugin_info) {
  // First time an in-process plugin was used, make a host for it.
  const PepperPluginInfo* info =
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(webplugin_info);
  DCHECK(!info->is_out_of_process);

  ppapi::PpapiPermissions perms(PepperPluginRegistry::GetInstance()
                                    ->GetInfoForPlugin(webplugin_info)
                                    ->permissions);
  RendererPpapiHostImpl* host_impl =
      RendererPpapiHostImpl::CreateOnModuleForInProcess(module, perms);
  render_frame->PepperPluginCreated(host_impl);
}

}  // namespace

// PluginModule ----------------------------------------------------------------

PluginModule::PluginModule(const std::string& name,
                           const std::string& version,
                           const base::FilePath& path,
                           const ppapi::PpapiPermissions& perms)
    : callback_tracker_(new ppapi::CallbackTracker),
      is_in_destructor_(false),
      is_crashed_(false),
      broker_(nullptr),
      library_(nullptr),
      name_(name),
      version_(version),
      path_(path),
      permissions_(ppapi::PpapiPermissions::GetForCommandLine(perms.GetBits())),
      reserve_instance_id_(nullptr) {
  // Ensure the globals object is created.
  if (!host_globals)
    host_globals = new HostGlobals;

  memset(&entry_points_, 0, sizeof(entry_points_));
  pp_module_ = HostGlobals::Get()->AddModule(this);
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // In the past there have been crashes reentering the plugin module
  // destructor. Catch if that happens again earlier.
  CHECK(!is_in_destructor_);
  is_in_destructor_ = true;

  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  // Some resources and other stuff are hung off of the embedder state, which
  // should be torn down before the routing stuff below.
  renderer_ppapi_host_.reset();

  GetLivePluginSet()->erase(this);

  callback_tracker_->AbortAll();

  if (entry_points_.shutdown_module)
    entry_points_.shutdown_module();

  if (library_)
    base::UnloadNativeLibrary(library_);

  // Notifications that we've been deleted should be last.
  HostGlobals::Get()->ModuleDeleted(pp_module_);
  if (!is_crashed_) {
    // When the plugin crashes, we immediately tell the lifetime delegate that
    // we're gone, so we don't want to tell it again.
    PepperPluginRegistry::GetInstance()->PluginModuleDead(this);
  }

  // Don't add stuff here, the two notifications that the module object has
  // been deleted should be last. This allows, for example,
  // PPB_Proxy.IsInModuleDestructor to map PP_Module to this class during the
  // previous parts of the destructor.
}

void PluginModule::SetRendererPpapiHost(
    std::unique_ptr<RendererPpapiHostImpl> host) {
  renderer_ppapi_host_ = std::move(host);
}

bool PluginModule::InitAsInternalPlugin(
    const PepperPluginInfo::EntryPoints& entry_points) {
  if (InitializeModule(entry_points)) {
    entry_points_ = entry_points;
    return true;
  }
  return false;
}

bool PluginModule::InitAsLibrary(const base::FilePath& path) {
  base::NativeLibrary library = base::LoadNativeLibrary(path, nullptr);
  if (!library)
    return false;

  PepperPluginInfo::EntryPoints entry_points;

  if (!LoadEntryPointsFromLibrary(library, &entry_points) ||
      !InitializeModule(entry_points)) {
    base::UnloadNativeLibrary(library);
    return false;
  }
  entry_points_ = entry_points;
  library_ = library;
  return true;
}

void PluginModule::InitAsProxied(
    HostDispatcherWrapper* host_dispatcher_wrapper) {
  DCHECK(!host_dispatcher_wrapper_.get());
  host_dispatcher_wrapper_.reset(host_dispatcher_wrapper);
}

scoped_refptr<PluginModule>
PluginModule::CreateModuleForExternalPluginInstance() {
  // Create a new module, but don't set the lifetime delegate. This isn't a
  // plugin in the usual sense, so it isn't tracked by the browser.
  scoped_refptr<PluginModule> external_plugin_module(
      new PluginModule(name_, version_, path_, permissions_));
  return external_plugin_module;
}

PP_ExternalPluginResult PluginModule::InitAsProxiedExternalPlugin(
    PepperPluginInstanceImpl* instance) {
  DCHECK(host_dispatcher_wrapper_.get());
  // InitAsProxied (for the trusted/out-of-process case) initializes only the
  // module, and one or more instances are added later. In this case, the
  // PluginInstance was already created as in-process, so we missed the proxy
  // AddInstance step and must do it now.
  host_dispatcher_wrapper_->AddInstance(instance->pp_instance());
  // For external plugins, we need to tell the instance to reset itself as
  // proxied. This will clear cached interface pointers and send DidCreate (etc)
  // to the plugin side of the proxy.
  return instance->ResetAsProxied(this);
}

bool PluginModule::IsProxied() const { return !!host_dispatcher_wrapper_; }

base::ProcessId PluginModule::GetPeerProcessId() {
  if (host_dispatcher_wrapper_)
    return host_dispatcher_wrapper_->peer_pid();
  return base::kNullProcessId;
}

int PluginModule::GetPluginChildId() {
  if (host_dispatcher_wrapper_)
    return host_dispatcher_wrapper_->plugin_child_id();
  return 0;
}

// static
const PPB_Core* PluginModule::GetCore() { return &core_interface; }

// static
bool PluginModule::SupportsInterface(const char* name) {
  return !!InternalGetInterface(name);
}

PepperPluginInstanceImpl* PluginModule::CreateInstance(
    RenderFrameImpl* render_frame,
    blink::WebPluginContainer* container,
    const GURL& plugin_url) {
  PepperPluginInstanceImpl* instance = PepperPluginInstanceImpl::Create(
      render_frame, this, container, plugin_url);
  if (!instance) {
    LOG(WARNING) << "Plugin doesn't support instance interface, failing.";
    return nullptr;
  }
  if (host_dispatcher_wrapper_)
    host_dispatcher_wrapper_->AddInstance(instance->pp_instance());
  return instance;
}

PepperPluginInstanceImpl* PluginModule::GetSomeInstance() const {
  // This will generally crash later if there is not actually any instance to
  // return, so we force a crash now to make bugs easier to track down.
  CHECK(!instances_.empty());
  return *instances_.begin();
}

const void* PluginModule::GetPluginInterface(const char* name) const {
  if (host_dispatcher_wrapper_)
    return host_dispatcher_wrapper_->GetProxiedInterface(name);

  // In-process plugins.
  if (!entry_points_.get_interface)
    return nullptr;
  return entry_points_.get_interface(name);
}

void PluginModule::InstanceCreated(PepperPluginInstanceImpl* instance) {
  instances_.insert(instance);
}

void PluginModule::InstanceDeleted(PepperPluginInstanceImpl* instance) {
  if (host_dispatcher_wrapper_)
    host_dispatcher_wrapper_->RemoveInstance(instance->pp_instance());
  instances_.erase(instance);
}

scoped_refptr<ppapi::CallbackTracker> PluginModule::GetCallbackTracker() {
  return callback_tracker_;
}

void PluginModule::PluginCrashed() {
  DCHECK(!is_crashed_);  // Should only get one notification.
  is_crashed_ = true;

  // Notify all instances that they crashed.
  for (auto i = instances_.begin(); i != instances_.end(); ++i)
    (*i)->InstanceCrashed();

  PepperPluginRegistry::GetInstance()->PluginModuleDead(this);
}

void PluginModule::SetReserveInstanceIDCallback(
    PP_Bool (*reserve)(PP_Module, PP_Instance)) {
  DCHECK(!reserve_instance_id_) << "Only expect one set.";
  reserve_instance_id_ = reserve;
}

bool PluginModule::ReserveInstanceID(PP_Instance instance) {
  if (reserve_instance_id_)
    return PP_ToBool(reserve_instance_id_(pp_module_, instance));
  return true;  // Instance ID is usable.
}

void PluginModule::SetBroker(PepperBroker* broker) {
  DCHECK(!broker_ || !broker);
  broker_ = broker;
}

PepperBroker* PluginModule::GetBroker() { return broker_; }

RendererPpapiHostImpl* PluginModule::CreateOutOfProcessModule(
    RenderFrameImpl* render_frame,
    const base::FilePath& path,
    ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId peer_pid,
    int plugin_child_id,
    bool is_external,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<PepperHungPluginFilter> hung_filter(new PepperHungPluginFilter(
      path, render_frame->GetRoutingID(), plugin_child_id));
  std::unique_ptr<HostDispatcherWrapper> dispatcher(new HostDispatcherWrapper(
      this, peer_pid, plugin_child_id, permissions, is_external));

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return nullptr;
  scoped_refptr<gpu::GpuChannelHost> channel =
      render_thread->EstablishGpuChannelSync();
  // If no channel is established, feature statuses are unknown and disabled.
  const gpu::GpuFeatureInfo default_gpu_feature_info;
  const gpu::GpuFeatureInfo& gpu_feature_info =
      channel ? channel->gpu_feature_info() : default_gpu_feature_info;

  if (!dispatcher->Init(channel_handle, &GetInterface,
                        ppapi::Preferences(PpapiPreferencesBuilder::Build(
                            render_frame->render_view()->webkit_preferences(),
                            gpu_feature_info)),
                        hung_filter.get(), task_runner)) {
    return nullptr;
  }

  RendererPpapiHostImpl* host_impl =
      RendererPpapiHostImpl::CreateOnModuleForOutOfProcess(
          this, dispatcher->dispatcher(), permissions);
  render_frame->PepperPluginCreated(host_impl);

  InitAsProxied(dispatcher.release());
  return host_impl;
}

// static
void PluginModule::ResetHostGlobalsForTest() {
  delete host_globals;
  host_globals = nullptr;
}

bool PluginModule::InitializeModule(
    const PepperPluginInfo::EntryPoints& entry_points) {
  DCHECK(!host_dispatcher_wrapper_.get()) << "Don't call for proxied modules.";
  DCHECK(entry_points.initialize_module != nullptr);
  int retval = entry_points.initialize_module(pp_module(), &GetInterface);
  if (retval != 0) {
#if BUILDFLAG(ENABLE_NACL)
    LOG(WARNING) << "PPP_InitializeModule returned failure " << retval;
#endif  // BUILDFLAG(ENABLE_NACL)
    return false;
  }
  return true;
}

scoped_refptr<PluginModule> PluginModule::Create(
    RenderFrameImpl* render_frame,
    const WebPluginInfo& webplugin_info,
    const base::Optional<url::Origin>& origin_lock,
    bool* pepper_plugin_was_registered,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  *pepper_plugin_was_registered = true;

  // See if a module has already been loaded for this plugin.
  base::FilePath path(webplugin_info.path);
  scoped_refptr<PluginModule> module =
      PepperPluginRegistry::GetInstance()->GetLiveModule(path, origin_lock);
  if (module.get()) {
    if (!module->renderer_ppapi_host()) {
      // If the module exists and no embedder state was associated with it,
      // then the module was one of the ones preloaded and is an in-process
      // plugin. We need to associate our host state with it.
      CreateHostForInProcessModule(render_frame, module.get(), webplugin_info);
    }
    return module;
  }

  // In-process plugins will have always been created up-front to avoid the
  // sandbox restrictions. So getting here implies it doesn't exist or should
  // be out of process.
  const PepperPluginInfo* info =
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(webplugin_info);
  if (!info) {
    *pepper_plugin_was_registered = false;
    return scoped_refptr<PluginModule>();
  } else if (!info->is_out_of_process) {
    // In-process plugin not preloaded, it probably couldn't be initialized.
    return scoped_refptr<PluginModule>();
  }

  // Out of process: have the browser start the plugin process for us.
  IPC::ChannelHandle channel_handle;
  base::ProcessId peer_pid = 0;
  int plugin_child_id = 0;
  render_frame->Send(new FrameHostMsg_OpenChannelToPepperPlugin(
      path, origin_lock, &channel_handle, &peer_pid, &plugin_child_id));
  if (!channel_handle.is_mojo_channel_handle()) {
    // Couldn't be initialized.
    return scoped_refptr<PluginModule>();
  }

  ppapi::PpapiPermissions permissions(info->permissions);

  // AddLiveModule must be called before any early returns since the
  // module's destructor will remove itself.
  module = new PluginModule(info->name, info->version, path, permissions);
  PepperPluginRegistry::GetInstance()->AddLiveModule(path, origin_lock,
                                                     module.get());

  if (!module->CreateOutOfProcessModule(render_frame, path, permissions,
                                        channel_handle, peer_pid,
                                        plugin_child_id, false,
                                        task_runner))  // is_external = false
    return scoped_refptr<PluginModule>();

  return module;
}

}  // namespace content
