// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_runtime_manager_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "content/browser/xr/service/xr_frame_sink_client_impl.h"
#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "content/browser/xr/webxr_internals/webxr_internals_handler_impl.h"
#include "content/browser/xr/xr_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "ui/gl/gl_switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/xr/service/isolated_device_provider.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {

using XrRuntimeManagerObservers =
    base::ObserverList<XRRuntimeManager::Observer>;

namespace {
XRRuntimeManagerImpl* g_xr_runtime_manager = nullptr;

XrRuntimeManagerObservers& GetXrRuntimeManagerObservers() {
  static base::NoDestructor<XrRuntimeManagerObservers>
      xr_runtime_manager_observers;
  return *xr_runtime_manager_observers;
}

#if !BUILDFLAG(IS_ANDROID)
bool IsEnabled(const base::CommandLine* command_line,
               const base::Feature& feature,
               const std::string& name) {
  if (!command_line->HasSwitch(switches::kWebXrForceRuntime))
    return base::FeatureList::IsEnabled(feature);

  return (base::CompareCaseInsensitiveASCII(
              command_line->GetSwitchValueASCII(switches::kWebXrForceRuntime),
              name) == 0);
}
#endif

bool IsForcedRuntime(const base::CommandLine* command_line,
                     const std::string& name) {
  return (base::CompareCaseInsensitiveASCII(
              command_line->GetSwitchValueASCII(switches::kWebXrForceRuntime),
              name) == 0);
}

std::optional<device::mojom::XRDeviceId> GetForcedRuntime(
    device::mojom::XRSessionMode mode) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kWebXrForceRuntime)) {
    return std::nullopt;
  }

  switch (mode) {
    case device::mojom::XRSessionMode::kImmersiveAr:
#if BUILDFLAG(ENABLE_ARCORE)
      if (IsForcedRuntime(cmd_line, switches::kWebXrRuntimeArCore)) {
        return device::mojom::XRDeviceId::ARCORE_DEVICE_ID;
      }
#endif
#if BUILDFLAG(ENABLE_OPENXR)
      if (IsForcedRuntime(cmd_line, switches::kWebXrRuntimeOpenXr)) {
        return device::mojom::XRDeviceId::OPENXR_DEVICE_ID;
      }
#endif
      break;
    case device::mojom::XRSessionMode::kImmersiveVr:
#if BUILDFLAG(ENABLE_OPENXR)
      if (IsForcedRuntime(cmd_line, switches::kWebXrRuntimeOpenXr)) {
        return device::mojom::XRDeviceId::OPENXR_DEVICE_ID;
      }
#endif
#if BUILDFLAG(ENABLE_CARDBOARD)
      if (IsForcedRuntime(cmd_line, switches::kWebXrRuntimeCardboard)) {
        return device::mojom::XRDeviceId::CARDBOARD_DEVICE_ID;
      }
#endif
      break;
    case device::mojom::XRSessionMode::kInline:
      if (IsForcedRuntime(cmd_line,
                          switches::kWebXrRuntimeOrientationSensors)) {
        return device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID;
      }
      break;
  }

  return device::mojom::XRDeviceId::FAKE_DEVICE_ID;
}

std::unique_ptr<device::XrFrameSinkClient> FrameSinkClientFactory(
    int32_t render_process_id,
    int32_t render_frame_id) {
  // The XrFrameSinkClientImpl needs to be constructed (and destructed) on the
  // main thread. Currently, the only runtime that uses this is ArCore, which
  // runs on the browser main thread (which per comments in
  // content/public/browser/browser_thread.h is also the UI thread).
  DCHECK(GetUIThreadTaskRunner({})->BelongsToCurrentThread())
      << "Must construct XrFrameSinkClient from UI thread";
  return std::make_unique<XrFrameSinkClientImpl>(render_process_id,
                                                 render_frame_id);
}

}  // namespace

// XRRuntimeManager statics
XRRuntimeManager* XRRuntimeManager::GetInstanceIfCreated() {
  return g_xr_runtime_manager;
}

void XRRuntimeManager::AddObserver(XRRuntimeManager::Observer* observer) {
  GetXrRuntimeManagerObservers().AddObserver(observer);
}

void XRRuntimeManager::RemoveObserver(XRRuntimeManager::Observer* observer) {
  GetXrRuntimeManagerObservers().RemoveObserver(observer);
}

void XRRuntimeManager::ExitImmersivePresentation() {
  if (!g_xr_runtime_manager) {
    return;
  }

  auto* browser_xr_runtime =
      g_xr_runtime_manager->GetCurrentlyPresentingImmersiveRuntime();
  if (!browser_xr_runtime) {
    return;
  }

  browser_xr_runtime->ExitActiveImmersiveSession();
}

