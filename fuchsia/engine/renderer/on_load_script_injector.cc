// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/on_load_script_injector.h"

#include <lib/zx/vmo.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/shared_memory_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

OnLoadScriptInjector::OnLoadScriptInjector(content::RenderFrame* frame)
    : RenderFrameObserver(frame), weak_ptr_factory_(this) {
  render_frame()->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&OnLoadScriptInjector::BindToReceiver,
                          weak_ptr_factory_.GetWeakPtr()));
}

OnLoadScriptInjector::~OnLoadScriptInjector() {}

void OnLoadScriptInjector::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::OnLoadScriptInjector> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void OnLoadScriptInjector::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  // Ignore pushState or document fragment navigation.
  if (is_same_document_navigation)
    return;

  // Don't inject anything for subframes.
  if (!render_frame()->IsMainFrame())
    return;

  for (mojo::ScopedSharedBufferHandle& script : on_load_scripts_) {
    DCHECK_EQ(script->GetSize() % 2, 0u);  // Crude check to see this is UTF-16.

    auto mapping = script->Map(script->GetSize());
    base::string16 script_converted(static_cast<base::char16*>(mapping.get()),
                                    script->GetSize() / sizeof(base::char16));
    render_frame()->ExecuteJavaScript(script_converted);
  }
}

void OnLoadScriptInjector::AddOnLoadScript(
    mojo::ScopedSharedBufferHandle script) {
  on_load_scripts_.push_back(std::move(script));
}

void OnLoadScriptInjector::ClearOnLoadScripts() {
  on_load_scripts_.clear();
}

void OnLoadScriptInjector::OnDestruct() {
  delete this;
}
