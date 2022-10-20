// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
#define EXTENSIONS_TEST_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"

namespace content {
class BrowserContext;
class NotificationDetails;
class WebContents;
class WindowedNotificationObserver;
}

namespace extensions {

// Test helper class for observing extension-related events.
class ExtensionTestNotificationObserver : public content::NotificationObserver,
                                          ExtensionRegistryObserver {
 public:
  explicit ExtensionTestNotificationObserver(content::BrowserContext* context);

  ExtensionTestNotificationObserver(const ExtensionTestNotificationObserver&) =
      delete;
  ExtensionTestNotificationObserver& operator=(
      const ExtensionTestNotificationObserver&) = delete;

  ~ExtensionTestNotificationObserver() override;

  // Wait for the crx installer to be done. Returns true if it has finished
  // successfully.
  bool WaitForCrxInstallerDone();

  // Watch for the given event type from the given source.
  // After calling this method, call Wait() to ensure that RunMessageLoop() is
  // called appropriately and cleanup is performed.
  void Watch(int type, const content::NotificationSource& source);

  // After registering one or more event types with Watch(), call
  // this method to run the message loop and perform cleanup.
  void Wait();

  const std::string& last_loaded_extension_id() {
    return last_loaded_extension_id_;
  }
  void set_last_loaded_extension_id(
      const std::string& last_loaded_extension_id) {
    last_loaded_extension_id_ = last_loaded_extension_id;
  }

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnShutdown(ExtensionRegistry* registry) override;

 protected:
  class NotificationSet : public content::NotificationObserver,
                          public extensions::ProcessManagerObserver {
   public:
    NotificationSet();

    NotificationSet(const NotificationSet&) = delete;
    NotificationSet& operator=(const NotificationSet&) = delete;

    ~NotificationSet() override;

    void Add(int type, const content::NotificationSource& source);
    void Add(int type);
    void AddExtensionFrameUnregistration(extensions::ProcessManager* manager);
    void AddWebContentsDestroyed(extensions::ProcessManager* manager);

    // Notified any time an Add()ed notification is received.
    // The details of the notification are dropped.
    base::RepeatingClosureList& closure_list() { return closure_list_; }

   private:
    class ForwardingWebContentsObserver;

    // content::NotificationObserver:
    void Observe(int type,
                 const content::NotificationSource& source,
                 const content::NotificationDetails& details) override;

    // extensions::ProcessManagerObserver:
    void OnExtensionFrameUnregistered(
        const std::string& extension_id,
        content::RenderFrameHost* render_frame_host) override;

    void WebContentsDestroyed(content::WebContents* web_contents);

    content::NotificationRegistrar notification_registrar_;
    base::RepeatingClosureList closure_list_;
    base::ScopedObservation<extensions::ProcessManager,
                            extensions::ProcessManagerObserver>
        process_manager_observation_{this};

    std::map<content::WebContents*,
             std::unique_ptr<ForwardingWebContentsObserver>>
        web_contents_observers_;
  };

  // Wait for |condition_| to be met. |notification_set| is the set of
  // notifications to wait for and to check |condition| when observing. This
  // can be NULL if we are instead waiting for a different observer method, like
  // OnPageActionsUpdated().
  void WaitForCondition(const base::RepeatingCallback<bool(void)>& condition,
                        NotificationSet* notification_set);

  void WaitForNotification(int notification_type);

  // Quits the message loop if |condition_| is met.
  void MaybeQuit();

  raw_ptr<content::BrowserContext, DanglingUntriaged> context_;

 private:
  content::NotificationRegistrar registrar_;
  std::unique_ptr<content::WindowedNotificationObserver> observer_;

  std::string last_loaded_extension_id_;
  int crx_installers_done_observed_;

  // The condition for which we are waiting. This should be checked in any
  // observing methods that could trigger it.
  base::RepeatingCallback<bool(void)> condition_;

  // The closure to quit the currently-running message loop.
  base::OnceClosure quit_closure_;

  // Listens to extension loaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
