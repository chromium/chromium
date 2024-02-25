// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
#define EXTENSIONS_TEST_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

// Test helper class for observing extension-related events.
class ExtensionTestNotificationObserver {
 public:
  explicit ExtensionTestNotificationObserver(content::BrowserContext* context);

  ExtensionTestNotificationObserver(const ExtensionTestNotificationObserver&) =
      delete;
  ExtensionTestNotificationObserver& operator=(
      const ExtensionTestNotificationObserver&) = delete;

  ~ExtensionTestNotificationObserver();

 protected:
  class NotificationSet : public extensions::ProcessManagerObserver {
   public:
    explicit NotificationSet(ProcessManager* manager);

    NotificationSet(const NotificationSet&) = delete;
    NotificationSet& operator=(const NotificationSet&) = delete;

    ~NotificationSet() override;

    // Notified any time a notification is received.
    // The details of the notification are dropped.
    base::RepeatingClosureList& closure_list() { return closure_list_; }

   private:
    class ForwardingWebContentsObserver;

    // extensions::ProcessManagerObserver:
    void OnExtensionFrameUnregistered(
        const ExtensionId& extension_id,
        content::RenderFrameHost* render_frame_host) override;

    void OnWebContentsCreated(content::WebContents* web_contents);
    void StartObservingWebContents(content::WebContents* web_contents);

    void DidStopLoading(content::WebContents* web_contents);
    void WebContentsDestroyed(content::WebContents* web_contents);

    base::RepeatingClosureList closure_list_;

    base::ScopedObservation<extensions::ProcessManager,
                            extensions::ProcessManagerObserver>
        process_manager_observation_{this};

    base::CallbackListSubscription web_contents_creation_subscription_ =
        content::RegisterWebContentsCreationCallback(
            base::BindRepeating(&NotificationSet::OnWebContentsCreated,
                                base::Unretained(this)));

    std::map<content::WebContents*,
             std::unique_ptr<ForwardingWebContentsObserver>>
        web_contents_observers_;
  };

  // A callback that returns true if the condition has been met and takes no
  // arguments.
  using ConditionCallback = base::RepeatingCallback<bool(void)>;

  // Wait for |condition_| to be met. |notification_set| is the set of
  // notifications to wait for and to check |condition| when observing. This
  // can be NULL if we are instead waiting for a different observer method, like
  // OnPageActionsUpdated().
  void WaitForCondition(const ConditionCallback& condition,
                        NotificationSet* notification_set);

  // Quits the message loop if |condition_| is met.
  void MaybeQuit();

  raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged> context_;

 private:
  // The condition for which we are waiting. This should be checked in any
  // observing methods that could trigger it.
  ConditionCallback condition_;

  // The closure to quit the currently-running message loop.
  base::OnceClosure quit_closure_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