// Static
scoped_refptr<XRRuntimeManagerImpl>
XRRuntimeManagerImpl::GetOrCreateRuntimeManagerInternal(
    WebContents* web_contents) {
  if (g_xr_runtime_manager) {
    return base::WrapRefCounted(g_xr_runtime_manager);
  }

  // Start by getting any providers specified by the XrIntegrationClient
  XRProviderList providers;
  auto* integration_client = GetXrIntegrationClient();

  if (integration_client) {
    auto additional_providers = integration_client->GetAdditionalProviders();
    providers.insert(providers.end(),
                     make_move_iterator(additional_providers.begin()),
                     make_move_iterator(additional_providers.end()));
  }

  // Then add any other "built-in" providers
#if !BUILDFLAG(IS_ANDROID)
  providers.push_back(std::make_unique<IsolatedVRDeviceProvider>());
#endif  // !BUILDFLAG(IS_ANDROID)

  bool orientation_provider_enabled = true;

#if !BUILDFLAG(IS_ANDROID)
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  orientation_provider_enabled =
      IsEnabled(cmd_line, device::features::kWebXrOrientationSensorDevice,
                ::switches::kWebXrRuntimeOrientationSensors);
#endif

  if (orientation_provider_enabled) {
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    GetDeviceService().BindSensorProvider(
        sensor_provider.InitWithNewPipeAndPassReceiver());
    providers.emplace_back(
        std::make_unique<device::VROrientationDeviceProvider>(
            std::move(sensor_provider)));
  }
  return CreateInstance(std::move(providers), web_contents);
}

scoped_refptr<XRRuntimeManagerImpl> XRRuntimeManagerImpl::GetOrCreateInstance(
    WebContents& web_contents) {
  return GetOrCreateRuntimeManagerInternal(&web_contents);
}

scoped_refptr<XRRuntimeManagerImpl>
XRRuntimeManagerImpl::GetOrCreateInstanceForTesting() {
  return GetOrCreateRuntimeManagerInternal(nullptr);
}

// static
content::WebContents* XRRuntimeManagerImpl::GetImmersiveSessionWebContents() {
  if (!g_xr_runtime_manager)
    return nullptr;
  BrowserXRRuntimeImpl* browser_xr_runtime =
      g_xr_runtime_manager->GetCurrentlyPresentingImmersiveRuntime();
  if (!browser_xr_runtime)
    return nullptr;
  VRServiceImpl* vr_service =
      browser_xr_runtime->GetServiceWithActiveImmersiveSession();
  return vr_service ? vr_service->GetWebContents() : nullptr;
}

void XRRuntimeManagerImpl::AddService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;

  if (AreAllProvidersInitialized())
    service->InitializationComplete();

  services_.insert(service);
}

void XRRuntimeManagerImpl::RemoveService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  services_.erase(service);

  for (const auto& runtime : runtimes_) {
    runtime.second->OnServiceRemoved(service);
  }
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetRuntime(
    device::mojom::XRDeviceId id) {
  auto it = runtimes_.find(id);
  if (it == runtimes_.end())
    return nullptr;

  return it->second.get();
}

