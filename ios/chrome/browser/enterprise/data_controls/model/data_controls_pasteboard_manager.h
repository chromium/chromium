// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_PASTEBOARD_MANAGER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_PASTEBOARD_MANAGER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/no_destructor.h"
#import "base/sequence_checker.h"
#import "url/gurl.h"

@class PasteboardObserver;
@class UIPasteboard;

class ProfileIOS;

namespace data_controls {

// Source of the data on the pasteboard, used for evaluating Data Controls
// policies.
struct PasteboardSource {
  // The URL of the page where the data was copied from.
  GURL source_url;

  // Non-owning pointer to the ProfileIOS owning the tab the pasteboard content
  // originated from.
  raw_ptr<ProfileIOS> source_profile;
};

// Manages pasteboard source for Data Controls policies.
//
// This class is responsible for associating content on the system pasteboard
// with its origin within the application, for the purpose of enforcing
// data protection rules.
//
// General flow:
//
// 1.  **Setting the Source:** When the application is about to copy data to the
//     pasteboard, it must first inform the `DataControlsPasteboardManager`
//     about the origin of this data. This is done by calling
//     `SetNextPasteboardItemsSource()` with the source URL and profile. This
//     call should happen *before* the data is placed on the system pasteboard.
//
// 2.  **Pasteboard Update:** The application or the user then performs the copy
//     action, which updates the system pasteboard's content and increments its
//     `changeCount`.
//
// 3.  **Change Detection:** The `PasteboardObserver` detects that the system
//     pasteboard has been modified. This can happen through system
//     notifications (`UIPasteboardChangedNotification`) or by checking the
//     `changeCount` when the app returns to the foreground.
//
// 4.  **Source Association:** Upon detecting a change, the
//     `DataControlsPasteboardManager` links the most recently set source
//     information (from step 1) with the new content on the pasteboard.
//
// 5.  **Retrieving the Source:** When a paste action is attempted, other parts
//     of the application can query the manager using
//     `GetCurrentPasteboardItemsSource()` to determine the origin of the data
//     currently on the pasteboard.
//
// 6.  **Source Invalidation:** If the pasteboard is changed again *without* a
//     preceding call to `SetNextPasteboardItemsSource()` (e.g., the user copies
//     from another app, or copies again within the app in a flow that doesn't
//     set the source), the manager invalidates the previous source association.
//     Subsequent calls to `GetCurrentPasteboardItemsSource()` will indicate an
//     unknown source until a new source is set and a copy occurs.
//
class DataControlsPasteboardManager {
 public:
  DataControlsPasteboardManager(const DataControlsPasteboardManager&) = delete;
  DataControlsPasteboardManager& operator=(
      const DataControlsPasteboardManager&) = delete;

  static DataControlsPasteboardManager* GetInstance();

  // Sets the source for the next content that will be written to the
  // pasteboard. This should be called before the data is written to the
  // pasteboard.
  void SetNextPasteboardItemsSource(GURL source_url,
                                    ProfileIOS* source_profile,
                                    bool os_clipboard_allowed);

  // Returns the source for the current content on the pasteboard. Returns a
  // default constructed `PasteboardSource` when the source is not known.
  PasteboardSource GetCurrentPasteboardItemsSource();

  // Restores the items copied by the user if they were replaced with a
  // placeholder. No-op for items that were not replaced with a placeholder.
  void RestoreItemsToGeneralPasteboardIfNeeded(base::OnceClosure callback);

  // Replaces the items in the general Pasteboard if `pasteboard_state_`
  // indicates that the items can't remain in the general Pasteboard.
  void RestorePlaceholderToGeneralPasteboardIfNeeded();

 private:
  friend class DataControlsPasteboardManagerTest;
  friend class DataControlsTabHelperTest;
  friend class base::NoDestructor<DataControlsPasteboardManager>;

  // Holds information about the items in the Pasteboard.
  struct PasteboardState {
    // Url of the page where the Pasteboard items were copied from.
    GURL source_url;
    // Name of the ProfileIOS owning the tab where the Pasteboard items
    // originated from. When the items originated from an incognito ProfileIOS,
    // holds the name of the corresponding non-incognito profile.
    std::string source_profile_name;
    // Whether the source ProfileIOS was incognito.
    bool source_profile_incognito;
    // True if the items can remain in the Pasteboard. False for items that
    // should be replaced with a placeholder so they can't be freely copied
    // without Data Control rules evaluation.
    bool os_clipboard_allowed = true;
    // Copy of the items in the Pasteboard.
    NSArray<NSDictionary<NSString*, id>*>* items;
  };

  // Describes the possible states of DataControlsPasteboardManager
  enum class Stage {
    // The manager does not know the source for the current pasteboard items.
    kUnknownSource,
    // The manager knows the source for the the items that are about to be
    // written pasteboard.
    kPendingSource,
    // The manager is replacing the items in the pasteboard. This happens when
    // the items stored by the user are replaced with a placeholder or when said
    // items are being temporarily restored so they can be pasted to a
    // destination approved by Data Control rules.
    kReplacingItems,
    // The manager knows the source for the current pasteboard items.
    kKnownSource,

  };

  DataControlsPasteboardManager();
  ~DataControlsPasteboardManager();

  // Called when the pasteboard content changes.
  void OnPasteboardChanged(UIPasteboard* pasteboard);

  // Resets the manager to its initial state. For testing purposes only.
  void ResetForTesting();

  // Initializes the manager to its initial state.
  void Initialize();

  // Information about the pasteboard items.
  PasteboardState pasteboard_state_;

  // Helper object to observe pasteboard notifications.
  __strong PasteboardObserver* pasteboard_observer_;

  Stage stage_ = Stage::kUnknownSource;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_PASTEBOARD_MANAGER_H_
