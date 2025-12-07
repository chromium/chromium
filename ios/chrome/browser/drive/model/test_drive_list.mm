// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_list.h"

#import "base/task/single_thread_task_runner.h"
#import "base/time/time.h"

namespace {

// Time constant used to post delayed tasks and simulate Drive list requests.
constexpr base::TimeDelta kTestDriveListTimeConstant = base::Milliseconds(100);

}  // namespace

TestDriveList::TestDriveList(id<SystemIdentity> identity)
    : identity_(identity) {}

TestDriveList::~TestDriveList() = default;

DriveListResult TestDriveList::GetDriveListResult() const {
  if (drive_list_result_) {
    return *drive_list_result_;
  }
  DriveListResult result;
  for (int i = 0; i < 10; ++i) {
    DriveItem item;
    item.identifier = [[NSUUID UUID] UUIDString];
    item.name = [NSString stringWithFormat:@"Fake Item %d", i];
    result.items.push_back(item);
  }
  return result;
}

void TestDriveList::SetDriveListResult(const DriveListResult& result) {
  drive_list_result_ = result;
}

void TestDriveList::SetListItemsCompletionQuitClosure(
    base::RepeatingClosure quit_closure) {
  list_items_completion_quit_closure_ = std::move(quit_closure);
}

#pragma mark - DriveList

id<SystemIdentity> TestDriveList::GetIdentity() const {
  return identity_;
}

bool TestDriveList::IsExecutingQuery() const {
  return callbacks_weak_ptr_factory_.HasWeakPtrs();
}

void TestDriveList::CancelCurrentQuery() {
  callbacks_weak_ptr_factory_.InvalidateWeakPtrs();
}

void TestDriveList::ListFiles(const DriveListQuery& query,
                              DriveListCompletionCallback completion_callback) {
  const auto completion_quit_closure =
      base::BindRepeating(&TestDriveList::RunListItemsCompletionQuitClosure,
                          callbacks_weak_ptr_factory_.GetWeakPtr());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestDriveList::ReportDriveListResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), GetDriveListResult())
          .Then(completion_quit_closure),
      kTestDriveListTimeConstant);
}

#pragma mark - Private

void TestDriveList::ReportDriveListResult(
    DriveListCompletionCallback completion_callback,
    const DriveListResult& list_items_result) {
  std::move(completion_callback).Run(list_items_result);
}

void TestDriveList::RunListItemsCompletionQuitClosure() {
  list_items_completion_quit_closure_.Run();
}
