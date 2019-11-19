// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_support.h"

#include "base/logging.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

bool IsKeySystemSupported(
    const std::string& key_system,
    media::mojom::KeySystemCapabilityPtr* key_system_capability) {
  DVLOG(3) << __func__ << " key_system: " << key_system;

  bool is_supported = false;
  mojo::Remote<media::mojom::KeySystemSupport> key_system_support;
  content::RenderThread::Get()->BindHostReceiver(
      key_system_support.BindNewPipeAndPassReceiver());

  key_system_support->IsKeySystemSupported(key_system, &is_supported,
                                           key_system_capability);
  return is_supported;
}

}  // namespace content
