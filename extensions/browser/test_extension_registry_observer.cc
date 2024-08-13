// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_extension_registry_observer.h"

#include <memory>

#include "base/run_loop.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class TestExtensionRegistryObserver::Waiter {
 public:
  Waiter() = default;
  Waiter(const Waiter&) = delete;
  Waiter& operator=(const Waiter&) = delete;

  scoped_refptr<const Extension> Wait() {
    if (!observed_)
      run_loop_.Run();
    return extension_;
  }

  void OnObserved(const Extension* extension) {
    observed_ = true;
    run_loop_.Quit();
    extension_ = extension;
  }

 private:
  bool observed_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  scoped_refptr<const Extension> extension_;
};

TestExtensionRegistryObserver::TestExtensionRegistryObserver(
    ExtensionRegistry* registry)
    : TestExtensionRegistryObserver(registry, std::string()) {
}

TestExtensionRegistryObserver::TestExtensionRegistryObserver(
    ExtensionRegistry* registry,
    const ExtensionId& extension_id)
    : will_be_installed_waiter_(std::make_unique<Waiter>()),
      installed_waiter_(std::make_unique<Waiter>()),
      uninstalled_waiter_(std::make_unique<Waiter>()),
      uninstallation_denied_waiter_(std::make_unique<Waiter>()),
      loaded_waiter_(std::make_unique<Waiter>()),
      ready_waiter_(std::make_unique<Waiter>()),
      unloaded_waiter_(std::make_unique<Waiter>()),
      extension_id_(extension_id) {
  extension_registry_observation_.Observe(registry);
}

TestExtensionRegistryObserver::~TestExtensionRegistryObserver() {
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionUninstalled() {
  return Wait(&uninstalled_waiter_);
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionUninstallationDenied() {
  return Wait(&uninstallation_denied_waiter_);
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionWillBeInstalled() {
  return Wait(&will_be_installed_waiter_);
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionInstalled() {
  return Wait(&installed_waiter_);
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionLoaded() {
  return Wait(&loaded_waiter_);
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionUnloaded() {
  return Wait(&unloaded_waiter_);
}

scoped_refptr<const Extension>
TestExtensionRegistryObserver::WaitForExtensionReady() {
  return Wait(&ready_waiter_);
}

void TestExtensionRegistryObserver::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    will_be_installed_waiter_->OnObserved(extension);
}

void TestExtensionRegistryObserver::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    installed_waiter_->OnObserved(extension);
}

void TestExtensionRegistryObserver::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    uninstalled_waiter_->OnObserved(extension);
}

void TestExtensionRegistryObserver::OnExtensionUninstallationDenied(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    uninstallation_denied_waiter_->OnObserved(extension);
}

void TestExtensionRegistryObserver::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    loaded_waiter_->OnObserved(extension);
}

void TestExtensionRegistryObserver::OnExtensionReady(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    ready_waiter_->OnObserved(extension);
}

void TestExtensionRegistryObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (extension_id_.empty() || extension->id() == extension_id_)
    unloaded_waiter_->OnObserved(extension);
}

scoped_refptr<const Extension> TestExtensionRegistryObserver::Wait(
    std::unique_ptr<Waiter>* waiter) {
  scoped_refptr<const Extension> extension = (*waiter)->Wait();
  // Reset the waiter for future uses.
  // We could have a Waiter::Reset method, but it would reset every field in the
  // class, so let's just reset the pointer.
  *waiter = std::make_unique<Waiter>();
  return extension;
}

}  // namespace extensions
