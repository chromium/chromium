// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

#include <dawn/wire/WireClient.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gl/buildflags.h"

namespace blink {

// static
scoped_refptr<DawnControlClientHolder> DawnControlClientHolder::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto dawn_control_client_holder =
      base::MakeRefCounted<DawnControlClientHolder>(std::move(context_provider),
                                                    std::move(task_runner));
  // The context lost callback occurs when the client receives
  // OnGpuControlLostContext. This can happen on fatal errors when the GPU
  // channel is disconnected: the GPU process crashes, the GPU process fails to
  // deserialize a message, etc. We mark the context lost, but NOT destroy the
  // entire WebGraphicsContext3DProvider as that would free services for mapping
  // shared memory. There may still be outstanding mapped GPUBuffers pointing to
  // this memory.
  dawn_control_client_holder->context_provider_->ContextProvider()
      ->SetLostContextCallback(WTF::BindRepeating(
          &DawnControlClientHolder::MarkContextLost,
          dawn_control_client_holder->weak_ptr_factory_.GetWeakPtr()));
  return dawn_control_client_holder;
}

DawnControlClientHolder::DawnControlClientHolder(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      task_runner_(task_runner),
      api_channel_(context_provider_->ContextProvider()
                       ->WebGPUInterface()
                       ->GetAPIChannel()),
      recyclable_resource_cache_(GetContextProviderWeakPtr(), task_runner) {}

DawnControlClientHolder::~DawnControlClientHolder() = default;

void DawnControlClientHolder::Destroy() {
  MarkContextLost();

  // Destroy the WebGPU context.
  // This ensures that GPU resources are eagerly reclaimed.
  // Because we have disconnected the wire client, any JavaScript which uses
  // WebGPU will do nothing.
  if (context_provider_) {
    // If the context provider is destroyed during a real lost context event, it
    // causes the CommandBufferProxy that the context provider owns, which is
    // what issued the lost context event in the first place, to be destroyed
    // before the event is done being handled. This causes a crash when an
    // outstanding AutoLock goes out of scope. To avoid this, we create a no-op
    // task to hold a reference to the context provider until this function is
    // done executing, and drop it after.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](std::unique_ptr<WebGraphicsContext3DProviderWrapper>
                              context_provider) {},
                       std::move(context_provider_)));
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DawnControlClientHolder::GetContextProviderWeakPtr() const {
  if (!context_provider_) {
    return nullptr;
  }
  return context_provider_->GetWeakPtr();
}

wgpu::Instance DawnControlClientHolder::GetWGPUInstance() const {
  return wgpu::Instance(api_channel_->GetWGPUInstance());
}

void DawnControlClientHolder::MarkContextLost() {
  if (context_lost_) {
    return;
  }
  api_channel_->Disconnect();
  context_lost_ = true;
}

bool DawnControlClientHolder::IsContextLost() const {
  return context_lost_;
}

std::unique_ptr<RecyclableCanvasResource>
DawnControlClientHolder::GetOrCreateCanvasResource(const SkImageInfo& info) {
  return recyclable_resource_cache_.GetOrCreateCanvasResource(info);
}

void DawnControlClientHolder::Flush() {
  auto context_provider = GetContextProviderWeakPtr();
  if (context_provider) [[likely]] {
    context_provider->ContextProvider()->WebGPUInterface()->FlushCommands();
  }
}

void DawnControlClientHolder::EnsureFlush(scheduler::EventLoop& event_loop) {
  auto context_provider = GetContextProviderWeakPtr();
  if (!context_provider) [[unlikely]] {
    return;
  }
  if (!context_provider->ContextProvider()
           ->WebGPUInterface()
           ->EnsureAwaitingFlush()) {
    // We've already enqueued a task to flush, or the command buffer
    // is empty. Do nothing.
    return;
  }
  event_loop.EnqueueMicrotask(WTF::BindOnce(
      [](scoped_refptr<DawnControlClientHolder> dawn_control_client) {
        if (auto context_provider =
                dawn_control_client->GetContextProviderWeakPtr()) {
          context_provider->ContextProvider()
              ->WebGPUInterface()
              ->FlushAwaitingCommands();
        }
      },
      scoped_refptr<DawnControlClientHolder>(this)));
}

std::vector<wgpu::WGSLFeatureName> GatherWGSLFeatures() {
#if BUILDFLAG(USE_DAWN)
  // Create a dawn::wire::WireClient on a noop serializer, to get an instance
  // from it.
  class NoopSerializer : public dawn::wire::CommandSerializer {
   public:
    size_t GetMaximumAllocationSize() const override { return sizeof(buf); }
    void* GetCmdSpace(size_t size) override { return buf; }
    bool Flush() override { return true; }

   private:
    char buf[1024];
  };

  NoopSerializer noop_serializer;
  dawn::wire::WireClient client{{.serializer = &noop_serializer}};

  // Control which WGSL features are exposed based on flags.
  wgpu::DawnWireWGSLControl wgsl_control = {{
      .enableUnsafe = base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUnsafeWebGPU),
      // This can be changed to true for manual testing with the
      // chromium_testing_* WGSL features.
      .enableTesting = false,
  }};
  wgsl_control.enableExperimental =
      wgsl_control.enableUnsafe ||
      RuntimeEnabledFeatures::WebGPUExperimentalFeaturesEnabled();

  // Additionally populate the WGSL blocklist based on the Finch feature.
  std::vector<std::string> wgsl_unsafe_features_owned;
  std::vector<const char*> wgsl_unsafe_features;

  if (!wgsl_control.enableUnsafe) {
    wgsl_unsafe_features_owned =
        base::SplitString(features::kWGSLUnsafeFeatures.Get(), ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    wgsl_unsafe_features.reserve(wgsl_unsafe_features_owned.size());
    for (const auto& f : wgsl_unsafe_features_owned) {
      wgsl_unsafe_features.push_back(f.c_str());
    }
  }
  wgpu::DawnWGSLBlocklist wgsl_blocklist = {{
      .nextInChain = &wgsl_control,
      .blocklistedFeatureCount = wgsl_unsafe_features.size(),
      .blocklistedFeatures = wgsl_unsafe_features.data(),
  }};
  // Create the instance from all the chained structures and gather features
  // from it.
  wgpu::InstanceDescriptor instance_desc = {
      .nextInChain = &wgsl_blocklist,
  };
  wgpu::Instance instance = wgpu::Instance::Acquire(
      client
          .ReserveInstance(
              &static_cast<const WGPUInstanceDescriptor&>(instance_desc))
          .instance);

  size_t feature_count = instance.EnumerateWGSLLanguageFeatures(nullptr);
  std::vector<wgpu::WGSLFeatureName> features(feature_count);
  instance.EnumerateWGSLLanguageFeatures(features.data());

  return features;
#else
  return {};
#endif
}

}  // namespace blink
