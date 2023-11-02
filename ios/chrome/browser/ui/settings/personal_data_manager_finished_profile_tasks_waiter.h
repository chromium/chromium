// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PERSONAL_DATA_MANAGER_FINISHED_PROFILE_TASKS_WAITER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PERSONAL_DATA_MANAGER_FINISHED_PROFILE_TASKS_WAITER_H_

#include "base/run_loop.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {
class PersonalDataManager;
}

// Helper class to allow waiting until the asynchronous operation on the
// autofill::PersonalDataManager completed. Need to be used like this:
//
//   autofill::PersonalDataManager* personal_data_manager = ...;
//   PersonalDataManagerFinishedProfileTasksWaiter
//   observer(personal_data_manager); personal_data_manager->...();  // Starts
//   some asynchronous operation. observer.Wait();
//
class PersonalDataManagerFinishedProfileTasksWaiter
    : public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataManagerFinishedProfileTasksWaiter(
      autofill::PersonalDataManager* personal_data_manager);

  PersonalDataManagerFinishedProfileTasksWaiter(
      const PersonalDataManagerFinishedProfileTasksWaiter&) = delete;
  PersonalDataManagerFinishedProfileTasksWaiter& operator=(
      const PersonalDataManagerFinishedProfileTasksWaiter&) = delete;

  ~PersonalDataManagerFinishedProfileTasksWaiter() override;

  // Blocks until `OnPersonalDataFinishedProfileTasks` is invoked at the end of
  // the asynchronous modification on the PersonalDataManager.
  void Wait();

  // autofill::PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override;
  void OnPersonalDataFinishedProfileTasks() override;

 private:
  autofill::PersonalDataManager* personal_data_manager_;
  base::RunLoop run_loop_;
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PERSONAL_DATA_MANAGER_FINISHED_PROFILE_TASKS_WAITER_H_
