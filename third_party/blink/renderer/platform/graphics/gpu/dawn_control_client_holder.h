// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CONTROL_CLIENT_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CONTROL_CLIENT_HOLDER_H_

#include <dawn/dawn_proc_table.h>

#include <vector>

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_wrapper_cache.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {

class SingleThreadTaskRunner;

}  // namespace base

namespace blink {

template <>
struct HashTraits<wgpu::Buffer> : GenericHashTraits<wgpu::Buffer> {
  STATIC_ONLY(HashTraits);
  static unsigned GetHash(const wgpu::Buffer& buffer) {
    return HashPointer(buffer.Get());
  }
  static bool Equal(const wgpu::Buffer& a, const wgpu::Buffer& b) {
    return a.Get() == b.Get();
  }

  static constexpr bool kEmptyValueIsZero = true;
  static std::nullptr_t EmptyValue() { return nullptr; }
  static std::nullptr_t DeletedValue() { return nullptr; }
};

namespace scheduler {
class EventLoop;
}  // namespace scheduler

class WebGPUMailboxTexture;

// This class holds the WebGraphicsContext3DProviderWrapper and a strong
// reference to the WebGPU APIChannel.
// DawnControlClientHolder::Destroy() should be called to destroy the
// context which will free all command buffer and GPU resources. As long
// as the reference to the APIChannel is held, calling WebGPU procs is
// valid.
class PLATFORM_EXPORT DawnControlClientHolder
    : public RefCounted<DawnControlClientHolder> {
 public:
  static scoped_refptr<DawnControlClientHolder> Create(
      std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  DawnControlClientHolder(
      std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void Destroy();

  // Returns a weak pointer to |context_provider_|. If the pointer is valid and
  // non-null, the WebGPU context has not been destroyed, and it is safe to use
  // the WebGPU interface.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> GetContextProviderWeakPtr()
      const;
  wgpu::Instance GetWGPUInstance() const;
  void MarkContextLost();
  bool IsContextLost() const;
  std::unique_ptr<WebGpuSharedImageWrapperLease> LeaseWebGpuSharedImageWrapper(
      viz::SharedImageFormat format,
      gfx::Size size,
      const gfx::ColorSpace& color_space,
      SkAlphaType alpha_type);

  // Flush commands on this client immediately.
  void Flush();
  // Ensure commands on this client are flushed by the end of the task.
  void EnsureFlush(scheduler::EventLoop& event_loop);

  void TrackMailboxTexture(base::WeakPtr<WebGPUMailboxTexture>);
  void UntrackMailboxTexture(base::WeakPtr<WebGPUMailboxTexture>);

  void TrackMappableBuffer(const wgpu::Buffer& buffer);
  void UntrackMappableBuffer(const wgpu::Buffer& buffer);

 private:
  friend class RefCounted<DawnControlClientHolder>;
  ~DawnControlClientHolder();

  void DestroyMappableBuffers();

  bool context_lost_ = false;
  std::unique_ptr<WebGraphicsContext3DProviderWrapper> context_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<gpu::webgpu::APIChannel> api_channel_;
  WebGpuSharedImageWrapperCache shared_image_wrapper_cache_;
  Vector<base::WeakPtr<WebGPUMailboxTexture>> mailbox_textures_;
  HashSet<wgpu::Buffer> mappable_buffers_;

  base::WeakPtrFactory<DawnControlClientHolder> weak_ptr_factory_{this};
};

// Slightly hacky way to get the wgslLanguageFeatures without accessing the
// DawnControlClient because it is initialized asynchronously on workers.
// TODO(crbug.com/1246805): Remove this hack when the DawnControlClient can be
// initialized synchronously on workers and query from its wgpu::Instance
// instead.
PLATFORM_EXPORT std::vector<wgpu::WGSLLanguageFeatureName>
GatherWGSLLanguageFeatures();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CONTROL_CLIENT_HOLDER_H_
