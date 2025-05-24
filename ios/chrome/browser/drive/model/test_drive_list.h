// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_LIST_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_LIST_H_

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/drive/model/drive_list.h"

// Testing implementation for `DriveList`.
class TestDriveList final : public DriveList {
 public:
  explicit TestDriveList(id<SystemIdentity> identity);
  ~TestDriveList() final;

  // Returns values reported by callbacks of `DriveList` methods.
  DriveListResult GetDriveListResult() const;

  // Sets DriveListResult to be reported by `ListItems()`.
  void SetDriveListResult(const DriveListResult& result);

  // Set quit closures.
  void SetListItemsCompletionQuitClosure(base::RepeatingClosure quit_closure);

  // DriveList implementation
  id<SystemIdentity> GetIdentity() const final;
  bool IsExecutingQuery() const final;
  void CancelCurrentQuery() final;
  void ListFiles(const DriveListQuery& query,
                 DriveListCompletionCallback completion_callback) final;

 private:
  // Run quit closures.
  void RunListItemsCompletionQuitClosure();

  // Calls `completion_callback` with `drive_list_result` and calls
  // `list_items_completion_quit_closure_`.
  void ReportDriveListResult(DriveListCompletionCallback completion_callback,
                             const DriveListResult& drive_list_result);

  id<SystemIdentity> identity_;

  // Results/progress to be reported by callbacks of `DriveList` methods. If one
  // of these values is not set, a default value will be reported instead.
  std::optional<DriveListResult> drive_list_result_;

  // Quit closures.
  base::RepeatingClosure list_items_completion_quit_closure_ =
      base::DoNothing();

  // Weak pointer factory, for callbacks. Can be used to cancel any pending
  // tasks by invalidating all weak pointers.
  base::WeakPtrFactory<TestDriveList> callbacks_weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_LIST_H_
