// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_support.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

namespace {

// Helper function to help hold the `key_system_support` remote.
void OnIsKeySystemSupportedResult(
    mojo::Remote<media::mojom::KeySystemSupport> key_system_support,
    IsKeySystemSupportedCB cb,
    bool is_supported,
    media::mojom::KeySystemCapabilityPtr capability) {
  std::move(cb).Run(is_supported, std::move(capability));
}

}  // namespace

void IsKeySystemSupported(const std::string& key_system,
                          IsKeySystemSupportedCB cb) {
  DVLOG(3) << __func__ << ": key_system=" << key_system;

  mojo::Remote<media::mojom::KeySystemSupport> key_system_support;
  content::RenderThread::Get()->BindHostReceiver(
      key_system_support.BindNewPipeAndPassReceiver());

  auto* key_system_support_raw = key_system_support.get();
  key_system_support_raw->IsKeySystemSupported(
      key_system, base::BindOnce(&OnIsKeySystemSupportedResult,
                                 std::move(key_system_support), std::move(cb)));
}

}  // namespace content
