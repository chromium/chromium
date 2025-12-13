// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_context_host.h"

#include "base/atomic_sequence_num.h"
#include "base/no_destructor.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace content {

namespace {

base::AtomicSequenceNumber g_unique_id;

std::map<int, WakeLockContextHost*>& GetIdToContextHostMap() {
  static base::NoDestructor<std::map<int, WakeLockContextHost*>>
      id_to_context_host;
  return *id_to_context_host;
}

WakeLockContextHost* ContextHostFromId(int id) {
  auto it = GetIdToContextHostMap().find(id);
  return it != GetIdToContextHostMap().end() ? it->second : nullptr;
}

}  // namespace

WakeLockContextHost::WakeLockContextHost(WebContents* web_contents)
    : id_(g_unique_id.GetNext()), web_contents_(web_contents) {
  GetIdToContextHostMap()[id_] = this;

  // Connect to a WakeLockContext, associating it with |id_|.
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockContextForID(
      id_, wake_lock_context_.BindNewPipeAndPassReceiver());
}

WakeLockContextHost::~WakeLockContextHost() {
  GetIdToContextHostMap().erase(id_);
}

// static
gfx::NativeView WakeLockContextHost::GetNativeViewForContext(int context_id) {
  WakeLockContextHost* context_host = ContextHostFromId(context_id);
  if (context_host)
    return context_host->web_contents_->GetNativeView();
  return gfx::NativeView();
}

}  // namespace content