content::WebXrLoggerManager& XRRuntimeManagerImpl::GetLoggerManager() {
  return logger_manager_;
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetRuntimeForOptions(
    device::mojom::XRSessionOptions* options) {
  BrowserXRRuntimeImpl* runtime = nullptr;
  switch (options->mode) {
    case device::mojom::XRSessionMode::kImmersiveAr:
      runtime = GetImmersiveArRuntime();
      break;
    case device::mojom::XRSessionMode::kImmersiveVr:
      runtime = GetImmersiveVrRuntime();
      break;
    case device::mojom::XRSessionMode::kInline:
      runtime = GetInlineRuntime();
      break;
  }

  // Return the runtime from above if we got one and it supports all required
  // features.
  return runtime && runtime->SupportsAllFeatures(options->required_features)
             ? runtime
             : nullptr;
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetImmersiveVrRuntime() {
  std::optional<device::mojom::XRDeviceId> maybe_runtime =
      GetForcedRuntime(device::mojom::XRSessionMode::kImmersiveVr);
  if (maybe_runtime.has_value()) {
    return GetRuntime(maybe_runtime.value());
  }

// OpenXR is the highest priority if it's available.
#if BUILDFLAG(ENABLE_OPENXR)
  auto* openxr = GetRuntime(device::mojom::XRDeviceId::OPENXR_DEVICE_ID);
  if (openxr) {
    return openxr;
  }
#endif

#if BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_CARDBOARD)
  auto* cardboard = GetRuntime(device::mojom::XRDeviceId::CARDBOARD_DEVICE_ID);
  if (cardboard) {
    return cardboard;
  }
#endif
#endif

  return nullptr;
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetImmersiveArRuntime() {
  std::optional<device::mojom::XRDeviceId> maybe_runtime =
      GetForcedRuntime(device::mojom::XRSessionMode::kImmersiveAr);
  if (maybe_runtime.has_value()) {
    auto* runtime = GetRuntime(maybe_runtime.value());
    return runtime && runtime->SupportsArBlendMode() ? runtime : nullptr;
  }

#if BUILDFLAG(ENABLE_OPENXR)
  // If OpenXR is available and the runtime supports an AR blend mode, prefer
  // it over ARCore to unify VR/AR rendering paths.
  if (device::features::IsOpenXrArEnabled()) {
    auto* openxr = GetRuntime(device::mojom::XRDeviceId::OPENXR_DEVICE_ID);
    if (openxr && openxr->SupportsArBlendMode())
      return openxr;
  }
#endif

#if BUILDFLAG(ENABLE_ARCORE)
  auto* arcore_runtime =
      GetRuntime(device::mojom::XRDeviceId::ARCORE_DEVICE_ID);
  if (arcore_runtime && arcore_runtime->SupportsArBlendMode()) {
    return arcore_runtime;
  }
#endif

  return nullptr;
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetInlineRuntime() {
  std::optional<device::mojom::XRDeviceId> maybe_runtime =
      GetForcedRuntime(device::mojom::XRSessionMode::kInline);
  if (maybe_runtime.has_value()) {
    return GetRuntime(maybe_runtime.value());
  }

  // Try the orientation provider if it exists.
  // If we don't have an orientation provider, then we don't have an
  // explicit runtime to back a non-immersive session.
  return GetRuntime(device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID);
}

BrowserXRRuntimeImpl*
XRRuntimeManagerImpl::GetCurrentlyPresentingImmersiveRuntime() {
  auto it = base::ranges::find_if(
      runtimes_, [](const DeviceRuntimeMap::value_type& val) {
        return val.second->GetServiceWithActiveImmersiveSession() != nullptr;
      });

  if (it != runtimes_.end()) {
    return it->second.get();
  }

  return nullptr;
}

bool XRRuntimeManagerImpl::HasPendingImmersiveRequest() {
  return base::ranges::any_of(
      runtimes_, [](const DeviceRuntimeMap::value_type& val) {
        return val.second->HasPendingImmersiveSessionRequest();
      });
}

bool XRRuntimeManagerImpl::IsOtherClientPresenting(VRServiceImpl* service) {
  DCHECK(service);

  auto* runtime = GetCurrentlyPresentingImmersiveRuntime();
  if (!runtime)
    return false;  // No immersive runtime to be presenting.

  auto* presenting_service = runtime->GetServiceWithActiveImmersiveSession();

  // True if some other VRServiceImpl is presenting.
  return (presenting_service != service);
}

void XRRuntimeManagerImpl::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::SupportsSessionCallback callback) {
  auto* runtime = GetRuntimeForOptions(options.get());

  if (!runtime) {
    TRACE_EVENT("xr",
                "XRRuntimeManagerImpl::SupportsSession: runtime not found",
                perfetto::Flow::Global(options->trace_id));

    std::move(callback).Run(false);
    return;
  }

  // TODO(http://crbug.com/842025): Pass supports session on to the runtimes.
  std::move(callback).Run(true);
}

void XRRuntimeManagerImpl::MakeXrCompatible() {
  auto* runtime = GetImmersiveVrRuntime();
  if (!runtime)
    runtime = GetImmersiveArRuntime();

  if (!runtime) {
    for (VRServiceImpl* service : services_)
      service->OnMakeXrCompatibleComplete(
          device::mojom::XrCompatibleResult::kNoDeviceAvailable);
    return;
  }

  if (!IsInitializedOnCompatibleAdapter(runtime)) {
#if BUILDFLAG(IS_WIN)
    std::optional<CHROME_LUID> luid = runtime->GetLuid();
    // IsInitializedOnCompatibleAdapter should have returned true if the
    // runtime doesn't specify a LUID.
    DCHECK(luid && (luid->HighPart != 0 || luid->LowPart != 0));

    // Add the XR compatible adapter LUID to the browser command line.
    // GpuProcessHost::LaunchGpuProcess passes this to the GPU process.
    std::string luid_string = base::NumberToString(luid->HighPart) + "," +
                              base::NumberToString(luid->LowPart);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseAdapterLuid, luid_string);

    // Store the current GPU so we can revert back once XR is no longer needed.
    // If default_gpu_ is nonzero, we have already previously stored the
    // default GPU and should not overwrite it.
    if (default_gpu_.LowPart == 0 && default_gpu_.HighPart == 0) {
      default_gpu_ = content::GpuDataManager::GetInstance()
                         ->GetGPUInfo()
                         .active_gpu()
                         .luid;
    }
    xr_compatible_restarted_gpu_ = true;

    // Get notified when the new GPU process sends back its GPUInfo. This
    // indicates that the GPU process has finished initializing and the GPUInfo
    // contains the LUID of the active adapter.
    content::GpuDataManager::GetInstance()->AddObserver(this);

    content::KillGpuProcess();

    return;
#else
    // MakeXrCompatible is not yet supported on other platforms so
    // IsInitializedOnCompatibleAdapter should have returned true.
    NOTREACHED_IN_MIGRATION();
#endif
  }

  for (VRServiceImpl* service : services_)
    service->OnMakeXrCompatibleComplete(
        device::mojom::XrCompatibleResult::kAlreadyCompatible);
}

bool XRRuntimeManagerImpl::IsInitializedOnCompatibleAdapter(
    BrowserXRRuntimeImpl* runtime) {
#if BUILDFLAG(IS_WIN)
  std::optional<CHROME_LUID> luid = runtime->GetLuid();
  if (luid && (luid->HighPart != 0 || luid->LowPart != 0)) {
    CHROME_LUID active_luid =
        content::GpuDataManager::GetInstance()->GetGPUInfo().active_gpu().luid;
    return active_luid.HighPart == luid->HighPart &&
           active_luid.LowPart == luid->LowPart;
  }
#endif

  return true;
}

void XRRuntimeManagerImpl::OnGpuInfoUpdate() {
  content::GpuDataManager::GetInstance()->RemoveObserver(this);

  device::mojom::XrCompatibleResult xr_compatible_result;
  auto* runtime = GetImmersiveVrRuntime();

  if (runtime && IsInitializedOnCompatibleAdapter(runtime)) {
    xr_compatible_result =
        device::mojom::XrCompatibleResult::kCompatibleAfterRestart;
  } else {
    // We can still be incompatible after restarting if either:
    //  1. The runtime has been removed (usually means the VR headset was
    //     unplugged) since the GPU process restart was triggered. Per the WebXR
    //     spec, if there is no device, xr compatible is false.
    //  2. The GPU process is still not using the correct GPU after restarting.
    xr_compatible_result =
        device::mojom::XrCompatibleResult::kNotCompatibleAfterRestart;
  }

  for (VRServiceImpl* service : services_)
    service->OnMakeXrCompatibleComplete(xr_compatible_result);
}

XRRuntimeManagerImpl::XRRuntimeManagerImpl(XRProviderList providers,
                                           WebContents* web_contents)
    : providers_(std::move(providers)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!g_xr_runtime_manager);
  g_xr_runtime_manager = this;
  InitializeProviders(web_contents);
}

XRRuntimeManagerImpl::~XRRuntimeManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(g_xr_runtime_manager, this);
  g_xr_runtime_manager = nullptr;

  // If a GPU adapter LUID was added to the command line to pass to the GPU
  // process, remove the switch so subsequent GPU processes initialize on the
  // default GPU.
  if (xr_compatible_restarted_gpu_) {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kUseAdapterLuid);

#if BUILDFLAG(IS_WIN)
    // If we changed the GPU, revert it back to the default GPU. This is
    // separate from xr_compatible_restarted_gpu_ because the GPU process may
    // not have been successfully initialized using the specified GPU and is
    // still on the default adapter.
    CHROME_LUID active_gpu =
        content::GpuDataManager::GetInstance()->GetGPUInfo().active_gpu().luid;
    if (active_gpu.LowPart != default_gpu_.LowPart ||
        active_gpu.HighPart != default_gpu_.HighPart) {
      content::KillGpuProcess();
    }
#endif
  }
}

