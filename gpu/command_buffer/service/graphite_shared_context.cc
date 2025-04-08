// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_shared_context.h"

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/PrecompileContext.h"

namespace gpu {

// Helper class used by subclasses to acquire |lock_| if it exists.
class SCOPED_LOCKABLE GraphiteSharedContext::AutoLock {
  STACK_ALLOCATED();

 public:
  explicit AutoLock(const GraphiteSharedContext* context)
      EXCLUSIVE_LOCK_FUNCTION(context->lock_)
      : auto_lock_(context->lock_ ? &context->lock_.value() : nullptr) {}
  ~AutoLock() UNLOCK_FUNCTION() = default;

  AutoLock(const AutoLock&) = delete;
  AutoLock& operator=(const AutoLock&) = delete;

 private:
  base::AutoLockMaybe auto_lock_;
};

GraphiteSharedContext::GraphiteSharedContext(
    std::unique_ptr<skgpu::graphite::Context> graphite_context,
    bool is_thread_safe)
    : graphite_context_(std::move(graphite_context)) {
  CHECK(graphite_context_);

  if (is_thread_safe) {
    lock_.emplace();
  }
}

GraphiteSharedContext::~GraphiteSharedContext() = default;

skgpu::BackendApi GraphiteSharedContext::backend() const {
  AutoLock auto_lock(this);
  return graphite_context_->backend();
}

std::unique_ptr<skgpu::graphite::Recorder> GraphiteSharedContext::makeRecorder(
    const skgpu::graphite::RecorderOptions& options) {
  AutoLock auto_lock(this);
  return graphite_context_->makeRecorder(options);
}

std::unique_ptr<skgpu::graphite::PrecompileContext>
GraphiteSharedContext::makePrecompileContext() {
  AutoLock auto_lock(this);
  return graphite_context_->makePrecompileContext();
}

bool GraphiteSharedContext::insertRecording(
    const skgpu::graphite::InsertRecordingInfo& info) {
  AutoLock auto_lock(this);
  return graphite_context_->insertRecording(info);
}

bool GraphiteSharedContext::submit(skgpu::graphite::SyncToCpu syncToCpu) {
  AutoLock auto_lock(this);
  return graphite_context_->submit(syncToCpu);
}

bool GraphiteSharedContext::hasUnfinishedGpuWork() const {
  AutoLock auto_lock(this);
  return graphite_context_->hasUnfinishedGpuWork();
}

void GraphiteSharedContext::asyncRescaleAndReadPixels(
    const SkImage* src,
    const SkImageInfo& dstImageInfo,
    const SkIRect& srcRect,
    SkImage::RescaleGamma rescaleGamma,
    SkImage::RescaleMode rescaleMode,
    SkImage::ReadPixelsCallback callback,
    SkImage::ReadPixelsContext callbackContext) {
  AutoLock auto_lock(this);
  return graphite_context_->asyncRescaleAndReadPixels(
      src, dstImageInfo, srcRect, rescaleGamma, rescaleMode, callback,
      callbackContext);
}

void GraphiteSharedContext::asyncRescaleAndReadPixels(
    const SkSurface* src,
    const SkImageInfo& dstImageInfo,
    const SkIRect& srcRect,
    SkImage::RescaleGamma rescaleGamma,
    SkImage::RescaleMode rescaleMode,
    SkImage::ReadPixelsCallback callback,
    SkImage::ReadPixelsContext callbackContext) {
  AutoLock auto_lock(this);
  return graphite_context_->asyncRescaleAndReadPixels(
      src, dstImageInfo, srcRect, rescaleGamma, rescaleMode, callback,
      callbackContext);
}

void GraphiteSharedContext::asyncRescaleAndReadPixelsYUV420(
    const SkImage* src,
    SkYUVColorSpace yuvColorSpace,
    sk_sp<SkColorSpace> dstColorSpace,
    const SkIRect& srcRect,
    const SkISize& dstSize,
    SkImage::RescaleGamma rescaleGamma,
    SkImage::RescaleMode rescaleMode,
    SkImage::ReadPixelsCallback callback,
    SkImage::ReadPixelsContext callbackContext) {
  AutoLock auto_lock(this);
  return graphite_context_->asyncRescaleAndReadPixelsYUV420(
      src, yuvColorSpace, dstColorSpace, srcRect, dstSize, rescaleGamma,
      rescaleMode, callback, callbackContext);
}

void GraphiteSharedContext::asyncRescaleAndReadPixelsYUV420(
    const SkSurface* src,
    SkYUVColorSpace yuvColorSpace,
    sk_sp<SkColorSpace> dstColorSpace,
    const SkIRect& srcRect,
    const SkISize& dstSize,
    SkImage::RescaleGamma rescaleGamma,
    SkImage::RescaleMode rescaleMode,
    SkImage::ReadPixelsCallback callback,
    SkImage::ReadPixelsContext callbackContext) {
  AutoLock auto_lock(this);
  return graphite_context_->asyncRescaleAndReadPixelsYUV420(
      src, yuvColorSpace, dstColorSpace, srcRect, dstSize, rescaleGamma,
      rescaleMode, callback, callbackContext);
}

void GraphiteSharedContext::asyncRescaleAndReadPixelsYUVA420(
    const SkImage* src,
    SkYUVColorSpace yuvColorSpace,
    sk_sp<SkColorSpace> dstColorSpace,
    const SkIRect& srcRect,
    const SkISize& dstSize,
    SkImage::RescaleGamma rescaleGamma,
    SkImage::RescaleMode rescaleMode,
    SkImage::ReadPixelsCallback callback,
    SkImage::ReadPixelsContext callbackContext) {
  AutoLock auto_lock(this);
  return graphite_context_->asyncRescaleAndReadPixelsYUVA420(
      src, yuvColorSpace, dstColorSpace, srcRect, dstSize, rescaleGamma,
      rescaleMode, callback, callbackContext);
}

void GraphiteSharedContext::asyncRescaleAndReadPixelsYUVA420(
    const SkSurface* src,
    SkYUVColorSpace yuvColorSpace,
    sk_sp<SkColorSpace> dstColorSpace,
    const SkIRect& srcRect,
    const SkISize& dstSize,
    SkImage::RescaleGamma rescaleGamma,
    SkImage::RescaleMode rescaleMode,
    SkImage::ReadPixelsCallback callback,
    SkImage::ReadPixelsContext callbackContext) {
  AutoLock auto_lock(this);
  return graphite_context_->asyncRescaleAndReadPixelsYUVA420(
      src, yuvColorSpace, dstColorSpace, srcRect, dstSize, rescaleGamma,
      rescaleMode, callback, callbackContext);
}

void GraphiteSharedContext::checkAsyncWorkCompletion() {
  AutoLock auto_lock(this);
  return graphite_context_->checkAsyncWorkCompletion();
}

void GraphiteSharedContext::deleteBackendTexture(
    const skgpu::graphite::BackendTexture& texture) {
  AutoLock auto_lock(this);
  return graphite_context_->deleteBackendTexture(texture);
}

void GraphiteSharedContext::freeGpuResources() {
  AutoLock auto_lock(this);
  return graphite_context_->freeGpuResources();
}

void GraphiteSharedContext::performDeferredCleanup(
    std::chrono::milliseconds msNotUsed) {
  AutoLock auto_lock(this);
  return graphite_context_->performDeferredCleanup(msNotUsed);
}

size_t GraphiteSharedContext::currentBudgetedBytes() const {
  AutoLock auto_lock(this);
  return graphite_context_->currentBudgetedBytes();
}

size_t GraphiteSharedContext::currentPurgeableBytes() const {
  AutoLock auto_lock(this);
  return graphite_context_->currentPurgeableBytes();
}

size_t GraphiteSharedContext::maxBudgetedBytes() const {
  AutoLock auto_lock(this);
  return graphite_context_->maxBudgetedBytes();
}

void GraphiteSharedContext::setMaxBudgetedBytes(size_t bytes) {
  AutoLock auto_lock(this);
  return graphite_context_->setMaxBudgetedBytes(bytes);
}

void GraphiteSharedContext::dumpMemoryStatistics(
    SkTraceMemoryDump* traceMemoryDump) const {
  AutoLock auto_lock(this);
  return graphite_context_->dumpMemoryStatistics(traceMemoryDump);
}

bool GraphiteSharedContext::isDeviceLost() const {
  AutoLock auto_lock(this);
  return graphite_context_->isDeviceLost();
}

int GraphiteSharedContext::maxTextureSize() const {
  AutoLock auto_lock(this);
  return graphite_context_->maxTextureSize();
}

bool GraphiteSharedContext::supportsProtectedContent() const {
  AutoLock auto_lock(this);
  return graphite_context_->supportsProtectedContent();
}

skgpu::GpuStatsFlags GraphiteSharedContext::supportedGpuStats() const {
  AutoLock auto_lock(this);
  return graphite_context_->supportedGpuStats();
}

}  // namespace gpu
