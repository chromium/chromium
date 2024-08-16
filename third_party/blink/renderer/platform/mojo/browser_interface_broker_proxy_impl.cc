// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/browser_interface_broker_proxy_impl.h"

#include <string_view>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

// Helper for looking up `std::string_view`-represented mojo interface names in
// a `WTF::HashMap<String, ...>`.  Mojo interface names are ASCII-only, so
// `StringHasher::DefaultConverter` and `StringView(const LChar* chars, unsigned
// length)` work fine here.
struct InterfaceNameHashTranslator {
  static unsigned GetHash(std::string_view s) {
    const LChar* data = reinterpret_cast<const LChar*>(s.data());
    unsigned size = base::checked_cast<unsigned>(s.size());
    return StringHasher::HashMemory(data, size);
  }

  static bool Equal(const String& a, std::string_view b) {
    unsigned b_size = base::checked_cast<unsigned>(b.size());
    StringView wtf_b(b.data(), b_size);
    return EqualStringView(a, wtf_b);
  }
};

class EmptyBrowserInterfaceBrokerProxy
    : public TestableBrowserInterfaceBrokerProxy {
 public:
  EmptyBrowserInterfaceBrokerProxy() = default;
  ~EmptyBrowserInterfaceBrokerProxy() override = default;

  CrossVariantMojoReceiver<mojom::BrowserInterfaceBrokerInterfaceBase> Reset(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    // `Reset` should only be called on a real `BrowserInterfaceBrokerProxy`.
    // It should never be called on `EmptyBrowserInterfaceBrokerProxy`.
    NOTREACHED();
  }

  void GetInterface(mojo::GenericPendingReceiver receiver) const override {
    DCHECK(receiver.interface_name());
    TestBinder* binder = FindTestBinder(receiver.interface_name().value());
    if (binder) {
      binder->Run(receiver.PassPipe());
    }

    // Otherwise, do nothing and leave `receiver` unbound.
  }
};

}  // namespace

TestableBrowserInterfaceBrokerProxy::TestBinder*
TestableBrowserInterfaceBrokerProxy::FindTestBinder(
    std::string_view interface_name) const {
  if (!binder_map_for_testing_.empty()) {
    auto it = binder_map_for_testing_
                  .Find<InterfaceNameHashTranslator, std::string_view>(
                      interface_name);
    if (it != binder_map_for_testing_.end()) {
      return &it->value;
    }
  }

  return nullptr;
}

bool TestableBrowserInterfaceBrokerProxy::SetBinderForTesting(
    const std::string& name,
    base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder) const {
  String wtf_name(name);

  if (!binder) {
    binder_map_for_testing_.erase(wtf_name);
    return true;
  }

  auto result =
      binder_map_for_testing_.insert(std::move(wtf_name), std::move(binder));
  return result.is_new_entry;
}

BrowserInterfaceBrokerProxy& GetEmptyBrowserInterfaceBroker() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(EmptyBrowserInterfaceBrokerProxy,
                                  empty_broker, ());
  return empty_broker;
}

BrowserInterfaceBrokerProxy::BrowserInterfaceBrokerProxy() = default;
BrowserInterfaceBrokerProxy::~BrowserInterfaceBrokerProxy() = default;

BrowserInterfaceBrokerProxyImpl::BrowserInterfaceBrokerProxyImpl(
    ContextLifecycleNotifier* notifier)
    : broker_(notifier) {}

void BrowserInterfaceBrokerProxyImpl::Bind(
    CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase> broker,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  broker_.Bind(std::move(broker), std::move(task_runner));
}

CrossVariantMojoReceiver<mojom::BrowserInterfaceBrokerInterfaceBase>
BrowserInterfaceBrokerProxyImpl::Reset(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  broker_.reset();
  return broker_.BindNewPipeAndPassReceiver(std::move(task_runner));
}

void BrowserInterfaceBrokerProxyImpl::GetInterface(
    mojo::GenericPendingReceiver receiver) const {
  DCHECK(receiver.interface_name());
  TestBinder* binder = FindTestBinder(receiver.interface_name().value());
  if (binder) {
    binder->Run(receiver.PassPipe());
    return;
  }

  broker_->GetInterface(std::move(receiver));
}

bool BrowserInterfaceBrokerProxyImpl::is_bound() const {
  return broker_.is_bound();
}

void BrowserInterfaceBrokerProxyImpl::Trace(Visitor* visitor) const {
  visitor->Trace(broker_);
}

}  // namespace blink
