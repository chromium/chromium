// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/browser_interface_broker_proxy_impl.h"

#include <string_view>

#include "base/task/single_thread_task_runner.h"
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
    return StringHasher::ComputeHash<LChar>(data, size);
  }

  static bool Equal(const String& a, std::string_view b) {
    unsigned b_size = base::checked_cast<unsigned>(b.size());
    StringView wtf_b(b.data(), b_size);
    return EqualStringView(a, wtf_b);
  }
};

}  // namespace

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
  // Local binders can be registered via SetBinderForTesting.
  DCHECK(receiver.interface_name());

  if (!binder_map_for_testing_.empty()) {
    auto it = binder_map_for_testing_
                  .Find<InterfaceNameHashTranslator, std::string_view>(
                      receiver.interface_name().value());
    if (it != binder_map_for_testing_.end()) {
      it->value.Run(receiver.PassPipe());
      return;
    }
  }

  broker_->GetInterface(std::move(receiver));
}

bool BrowserInterfaceBrokerProxyImpl::is_bound() const {
  return broker_.is_bound();
}

bool BrowserInterfaceBrokerProxyImpl::SetBinderForTesting(
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

}  // namespace blink
