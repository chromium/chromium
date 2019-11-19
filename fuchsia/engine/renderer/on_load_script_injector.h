// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_
#define FUCHSIA_ENGINE_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_

#include <lib/zx/vmo.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/shared_memory_handle.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "fuchsia/engine/on_load_script_injector.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

// Injects one or more scripts into a RenderFrame at the earliest possible time
// during the page load process.
class OnLoadScriptInjector : public content::RenderFrameObserver,
                             public mojom::OnLoadScriptInjector {
 public:
  explicit OnLoadScriptInjector(content::RenderFrame* frame);

  void BindToReceiver(
      mojo::PendingAssociatedReceiver<mojom::OnLoadScriptInjector> receiver);

  void AddOnLoadScript(mojo::ScopedSharedBufferHandle script) override;
  void ClearOnLoadScripts() override;

  // RenderFrameObserver override:
  void OnDestruct() override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;

 private:
  // Called by OnDestruct(), when the RenderFrame is destroyed.
  ~OnLoadScriptInjector() override;

  std::vector<mojo::ScopedSharedBufferHandle> on_load_scripts_;
  mojo::AssociatedReceiverSet<mojom::OnLoadScriptInjector> receivers_;
  base::WeakPtrFactory<OnLoadScriptInjector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OnLoadScriptInjector);
};

#endif  // FUCHSIA_ENGINE_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_
