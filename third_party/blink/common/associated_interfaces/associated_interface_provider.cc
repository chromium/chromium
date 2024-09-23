// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#include <map>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace blink {

class AssociatedInterfaceProvider::LocalProvider
    : public mojom::AssociatedInterfaceProvider {
 public:
  using Binder =
      base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>;

  explicit LocalProvider(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    associated_interface_provider_receiver_.Bind(
        remote_.BindNewEndpointAndPassDedicatedReceiver(),
        std::move(task_runner));
  }
  LocalProvider(const LocalProvider&) = delete;
  LocalProvider& operator=(const LocalProvider&) = delete;

  ~LocalProvider() override = default;

  void SetBinderForName(const std::string& name, const Binder& binder) {
    binders_[name] = binder;
  }

  void ResetBinderForName(const std::string& name) { binders_.erase(name); }

  bool HasInterface(const std::string& name) const {
    return base::Contains(binders_, name);
  }

  void GetInterface(const std::string& name,
                    mojo::ScopedInterfaceEndpointHandle handle) {
    return remote_->GetAssociatedInterface(
        name, mojo::PendingAssociatedReceiver<mojom::AssociatedInterface>(
                  std::move(handle)));
  }

 private:
  // mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<mojom::AssociatedInterface> receiver)
      override {
    auto it = binders_.find(name);
    if (it != binders_.end()) {
      it->second.Run(receiver.PassHandle());
    }
  }

  std::map<std::string, Binder> binders_;
  mojo::AssociatedReceiver<mojom::AssociatedInterfaceProvider>
      associated_interface_provider_receiver_{this};
  mojo::AssociatedRemote<mojom::AssociatedInterfaceProvider> remote_;
};

AssociatedInterfaceProvider::AssociatedInterfaceProvider(
    mojo::PendingAssociatedRemote<mojom::AssociatedInterfaceProvider> proxy,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : proxy_(std::move(proxy), task_runner),
      task_runner_(std::move(task_runner)) {
  DCHECK(proxy_.is_bound());
}

AssociatedInterfaceProvider::AssociatedInterfaceProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : local_provider_(std::make_unique<LocalProvider>(task_runner)),
      task_runner_(std::move(task_runner)) {}

AssociatedInterfaceProvider::~AssociatedInterfaceProvider() = default;

void AssociatedInterfaceProvider::GetInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (local_provider_ && (local_provider_->HasInterface(name) || !proxy_)) {
    local_provider_->GetInterface(name, std::move(handle));
    return;
  }
  DCHECK(proxy_);
  proxy_->GetAssociatedInterface(
      name, mojo::PendingAssociatedReceiver<mojom::AssociatedInterface>(
                std::move(handle)));
}

void AssociatedInterfaceProvider::OverrideBinderForTesting(
    const std::string& name,
    const base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>&
        binder) {
  if (binder) {
    if (!local_provider_) {
      local_provider_ = std::make_unique<LocalProvider>(task_runner_);
    }
    local_provider_->SetBinderForName(name, binder);
  } else if (local_provider_) {
    local_provider_->ResetBinderForName(name);
  }
}

AssociatedInterfaceProvider*
AssociatedInterfaceProvider::GetEmptyAssociatedInterfaceProvider() {
  static base::NoDestructor<AssociatedInterfaceProvider>
      associated_interface_provider(
          base::SingleThreadTaskRunner::GetCurrentDefault());
  return associated_interface_provider.get();
}

}  // namespace blink
