// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/memory_stats.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

namespace {

bool g_initialized = false;

// Implementation of the Blink GpuDataManager interface to forward requests from
// a renderer to the GpuDataManagerImpl.
class GpuDataManagerReceiver : public blink::mojom::GpuDataManager {
 public:
  GpuDataManagerReceiver() = default;
  GpuDataManagerReceiver(const GpuDataManagerReceiver&) = delete;
  GpuDataManagerReceiver& operator=(const GpuDataManagerReceiver&) = delete;
  ~GpuDataManagerReceiver() override = default;

  void Bind(mojo::PendingReceiver<blink::mojom::GpuDataManager> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // blink::mojom::GpuDataManager:
  void Are3DAPIsBlockedForUrl(
      const GURL& url,
      Are3DAPIsBlockedForUrlCallback callback) override {
    auto* manager = GpuDataManagerImpl::GetInstance();
    if (!manager) {
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(
        manager->Are3DAPIsBlocked(url, THREE_D_API_TYPE_WEBGL));
  }

 private:
  mojo::ReceiverSet<blink::mojom::GpuDataManager> receivers_;
};

GpuDataManagerReceiver& GetGpuDataManagerReceiver() {
  static base::NoDestructor<GpuDataManagerReceiver> receiver;
  return *receiver.get();
}

}  // namespace

// static
GpuDataManager* GpuDataManager::GetInstance() {
  return GpuDataManagerImpl::GetInstance();
}

// static
bool GpuDataManager::Initialized() {
  return GpuDataManagerImpl::Initialized();
}

// static
GpuDataManagerImpl* GpuDataManagerImpl::GetInstance() {
  static base::NoDestructor<GpuDataManagerImpl> instance;
  return instance.get();
}

// static
bool GpuDataManagerImpl::Initialized() {
  return g_initialized;
}

void GpuDataManagerImpl::BlocklistWebGLForTesting() {
  base::AutoLock auto_lock(lock_);
  private_->BlocklistWebGLForTesting();  // IN-TEST
}

void GpuDataManagerImpl::SetSkiaGraphiteEnabledForTesting(bool enabled) {
  base::AutoLock auto_lock(lock_);
  private_->SetSkiaGraphiteEnabledForTesting(enabled);  // IN-TEST
}

gpu::GPUInfo GpuDataManagerImpl::GetGPUInfo() {
  base::AutoLock auto_lock(lock_);
  return private_->GetGPUInfo();
}

gpu::GpuFeatureStatus GpuDataManagerImpl::GetFeatureStatus(
    gpu::GpuFeatureType feature) {
  base::AutoLock auto_lock(lock_);
  return private_->GetFeatureStatus(feature);
}

bool GpuDataManagerImpl::GpuAccessAllowed(std::string* reason) {
  base::AutoLock auto_lock(lock_);
  return private_->GpuAccessAllowed(reason);
}

void GpuDataManagerImpl::RequestDx12VulkanVideoGpuInfoIfNeeded(
    GpuInfoRequest request,
    bool delayed) {
  base::AutoLock auto_lock(lock_);
  private_->RequestDx12VulkanVideoGpuInfoIfNeeded(request, delayed);
}

bool GpuDataManagerImpl::IsEssentialGpuInfoAvailable() {
  base::AutoLock auto_lock(lock_);
  return private_->IsEssentialGpuInfoAvailable();
}

bool GpuDataManagerImpl::IsDx12VulkanVersionAvailable() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsDx12VulkanVersionAvailable();
}

bool GpuDataManagerImpl::IsGpuFeatureInfoAvailable() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuFeatureInfoAvailable();
}

void GpuDataManagerImpl::RequestVideoMemoryUsageStatsUpdate(
    VideoMemoryUsageStatsCallback callback) {
  base::AutoLock auto_lock(lock_);
  private_->RequestVideoMemoryUsageStatsUpdate(std::move(callback));
}

void GpuDataManagerImpl::AddObserver(GpuDataManagerObserver* observer) {
  base::AutoLock auto_lock(lock_);
  private_->AddObserver(observer);
}

void GpuDataManagerImpl::RemoveObserver(GpuDataManagerObserver* observer) {
  base::AutoLock auto_lock(lock_);
  private_->RemoveObserver(observer);
}

void GpuDataManagerImpl::DisableHardwareAcceleration() {
  base::AutoLock auto_lock(lock_);
  private_->DisableHardwareAcceleration();
}

