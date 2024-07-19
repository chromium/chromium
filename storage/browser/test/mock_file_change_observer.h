// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_FILE_CHANGE_OBSERVER_H_
#define STORAGE_BROWSER_TEST_MOCK_FILE_CHANGE_OBSERVER_H_

#include "base/compiler_specific.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"

namespace storage {

// Mock file change observer.
class MockFileChangeObserver : public FileChangeObserver {
 public:
  MockFileChangeObserver();

  MockFileChangeObserver(const MockFileChangeObserver&) = delete;
  MockFileChangeObserver& operator=(const MockFileChangeObserver&) = delete;

  ~MockFileChangeObserver() override;

  // Creates a ChangeObserverList which only contains given |observer|.
  static ChangeObserverList CreateList(MockFileChangeObserver* observer);

  // FileChangeObserver overrides.
  void OnCreateFile(const FileSystemURL& url) override;
  void OnCreateFileFrom(const FileSystemURL& url,
                        const FileSystemURL& src) override;
  void OnMoveFileFrom(const FileSystemURL& url,
                      const FileSystemURL& src) override;
  void OnRemoveFile(const FileSystemURL& url) override;
  void OnModifyFile(const FileSystemURL& url) override;
  void OnCreateDirectory(const FileSystemURL& url) override;
  void OnRemoveDirectory(const FileSystemURL& url) override;

  void ResetCount() {
    create_file_count_ = 0;
    create_file_from_count_ = 0;
    remove_file_count_ = 0;
    modify_file_count_ = 0;
    create_directory_count_ = 0;
    remove_directory_count_ = 0;
    changed_urls_.clear();
  }

  bool HasNoChange() const {
    bool has_change = create_file_count_ || create_file_from_count_ ||
                      remove_file_count_ || modify_file_count_ ||
                      create_directory_count_ || remove_directory_count_;
    CHECK_NE(has_change, changed_urls_.empty());
    return !has_change;
  }

  const FileSystemURLSet& get_changed_urls() const { return changed_urls_; }

  int create_file_count() const { return create_file_count_; }
  int create_file_from_count() const { return create_file_from_count_; }
  int remove_file_count() const { return remove_file_count_; }
  int modify_file_count() const { return modify_file_count_; }
  int create_directory_count() const { return create_directory_count_; }
  int remove_directory_count() const { return remove_directory_count_; }

  int get_and_reset_create_file_count() {
    int tmp = create_file_count_;
    create_file_count_ = 0;
    changed_urls_.clear();
    return tmp;
  }
  int get_and_reset_create_file_from_count() {
    int tmp = create_file_from_count_;
    create_file_from_count_ = 0;
    changed_urls_.clear();
    return tmp;
  }
  int get_and_reset_remove_file_count() {
    int tmp = remove_file_count_;
    remove_file_count_ = 0;
    changed_urls_.clear();
    return tmp;
  }
  int get_and_reset_modify_file_count() {
    int tmp = modify_file_count_;
    modify_file_count_ = 0;
    changed_urls_.clear();
    return tmp;
  }
  int get_and_reset_create_directory_count() {
    int tmp = create_directory_count_;
    create_directory_count_ = 0;
    changed_urls_.clear();
    return tmp;
  }
  int get_and_reset_remove_directory_count() {
    int tmp = remove_directory_count_;
    remove_directory_count_ = 0;
    changed_urls_.clear();
    return tmp;
  }

 private:
  FileSystemURLSet changed_urls_;

  int create_file_count_;
  int create_file_from_count_;
  int remove_file_count_;
  int modify_file_count_;
  int create_directory_count_;
  int remove_directory_count_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_FILE_CHANGE_OBSERVER_H_
