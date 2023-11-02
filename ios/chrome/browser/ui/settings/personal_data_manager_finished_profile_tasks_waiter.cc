// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"

#include "components/autofill/core/browser/personal_data_manager.h"

PersonalDataManagerFinishedProfileTasksWaiter::
    PersonalDataManagerFinishedProfileTasksWaiter(
        autofill::PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {
  personal_data_manager_->AddObserver(this);
}

PersonalDataManagerFinishedProfileTasksWaiter::
    ~PersonalDataManagerFinishedProfileTasksWaiter() {
  personal_data_manager_->RemoveObserver(this);
}

void PersonalDataManagerFinishedProfileTasksWaiter::Wait() {
  // If a test is blocked in that method, it means that OnPersonalDataChanged()
  // was never called which indicates a bug in either the expectation of the
  // test (no asynchronous operation is executed on the PersonalDataManager
  // passed in the constructor) or in the utilisation of the
  // PersonalDataManagerFinishedProfileTasksWaiter (there is a race-condition
  // between sending the asynchronous operation and the creation of the
  // observer, it can be fixed by creating the observer before sending the
  // operation).
  run_loop_.Run();
}

void PersonalDataManagerFinishedProfileTasksWaiter::OnPersonalDataChanged() {}

void PersonalDataManagerFinishedProfileTasksWaiter::
    OnPersonalDataFinishedProfileTasks() {
  run_loop_.QuitWhenIdle();
}
