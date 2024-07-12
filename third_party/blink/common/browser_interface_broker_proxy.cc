// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

#include <map>
#include <string>

#include "base/notreached.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace blink {

namespace {

// TODO(https://crbug.com/41482945): Deduplicate `SetBinderForTesting`-related
// code - after moving `browser_interface_broker_proxy.h` from
// `blink/public/common` to `blink/public/platform` it should be possible to
// remove this class/code and instead use `BrowserInterfaceBrokerProxyImpl` from
// `blink/renderer/platform/mojo/browser_interface_broker_proxy_impl.cc` as the
// base class of `EmptyBrowserInterfaceBrokerProxy`.  This TODO will be resolved
// by the WIP CL at https://crrev.com/c/5651622.
class EmptyBrowserInterfaceBrokerProxy : public BrowserInterfaceBrokerProxy {
 public:
  ~EmptyBrowserInterfaceBrokerProxy() override = default;

  CrossVariantMojoReceiver<mojom::BrowserInterfaceBrokerInterfaceBase> Reset(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    // `Reset` should only be called on a real `BrowserInterfaceBrokerProxy`.
    // It should never be called on `EmptyBrowserInterfaceBrokerProxy`.
    NOTREACHED_NORETURN();
  }

  void GetInterface(mojo::GenericPendingReceiver receiver) const override {
    // If present, then use a binder registered via SetBinderForTesting.
    DCHECK(receiver.interface_name());
    auto it = binder_map_for_testing_.find(receiver.interface_name().value());
    if (it != binder_map_for_testing_.end()) {
      it->second.Run(receiver.PassPipe());
    }

    // Otherwise, do nothing and leave `receiver` unbound.
  }

  bool SetBinderForTesting(
      const std::string& name,
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder)
      const override {
    if (!binder) {
      binder_map_for_testing_.erase(name);
      return true;
    }

    auto result = binder_map_for_testing_.emplace(name, std::move(binder));
    return result.second;
  }

 private:
  using BinderMap =
      std::map<std::string,
               base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>>;
  mutable BinderMap binder_map_for_testing_;
};

}  // namespace

BrowserInterfaceBrokerProxy::BrowserInterfaceBrokerProxy() = default;
BrowserInterfaceBrokerProxy::~BrowserInterfaceBrokerProxy() = default;

BrowserInterfaceBrokerProxy& GetEmptyBrowserInterfaceBroker() {
  static base::SequenceLocalStorageSlot<EmptyBrowserInterfaceBrokerProxy>
      proxy_slot;
  return proxy_slot.GetOrCreateValue();
}

}  // namespace blink
