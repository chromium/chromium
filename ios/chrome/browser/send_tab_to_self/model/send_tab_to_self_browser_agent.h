// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_BROWSER_AGENT_H_

#import <CoreFoundation/CoreFoundation.h>

#import <optional>
#import <string>
#import <vector>

#import "base/containers/span.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/receiving_ui_handler.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"
#import "ios/web/public/web_state_observer.h"

class GURL;
@protocol SnackbarCommands;
class UrlLoadingNotifierBrowserAgent;
struct SendTabToSelfTextFragment;
struct UrlLoadParams;
class WebStateList;

namespace web {
class WebState;
}

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfModel;
}  // namespace send_tab_to_self

// A browser agent that coordinates Send Tab To Self actions.
class SendTabToSelfBrowserAgent
    : public BrowserUserData<SendTabToSelfBrowserAgent>,
      public send_tab_to_self::SendTabToSelfModelObserver,
      public send_tab_to_self::ReceivingUiHandler,
      public WebStateListObserver,
      public web::WebStateObserver,
      public UrlLoadingObserver,
      public BrowserObserver {
 public:
  using SendResultCallback =
      base::OnceCallback<void(send_tab_to_self::SendTabToSelfResult)>;

  ~SendTabToSelfBrowserAgent() override;

  // BrowserObserver::
  void BrowserDestroyed(Browser* browser) override;

  // Sends the tab with `url` and `title` to the target device with
  // `target_guid` and `target_device_name`. Dispatches appropriate snackbar
  // commands on completion and invokes `send_result_callback` if provided.
  void SendTabToTargetDevice(
      const GURL& url,
      const std::string& title,
      const std::string& target_guid,
      const std::string& target_device_name,
      send_tab_to_self::ShareEntryPoint entry_point,
      SendResultCallback send_result_callback = base::NullCallback());

  // Test-only wrapper for internal callback handler.
  void HandleEntrySentForTest(id<SnackbarCommands> snackbar_commands,
                              const std::string& target_device_name,
                              send_tab_to_self::SendTabToSelfResult result);

  // SendTabToSelfModelObserver::
  void OnEntriesAddedRemotely(
      base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries)
      override;
  void OnEntriesRemovedRemotely(base::span<const std::string> guids) override;
  void OnModelReady() override;

  // ReceivingUiHandler::
  void DisplayNewEntries(
      base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries)
      override;
  void DismissEntries(base::span<const std::string> guids) override;

  // WebStateListObserver::
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // WebStateObserver::
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // UrlLoadingObserver::
  void TabWillLoadUrl(const UrlLoadParams& params,
                      base::WeakPtr<web::WebState> web_state) override;

 private:
  friend class BrowserUserData<SendTabToSelfBrowserAgent>;
  friend class SendTabToSelfBrowserAgentExecutionTest;

  explicit SendTabToSelfBrowserAgent(Browser* browser);

  // Callback invoked when text fragment generation completes for a shared URL.
  // Enriches `page_context` with scroll position fragment data if present,
  // inserts the Send Tab to Self entry into the model, and dispatches snackbar
  // notifications.
  void HandleTextFragmentGenerated(
      const GURL& url,
      const std::string& title,
      const std::string& target_guid,
      const std::string& target_device_name,
      send_tab_to_self::ShareEntryPoint entry_point,
      send_tab_to_self::PageContext page_context,
      SendResultCallback send_result_callback,
      std::optional<SendTabToSelfTextFragment> text_fragment);

  // Callback invoked when an entry is sent to the target device.
  // Dispatches post-send snackbar feedback, triggers haptic feedback on
  // success, and runs `send_result_callback` if provided.
  void HandleEntrySent(id<SnackbarCommands> snackbar_commands,
                       const std::string& target_device_name,
                       SendResultCallback send_result_callback,
                       send_tab_to_self::SendTabToSelfResult result);

  // Checks if there are any unopened entries targeted to the local device
  // and auto-opens them as background tabs.
  void CheckAndOpenPendingEntriesIfBrowserVisible();

  // Opens `entry` in a new background tab and marks it as opened.
  void OpenEntryInBackgroundTab(
      const send_tab_to_self::SendTabToSelfEntry* entry);

  // Display an infobar for `entry` on the specified `web_state`.
  void DisplayInfoBar(web::WebState* web_state,
                      const send_tab_to_self::SendTabToSelfEntry* entry,
                      size_t opened_tab_count);

  // Stop observing the WebState and WebStateList and reset associated
  // variables.
  void CleanUpObserversAndVariables();

  // Owned by the SendTabToSelfSyncService which should outlive this class
  raw_ptr<send_tab_to_self::SendTabToSelfModel> model_ = nullptr;

  // The GUID of the pending SendTabToSelf entry to display an InfoBar for,
  // if any.
  std::optional<std::string> pending_entry_guid_;

  // The WebState that is being observed for activation, if any.
  raw_ptr<web::WebState> pending_web_state_ = nullptr;

  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  base::ScopedObservation<UrlLoadingNotifierBrowserAgent, UrlLoadingObserver>
      url_loading_observation_{this};

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::ScopedObservation<send_tab_to_self::SendTabToSelfModel,
                          send_tab_to_self::SendTabToSelfModelObserver>
      model_observation_{this};

  base::WeakPtrFactory<SendTabToSelfBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_BROWSER_AGENT_H_
