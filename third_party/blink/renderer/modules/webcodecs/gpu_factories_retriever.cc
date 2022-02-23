// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"

#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

media::GpuVideoAcceleratorFactories* GetGpuFactoriesOnMainThread() {
  DCHECK(IsMainThread());
  return Platform::Current()->GetGpuFactories();
}

void RetrieveGpuFactories(OutputCB result_callback) {
  if (IsMainThread()) {
    std::move(result_callback).Run(GetGpuFactoriesOnMainThread());
    return;
  }

  Thread::MainThread()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(
          CrossThreadBindOnce(&GetGpuFactoriesOnMainThread)),
      ConvertToBaseOnceCallback(std::move(result_callback)));
}

void OnSupportKnown(OutputCB result_cb,
                    media::GpuVideoAcceleratorFactories* factories) {
  std::move(result_cb).Run(factories);
}

}  // namespace

void RetrieveGpuFactoriesWithKnownEncoderSupport(OutputCB callback) {
  auto on_factories_received =
      [](OutputCB result_cb, media::GpuVideoAcceleratorFactories* factories) {
        if (!factories || factories->IsEncoderSupportKnown()) {
          std::move(result_cb).Run(factories);
        } else {
          factories->NotifyEncoderSupportKnown(ConvertToBaseOnceCallback(
              CrossThreadBindOnce(OnSupportKnown, std::move(result_cb),
                                  CrossThreadUnretained(factories))));
        }
      };

  auto factories_callback =
      CrossThreadBindOnce(on_factories_received, std::move(callback));

  RetrieveGpuFactories(std::move(factories_callback));
}

void RetrieveGpuFactoriesWithKnownDecoderSupport(OutputCB callback) {
  auto on_factories_received =
      [](OutputCB result_cb, media::GpuVideoAcceleratorFactories* factories) {
        if (!factories || factories->IsDecoderSupportKnown()) {
          std::move(result_cb).Run(factories);
        } else {
          factories->NotifyDecoderSupportKnown(ConvertToBaseOnceCallback(
              CrossThreadBindOnce(OnSupportKnown, std::move(result_cb),
                                  CrossThreadUnretained(factories))));
        }
      };

  auto factories_callback =
      CrossThreadBindOnce(on_factories_received, std::move(callback));

  RetrieveGpuFactories(std::move(factories_callback));
}

}  // namespace blink
