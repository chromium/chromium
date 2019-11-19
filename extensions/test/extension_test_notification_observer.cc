// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_test_notification_observer.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

// A callback that returns true if the condition has been met and takes no
// arguments.
using ConditionCallback = base::Callback<bool(void)>;

const Extension* GetNonTerminatedExtensions(const std::string& id,
                                            content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      id, ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::TERMINATED);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver::NotificationSet

ExtensionTestNotificationObserver::NotificationSet::NotificationSet() = default;
ExtensionTestNotificationObserver::NotificationSet::~NotificationSet() =
    default;

void ExtensionTestNotificationObserver::NotificationSet::Add(
    int type,
    const content::NotificationSource& source) {
  notification_registrar_.Add(this, type, source);
}

void ExtensionTestNotificationObserver::NotificationSet::Add(int type) {
  Add(type, content::NotificationService::AllSources());
}

void ExtensionTestNotificationObserver::NotificationSet::
    AddExtensionFrameUnregistration(ProcessManager* manager) {
  process_manager_observer_.Add(manager);
}

void ExtensionTestNotificationObserver::NotificationSet::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  callback_list_.Notify();
}

void ExtensionTestNotificationObserver::NotificationSet::
    OnExtensionFrameUnregistered(const std::string& extension_id,
                                 content::RenderFrameHost* render_frame_host) {
  callback_list_.Notify();
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver

ExtensionTestNotificationObserver::ExtensionTestNotificationObserver(
    content::BrowserContext* context)
    : context_(context),
      extension_installs_observed_(0),
      extension_load_errors_observed_(0),
      crx_installers_done_observed_(0) {
  if (context_)
    registry_observer_.Add(ExtensionRegistry::Get(context_));
}

ExtensionTestNotificationObserver::~ExtensionTestNotificationObserver() {}

void ExtensionTestNotificationObserver::WaitForNotification(
    int notification_type) {
  // TODO(bauerb): Using a WindowedNotificationObserver like this can break
  // easily, if the notification we're waiting for is sent before this method.
  // Change it so that the WindowedNotificationObserver is constructed earlier.
  content::NotificationRegistrar registrar;
  registrar.Add(this, notification_type,
                content::NotificationService::AllSources());
  content::WindowedNotificationObserver(
      notification_type, content::NotificationService::AllSources())
      .Wait();
}

bool ExtensionTestNotificationObserver::WaitForExtensionInstallError() {
  int before = extension_installs_observed_;
  content::WindowedNotificationObserver(
      NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllSources())
      .Wait();
  return extension_installs_observed_ == before;
}

bool ExtensionTestNotificationObserver::WaitForExtensionLoadError() {
  int before = extension_load_errors_observed_;
  WaitForNotification(NOTIFICATION_EXTENSION_LOAD_ERROR);
  return extension_load_errors_observed_ != before;
}

bool ExtensionTestNotificationObserver::WaitForExtensionCrash(
    const std::string& extension_id) {
  if (!GetNonTerminatedExtensions(extension_id, context_)) {
    // The extension is already unloaded, presumably due to a crash.
    return true;
  }

  content::WindowedNotificationObserver(
      NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources())
      .Wait();
  // GetNonTerminatedExtensions consults ExtensionRegistry which gets updated
  // asynchronously in a task posted when
  // NOTIFICATION_EXTENSION_PROCESS_TERMINATED is handled, so let this task run.
  base::RunLoop().RunUntilIdle();
  return (GetNonTerminatedExtensions(extension_id, context_) == NULL);
}

bool ExtensionTestNotificationObserver::WaitForCrxInstallerDone() {
  int before = crx_installers_done_observed_;
  WaitForNotification(NOTIFICATION_CRX_INSTALLER_DONE);
  return crx_installers_done_observed_ == before + 1 &&
         !last_loaded_extension_id_.empty();
}

void ExtensionTestNotificationObserver::Watch(
    int type,
    const content::NotificationSource& source) {
  CHECK(!observer_);
  observer_.reset(new content::WindowedNotificationObserver(type, source));
  registrar_.Add(this, type, source);
}

void ExtensionTestNotificationObserver::Wait() {
  observer_->Wait();

  registrar_.RemoveAll();
  observer_.reset();
}

void ExtensionTestNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_CRX_INSTALLER_DONE:
      VLOG(1) << "Got CRX_INSTALLER_DONE notification.";
      {
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        if (extension)
          last_loaded_extension_id_ = extension->id();
        else
          last_loaded_extension_id_.clear();
      }
      ++crx_installers_done_observed_;
      break;

    case NOTIFICATION_EXTENSION_LOAD_ERROR:
      VLOG(1) << "Got EXTENSION_LOAD_ERROR notification.";
      ++extension_load_errors_observed_;
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ExtensionTestNotificationObserver::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  last_loaded_extension_id_ = extension->id();
  VLOG(1) << "Got EXTENSION_LOADED notification.";
}

void ExtensionTestNotificationObserver::OnShutdown(
    ExtensionRegistry* registry) {
  registry_observer_.RemoveAll();
}

void ExtensionTestNotificationObserver::WaitForCondition(
    const ConditionCallback& condition,
    NotificationSet* notification_set) {
  if (condition.Run())
    return;
  condition_ = condition;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  std::unique_ptr<base::CallbackList<void()>::Subscription> subscription;
  if (notification_set) {
    subscription = notification_set->callback_list().Add(base::Bind(
        &ExtensionTestNotificationObserver::MaybeQuit, base::Unretained(this)));
  }
  run_loop.Run();

  condition_.Reset();
  quit_closure_.Reset();
}

void ExtensionTestNotificationObserver::MaybeQuit() {
  if (condition_.Run())
    quit_closure_.Run();
}

}  // namespace extensions