scoped_refptr<XRRuntimeManagerImpl> XRRuntimeManagerImpl::CreateInstance(
    XRProviderList providers,
    WebContents* contents) {
  auto* ptr = new XRRuntimeManagerImpl(std::move(providers), contents);
  CHECK_EQ(ptr, g_xr_runtime_manager);
  return base::AdoptRef(ptr);
}

device::mojom::XRRuntime* XRRuntimeManagerImpl::GetRuntimeForTest(
    device::mojom::XRDeviceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeviceRuntimeMap::iterator iter =
      runtimes_.find(static_cast<device::mojom::XRDeviceId>(id));
  if (iter == runtimes_.end())
    return nullptr;

  return iter->second->GetRuntime();
}

size_t XRRuntimeManagerImpl::NumberOfConnectedServices() {
  return services_.size();
}

// The initializing service is available so that providers that need access to
// some aspect of the service, such as the WebContents, to perform
// initialization can do so, but the providers should be initialized in such a
// way that they are not explicitly tied to this service.
void XRRuntimeManagerImpl::InitializeProviders(WebContents* web_contents) {
  if (providers_initialized_)
    return;

  for (const auto& provider : providers_) {
    if (!provider) {
      // TODO(crbug.com/40673158): Remove this logging after investigation.
      LOG(ERROR) << __func__ << " got null XR provider";
      continue;
    }

    // It is acceptable for the providers to potentially take/keep a reference
    // to ourselves here, since we own the providers and can guarantee that they
    // will not outlive us. Providers should not take a long-term reference to
    // the WebContents.
    provider->Initialize(this, web_contents);
  }

  providers_initialized_ = true;
}

