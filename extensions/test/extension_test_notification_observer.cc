// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_test_notification_observer.h"
#include "base/memory/raw_ptr.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"

namespace extensions {

// A callback that returns true if the condition has been met and takes no
// arguments.
using ConditionCallback = base::RepeatingCallback<bool(void)>;

////////////////////////////////////////////////////////////////////////////////
// NotificationSet::ForwardingWebContentsObserver

class ExtensionTestNotificationObserver::NotificationSet::
    ForwardingWebContentsObserver : public content::WebContentsObserver {
 public:
  ForwardingWebContentsObserver(
      content::WebContents* contents,
      ExtensionTestNotificationObserver::NotificationSet* owner)
      : WebContentsObserver(contents), owner_(owner) {}

 private:
  // content::WebContentsObserver
  void WebContentsDestroyed() override {
    // Do not add code after this line, deletes `this`.
    owner_->WebContentsDestroyed(web_contents());
  }

  raw_ptr<ExtensionTestNotificationObserver::NotificationSet> owner_;
};

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
  process_manager_observation_.Observe(manager);
}

void ExtensionTestNotificationObserver::NotificationSet::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  closure_list_.Notify();
}

void ExtensionTestNotificationObserver::NotificationSet::
    AddWebContentsDestroyed(extensions::ProcessManager* manager) {
  for (content::RenderFrameHost* render_frame_host : manager->GetAllFrames()) {
    content::WebContents* contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    if (!base::Contains(web_contents_observers_, contents)) {
      web_contents_observers_[contents] =
          std::make_unique<ForwardingWebContentsObserver>(contents, this);
    }
  }
}

void ExtensionTestNotificationObserver::NotificationSet::
    OnExtensionFrameUnregistered(const std::string& extension_id,
                                 content::RenderFrameHost* render_frame_host) {
  closure_list_.Notify();
}

void ExtensionTestNotificationObserver::NotificationSet::WebContentsDestroyed(
    content::WebContents* web_contents) {
  web_contents_observers_.erase(web_contents);
  closure_list_.Notify();
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver

ExtensionTestNotificationObserver::ExtensionTestNotificationObserver(
    content::BrowserContext* context)
    : context_(context),
      crx_installers_done_observed_(0) {
  if (context_)
    registry_observation_.Observe(ExtensionRegistry::Get(context_));
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
  observer_ =
      std::make_unique<content::WindowedNotificationObserver>(type, source);
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
  registry_observation_.Reset();
}

void ExtensionTestNotificationObserver::WaitForCondition(
    const ConditionCallback& condition,
    NotificationSet* notification_set) {
  if (condition.Run())
    return;
  condition_ = condition;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  base::CallbackListSubscription subscription;
  if (notification_set) {
    subscription = notification_set->closure_list().Add(base::BindRepeating(
        &ExtensionTestNotificationObserver::MaybeQuit, base::Unretained(this)));
  }
  run_loop.Run();

  condition_.Reset();
  quit_closure_.Reset();
}

void ExtensionTestNotificationObserver::MaybeQuit() {
  // We can be called synchronously from any of the events being observed,
  // so return immediately if the closure has already been run.
  if (quit_closure_.is_null())
    return;

  if (condition_.Run())
    std::move(quit_closure_).Run();
}

}  // namespace extensions
