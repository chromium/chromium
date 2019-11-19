// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"

#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

LayerTreeHostEmbedder::LayerTreeHostEmbedder()
    : LayerTreeHostEmbedder(/*client=*/nullptr,
                            /*single_thread_client=*/nullptr) {}

LayerTreeHostEmbedder::LayerTreeHostEmbedder(
    cc::LayerTreeHostClient* client,
    cc::LayerTreeHostSingleThreadClient* single_thread_client) {
  cc::LayerTreeSettings settings;
  settings.single_thread_proxy_scheduler = false;
  settings.use_layer_lists = true;
  animation_host_ = cc::AnimationHost::CreateMainInstance();
  cc::LayerTreeHost::InitParams params;
  params.client = client ? client : &layer_tree_host_client_;
  params.settings = &settings;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.task_graph_runner = &task_graph_runner_;
  params.mutator_host = animation_host_.get();

  layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(
      single_thread_client ? single_thread_client
                           : &layer_tree_host_single_thread_client_,
      std::move(params));
}

}  // namespace blink
