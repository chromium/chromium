// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl.h"

#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "gpu/ipc/common/memory_stats.h"

namespace content {

// static
GpuDataManager* GpuDataManager::GetInstance() {
  return GpuDataManagerImpl::GetInstance();
}

// static
GpuDataManagerImpl* GpuDataManagerImpl::GetInstance() {
  static base::NoDestructor<GpuDataManagerImpl> instance;
  return instance.get();
}

void GpuDataManagerImpl::BlacklistWebGLForTesting() {
  base::AutoLock auto_lock(lock_);
  private_->BlacklistWebGLForTesting();
}

gpu::GPUInfo GpuDataManagerImpl::GetGPUInfo() {
  base::AutoLock auto_lock(lock_);
  return private_->GetGPUInfo();
}

bool GpuDataManagerImpl::GpuAccessAllowed(std::string* reason) {
  base::AutoLock auto_lock(lock_);
  return private_->GpuAccessAllowed(reason);
}

void GpuDataManagerImpl::RequestDxdiagDx12VulkanGpuInfoIfNeeded(
    GpuInfoRequest request,
    bool delayed) {
  base::AutoLock auto_lock(lock_);
  private_->RequestDxdiagDx12VulkanGpuInfoIfNeeded(request, delayed);
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

gpu::GpuFeatureStatus GpuDataManagerImpl::GetFeatureStatus(
    gpu::GpuFeatureType feature) const {
  base::AutoLock auto_lock(lock_);
  return private_->GetFeatureStatus(feature);
}

void GpuDataManagerImpl::RequestVideoMemoryUsageStatsUpdate(
    VideoMemoryUsageStatsCallback callback) {
  base::AutoLock auto_lock(lock_);
  private_->RequestVideoMemoryUsageStatsUpdate(std::move(callback));
}

void GpuDataManagerImpl::AddObserver(
    GpuDataManagerObserver* observer) {
  base::AutoLock auto_lock(lock_);
  private_->AddObserver(observer);
}

void GpuDataManagerImpl::RemoveObserver(
    GpuDataManagerObserver* observer) {
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

bool GpuDataManagerImpl::GpuProcessStartAllowed() const {
  base::AutoLock auto_lock(lock_);
  return private_->GpuProcessStartAllowed();
}

void GpuDataManagerImpl::UpdateGpuInfo(
    const gpu::GPUInfo& gpu_info,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuInfo(gpu_info, gpu_info_for_hardware_gpu);
}

#if defined(OS_WIN)
void GpuDataManagerImpl::UpdateDxDiagNode(
    const gpu::DxDiagNode& dx_diagnostics) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDxDiagNode(dx_diagnostics);
}

void GpuDataManagerImpl::UpdateDx12VulkanInfo(
    const gpu::Dx12VulkanVersionInfo& dx12_vulkan_version_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDx12VulkanInfo(dx12_vulkan_version_info);
}

void GpuDataManagerImpl::UpdateDxDiagNodeRequestStatus(bool request_continues) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDxDiagNodeRequestStatus(request_continues);
}

void GpuDataManagerImpl::UpdateDx12VulkanRequestStatus(bool request_continues) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateDx12VulkanRequestStatus(request_continues);
}

bool GpuDataManagerImpl::Dx12VulkanRequested() const {
  base::AutoLock auto_lock(lock_);
  return private_->Dx12VulkanRequested();
}
#endif

void GpuDataManagerImpl::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuFeatureInfo(gpu_feature_info,
                                 gpu_feature_info_for_hardware_gpu);
}

void GpuDataManagerImpl::UpdateGpuExtraInfo(
    const gpu::GpuExtraInfo& gpu_extra_info) {
  base::AutoLock auto_lock(lock_);
  private_->UpdateGpuExtraInfo(gpu_extra_info);
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

gpu::GpuExtraInfo GpuDataManagerImpl::GetGpuExtraInfo() const {
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

void GpuDataManagerImpl::ProcessCrashed(
    base::TerminationStatus exit_code) {
  base::AutoLock auto_lock(lock_);
  private_->ProcessCrashed(exit_code);
}

std::unique_ptr<base::ListValue> GpuDataManagerImpl::GetLogMessages() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetLogMessages();
}

void GpuDataManagerImpl::HandleGpuSwitch() {
  base::AutoLock auto_lock(lock_);
  private_->HandleGpuSwitch();
}

void GpuDataManagerImpl::BlockDomainFrom3DAPIs(const GURL& url,
                                               gpu::DomainGuilt guilt) {
  base::AutoLock auto_lock(lock_);
  private_->BlockDomainFrom3DAPIs(url, guilt);
}

bool GpuDataManagerImpl::Are3DAPIsBlocked(const GURL& top_origin_url,
                                          int render_process_id,
                                          int render_frame_id,
                                          ThreeDAPIType requester) {
  base::AutoLock auto_lock(lock_);
  return private_->Are3DAPIsBlocked(
      top_origin_url, render_process_id, render_frame_id, requester);
}

void GpuDataManagerImpl::UnblockDomainFrom3DAPIs(const GURL& url) {
  base::AutoLock auto_lock(lock_);
  private_->UnblockDomainFrom3DAPIs(url);
}

void GpuDataManagerImpl::DisableDomainBlockingFor3DAPIsForTesting() {
  base::AutoLock auto_lock(lock_);
  private_->DisableDomainBlockingFor3DAPIsForTesting();
}

bool GpuDataManagerImpl::UpdateActiveGpu(uint32_t vendor_id,
                                         uint32_t device_id) {
  base::AutoLock auto_lock(lock_);
  return private_->UpdateActiveGpu(vendor_id, device_id);
}

gpu::GpuMode GpuDataManagerImpl::GetGpuMode() const {
  base::AutoLock auto_lock(lock_);
  return private_->GetGpuMode();
}

void GpuDataManagerImpl::FallBackToNextGpuMode() {
  base::AutoLock auto_lock(lock_);
  private_->FallBackToNextGpuMode();
}

bool GpuDataManagerImpl::IsGpuProcessUsingHardwareGpu() const {
  base::AutoLock auto_lock(lock_);
  return private_->IsGpuProcessUsingHardwareGpu();
}

void GpuDataManagerImpl::SetApplicationVisible(bool is_visible) {
  base::AutoLock auto_lock(lock_);
  private_->SetApplicationVisible(is_visible);
}

GpuDataManagerImpl::GpuDataManagerImpl()
    : private_(std::make_unique<GpuDataManagerImplPrivate>(this)) {}

GpuDataManagerImpl::~GpuDataManagerImpl() = default;

}  // namespace content
