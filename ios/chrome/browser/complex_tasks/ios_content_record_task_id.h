// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPLEX_TASKS_IOS_CONTENT_RECORD_TASK_ID_H_
#define IOS_CHROME_BROWSER_COMPLEX_TASKS_IOS_CONTENT_RECORD_TASK_ID_H_

#include <stdint.h>

#include <string>
#include <vector>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Stores Task ID data in a NavigationItem. Task IDs track navigations and
// relationships between navigations
//
// TODO(crbug.com/976501)
// Move common logic to components/sessionss/core and have
// IOSContentRecordTaskId inherit from there. Common logic in
// components/sessions/content/content_record_task_id.h will inherit from
// components/sessions/core as well
class IOSContentRecordTaskId {
 public:
  IOSContentRecordTaskId();
  IOSContentRecordTaskId(const IOSContentRecordTaskId& content_record_task_id);
  ~IOSContentRecordTaskId();

  int64_t task_id() const { return task_id_; }

  int64_t parent_task_id() const { return parent_task_id_; }

  int64_t root_task_id() const { return root_task_id_; }

  const std::vector<int64_t>& children_task_ids() const {
    return children_task_ids_;
  }

  void set_task_id(int64_t task_id) { task_id_ = task_id; }

  void set_parent_task_id(int64_t parent_task_id) {
    parent_task_id_ = parent_task_id;
  }

  void set_root_task_id(int64_t root_task_id) { root_task_id_ = root_task_id; }

  void set_children_task_ids(const std::vector<int64_t>& children_task_ids) {
    children_task_ids_ = children_task_ids;
  }

 private:
  // A Task is a collection of navigations.
  //
  // A Task ID is an identifier of a Task. It is a Unique ID upon the first
  // navigation - navigating via the back button will not create a new ID but
  // the ID upon the first navigation will be used.
  //
  // A Parent Task ID is the identifier for the previous task in a series of
  // navigations.
  //
  // A Child Task ID is a subsequent Task ID in a series of navigations. This is
  // not recursive. There can be multiple Child Task IDs in the event that
  // multiple tabs are opened from a page.
  //
  // A Root Task ID is the first Task ID in a collection of navigations. Root
  // Task IDs are tracked for task clustering in the event that an intermediate
  // Tab is closed. It is not possible to group the tasks via a tree traversal
  // in this situation.
  int64_t task_id_ = -1;
  int64_t parent_task_id_ = -1;
  int64_t root_task_id_ = -1;
  std::vector<int64_t> children_task_ids_;
};

#endif  // IOS_CHROME_BROWSER_COMPLEX_TASKS_IOS_CONTENT_RECORD_TASK_ID_H_