void XRRuntimeManagerImpl::OnProviderInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++num_initialized_providers_;
  if (AreAllProvidersInitialized()) {
    for (VRServiceImpl* service : services_)
      service->InitializationComplete();
  }
}

bool XRRuntimeManagerImpl::AreAllProvidersInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return num_initialized_providers_ == providers_.size();
}

void XRRuntimeManagerImpl::AddRuntime(
    device::mojom::XRDeviceId id,
    device::mojom::XRDeviceDataPtr device_data,
    mojo::PendingRemote<device::mojom::XRRuntime> runtime) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(runtimes_.find(id) == runtimes_.end());

  TRACE_EVENT_INSTANT1("xr", "AddRuntime", TRACE_EVENT_SCOPE_THREAD, "id", id);

  webxr::mojom::RuntimeInfoPtr runtime_added_record =
      webxr::mojom::RuntimeInfo::New();
  runtime_added_record->device_id = id;
  runtime_added_record->supported_features = device_data->supported_features;
  runtime_added_record->is_ar_blend_mode_supported =
      device_data->is_ar_blend_mode_supported;
  GetLoggerManager().RecordRuntimeAdded(std::move(runtime_added_record));

  runtimes_[id] = std::make_unique<BrowserXRRuntimeImpl>(
      id, std::move(device_data), std::move(runtime));

  for (Observer& obs : GetXrRuntimeManagerObservers()) {
    obs.OnRuntimeAdded(runtimes_[id].get());
  }

  for (VRServiceImpl* service : services_) {
    // TODO(sumankancherla): Consider combining with XRRuntimeManager::Observer.
    service->RuntimesChanged();
    runtimes_[id]->OnServiceAdded(service);
  }
}

void XRRuntimeManagerImpl::RemoveRuntime(device::mojom::XRDeviceId id) {
  DVLOG(1) << __func__ << " id: " << id;
  TRACE_EVENT_INSTANT1("xr", "RemoveRuntime", TRACE_EVENT_SCOPE_THREAD, "id",
                       id);

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = runtimes_.find(id);
  CHECK(it != runtimes_.end(), base::NotFatalUntil::M130);

  GetLoggerManager().RecordRuntimeRemoved(id);

  // Give the runtime a chance to clean itself up before notifying services
  // that it was removed.
  it->second->BeforeRuntimeRemoved();

  // Remove the runtime from runtimes_ before notifying services that it was
  // removed, since they will query for runtimes in RuntimesChanged.
  std::unique_ptr<BrowserXRRuntimeImpl> removed_runtime = std::move(it->second);
  runtimes_.erase(it);

  for (VRServiceImpl* service : services_)
    service->RuntimesChanged();
}

device::XrFrameSinkClientFactory
XRRuntimeManagerImpl::GetXrFrameSinkClientFactory() {
  return base::BindRepeating(&FrameSinkClientFactory);
}

std::vector<webxr::mojom::RuntimeInfoPtr>
XRRuntimeManagerImpl::GetActiveRuntimes() {
  std::vector<webxr::mojom::RuntimeInfoPtr> active_runtimes;
  for (auto& runtime : runtimes_) {
    webxr::mojom::RuntimeInfoPtr runtime_info =
        webxr::mojom::RuntimeInfo::New();
    runtime_info->device_id = runtime.first;
    runtime_info->supported_features = runtime.second->GetSupportedFeatures();
    runtime_info->is_ar_blend_mode_supported =
        runtime.second->SupportsArBlendMode();

    active_runtimes.push_back(std::move(runtime_info));
  }
  return active_runtimes;
}

}  // namespace content
