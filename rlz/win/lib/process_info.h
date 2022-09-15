// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Information about the current process.

#ifndef RLZ_WIN_LIB_PROCESS_INFO_H_
#define RLZ_WIN_LIB_PROCESS_INFO_H_

namespace rlz_lib {

class ProcessInfo {
 public:
  ProcessInfo(const ProcessInfo&) = delete;
  ProcessInfo& operator=(const ProcessInfo&) = delete;

  // All these functions cache the result after first run.
  static bool IsRunningAsSystem();
  static bool HasAdminRights();  // System / Admin / High Elevation on Vista
};  // class
}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_PROCESS_INFO_H_