bool GpuDataManagerImpl::HardwareAccelerationEnabled() {
  base::AutoLock auto_lock(lock_);
  return private_->HardwareAccelerationEnabled();
}

void GpuDataManagerImpl::AppendGpuCommandLine(base::CommandLine* command_line,
                                              GpuProcessKind kind) {
  base::AutoLock auto_lock(lock_);
  private_->AppendGpuCommandLine(command_line, kind);
}

void GpuDataManagerImpl::StartUmaTimer() {
  base::AutoLock auto_lock(lock_);
  private_->StartUmaTimer();
}

void GpuDataManagerImpl::UpdateGpuInfo(
    const gpu::GPUInfo& gpu_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuInfo(gpu_info, gpu_info_for_hardware_gpu);
}

#if BUILDFLAG(IS_WIN)

void GpuDataManagerImpl::UpdateDirectXInfo(uint32_t d3d12_feature_level,
                                           uint32_t directml_feature_level) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDirectXInfo(d3d12_feature_level, directml_feature_level);
}

void GpuDataManagerImpl::UpdateVulkanInfo(uint32_t vulkan_version) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateVulkanInfo(vulkan_version);
}

void GpuDataManagerImpl::UpdateDevicePerfInfo(
    const gpu::DevicePerfInfo& device_perf_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDevicePerfInfo(device_perf_info);
}

void GpuDataManagerImpl::UpdateOverlayInfo(
    const gpu::OverlayInfo& overlay_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateOverlayInfo(overlay_info);
}
void GpuDataManagerImpl::UpdateDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDXGIInfo(std::move(dxgi_info));
}

void GpuDataManagerImpl::UpdateDirectXRequestStatus(bool request_continues) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDirectXRequestStatus(request_continues);
}

void GpuDataManagerImpl::UpdateVulkanRequestStatus(bool request_continues) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateVulkanRequestStatus(request_continues);
}

bool GpuDataManagerImpl::DirectXRequested() const {
  base::AutoLock auto_lock(lock_);
  return private_->DirectXRequested();
}

bool GpuDataManagerImpl::VulkanRequested() const {
  base::AutoLock auto_lock(lock_);
  return private_->VulkanRequested();
}

void GpuDataManagerImpl::TerminateInfoCollectionGpuProcess() {
  base::AutoLock auto_lock(lock_);
  private_->TerminateInfoCollectionGpuProcess();
}
#endif  // BUILDFLAG(IS_WIN)

void GpuDataManagerImpl::PostCreateThreads() {
  base::AutoLock auto_lock(lock_);
  private_->PostCreateThreads();
}

void GpuDataManagerImpl::UpdateDawnInfo(
    const std::vector<std::string>& dawn_info_list) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDawnInfo(dawn_info_list);
}

void GpuDataManagerImpl::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuFeatureInfo(gpu_feature_info,
                                 gpu_feature_info_for_hardware_gpu);
}

void GpuDataManagerImpl::UpdateGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuExtraInfo(gpu_extra_info);
}

void GpuDataManagerImpl::UpdateMojoMediaVideoDecoderCapabilities(
    const media::SupportedVideoDecoderConfigs& configs) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateMojoMediaVideoDecoderCapabilities(configs);
}

void GpuDataManagerImpl::UpdateMojoMediaVideoEncoderCapabilities(
    const media::VideoEncodeAccelerator::SupportedProfiles&
        supported_profiles) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateMojoMediaVideoEncoderCapabilities(supported_profiles);
}

gpu::GpuFeatureInfo GpuDataManagerImpl::GetGpuFeatureInfo() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGpuFeatureInfo();
}

gpu::GPUInfo GpuDataManagerImpl::GetGPUInfoForHardwareGpu() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGPUInfoForHardwareGpu();
}

gpu::GpuFeatureInfo GpuDataManagerImpl::GetGpuFeatureInfoForHardwareGpu()
    const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGpuFeatureInfoForHardwareGpu();
}

std::vector<std::string> GpuDataManagerImpl::GetDawnInfoList() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetDawnInfoList();
}

bool GpuDataManagerImpl::GpuAccessAllowedForHardwareGpu(std::string* reason) {
  base::AutoLock auto_lock(lock_);
  return private_->GpuAccessAllowedForHardwareGpu(reason);
}

bool GpuDataManagerImpl::IsGpuCompositingDisabledForHardwareGpu() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuCompositingDisabledForHardwareGpu();
}

