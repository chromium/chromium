// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_test_notification_observer.h"
#include "base/memory/raw_ptr.h"

#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

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
  void DidStopLoading() override { owner_->DidStopLoading(web_contents()); }

  void WebContentsDestroyed() override {
    // Do not add code after this line, deletes `this`.
    owner_->WebContentsDestroyed(web_contents());
  }

  raw_ptr<ExtensionTestNotificationObserver::NotificationSet> owner_;
};

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver::NotificationSet

ExtensionTestNotificationObserver::NotificationSet::NotificationSet(
    ProcessManager* manager) {
  process_manager_observation_.Observe(manager);

  std::set<content::WebContents*> initial_extension_contents;
  for (content::RenderFrameHost* rfh : manager->GetAllFrames()) {
    initial_extension_contents.insert(
        content::WebContents::FromRenderFrameHost(rfh));
  }
  for (content::WebContents* web_contents : initial_extension_contents) {
    StartObservingWebContents(web_contents);
  }
}

ExtensionTestNotificationObserver::NotificationSet::~NotificationSet() =
    default;

void ExtensionTestNotificationObserver::NotificationSet::
    OnExtensionFrameUnregistered(const ExtensionId& extension_id,
                                 content::RenderFrameHost* render_frame_host) {
  closure_list_.Notify();
}

void ExtensionTestNotificationObserver::NotificationSet::OnWebContentsCreated(
    content::WebContents* web_contents) {
  StartObservingWebContents(web_contents);
}

void ExtensionTestNotificationObserver::NotificationSet::
    StartObservingWebContents(content::WebContents* web_contents) {
  CHECK(!base::Contains(web_contents_observers_, web_contents));
  web_contents_observers_[web_contents] =
      std::make_unique<ForwardingWebContentsObserver>(web_contents, this);
}

void ExtensionTestNotificationObserver::NotificationSet::DidStopLoading(
    content::WebContents* web_contents) {
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
    : context_(context) {}

ExtensionTestNotificationObserver::~ExtensionTestNotificationObserver() =
    default;

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