gfx::GpuExtraInfo GpuDataManagerImpl::GetGpuExtraInfo() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGpuExtraInfo();
}

bool GpuDataManagerImpl::IsGpuCompositingDisabled() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuCompositingDisabled();
}

void GpuDataManagerImpl::SetGpuCompositingDisabled() {
  base::AutoLock auto_lock(lock_);
  private_->SetGpuCompositingDisabled();
}

void GpuDataManagerImpl::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences,
    GpuProcessKind kind) const {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuPreferences(gpu_preferences, kind);
}

void GpuDataManagerImpl::AddLogMessage(int level,
                                       const std::string& header,
                                       const std::string& message) {
  base::AutoLock auto_lock(lock_);
  private_->AddLogMessage(level, header, message);
}

void GpuDataManagerImpl::ProcessCrashed() {
  base::AutoLock auto_lock(lock_);
  private_->ProcessCrashed();
}

base::Value::List GpuDataManagerImpl::GetLogMessages() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetLogMessages();
}

void GpuDataManagerImpl::HandleGpuSwitch() {
  base::AutoLock auto_lock(lock_);
  private_->HandleGpuSwitch();
}

void GpuDataManagerImpl::BlockDomainsFrom3DAPIs(const std::set<GURL>& urls,
                                                gpu::DomainGuilt guilt) {
  base::AutoLock auto_lock(lock_);
  private_->BlockDomainsFrom3DAPIs(urls, guilt);
}

bool GpuDataManagerImpl::Are3DAPIsBlocked(const GURL& top_origin_url,
                                          ThreeDAPIType requester) {
  base::AutoLock auto_lock(lock_);
  return private_->Are3DAPIsBlocked(top_origin_url, requester);
}

void GpuDataManagerImpl::UnblockDomainFrom3DAPIs(const GURL& url) {
  base::AutoLock auto_lock(lock_);
  private_->UnblockDomainFrom3DAPIs(url);
}

void GpuDataManagerImpl::DisableDomainBlockingFor3DAPIsForTesting() {
  base::AutoLock auto_lock(lock_);
  private_->DisableDomainBlockingFor3DAPIsForTesting();  // IN-TEST
}

gpu::GpuMode GpuDataManagerImpl::GetGpuMode() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGpuMode();
}

void GpuDataManagerImpl::FallBackToNextGpuMode() {
  base::AutoLock auto_lock(lock_);
  private_->FallBackToNextGpuMode();
}

bool GpuDataManagerImpl::CanFallback() const {
  base::AutoLock auto_lock(lock_);
  return private_->CanFallback();
}

bool GpuDataManagerImpl::IsGpuProcessUsingHardwareGpu() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuProcessUsingHardwareGpu();
}

void GpuDataManagerImpl::SetApplicationVisible(bool is_visible) {
  base::AutoLock auto_lock(lock_);
  private_->SetApplicationVisible(is_visible);
}

void GpuDataManagerImpl::OnDisplayAdded(const display::Display& new_display) {
  base::AutoLock auto_lock(lock_);
  private_->OnDisplayAdded(new_display);
}

void GpuDataManagerImpl::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  base::AutoLock auto_lock(lock_);
  private_->OnDisplaysRemoved(removed_displays);
}

void GpuDataManagerImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  base::AutoLock auto_lock(lock_);
  private_->OnDisplayMetricsChanged(display, changed_metrics);
}

#if BUILDFLAG(IS_LINUX)
bool GpuDataManagerImpl::IsGpuMemoryBufferNV12Supported() {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuMemoryBufferNV12Supported();
}
void GpuDataManagerImpl::SetGpuMemoryBufferNV12Supported(bool supported) {
  base::AutoLock auto_lock(lock_);
  private_->SetGpuMemoryBufferNV12Supported(supported);
}
#endif  // BUILDFLAG(IS_LINUX)

// static
void GpuDataManagerImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::GpuDataManager> receiver) {
  // This is intentionally always bound on the IO thread to ensure a low-latency
  // response to sync IPCs.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetGpuDataManagerReceiver().Bind(std::move(receiver));
}

GpuDataManagerImpl::GpuDataManagerImpl()
    : private_(std::make_unique<GpuDataManagerImplPrivate>(this)) {
  g_initialized = true;
}

GpuDataManagerImpl::~GpuDataManagerImpl() = default;

}  // namespace content
