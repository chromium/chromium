// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_EXPERIMENTS_H_
#define NET_DISK_CACHE_BLOCKFILE_EXPERIMENTS_H_

namespace disk_cache {

// This lists the experiment groups that we care about. Only add new groups at
// the end of the list, and always increase the number.
enum {
  NO_EXPERIMENT = 0,
  EXPERIMENT_OLD_FILE1 = 3,
  EXPERIMENT_OLD_FILE2 = 4,
  EXPERIMENT_DELETED_LIST_OUT = 11,
  EXPERIMENT_DELETED_LIST_CONTROL = 12,
  EXPERIMENT_DELETED_LIST_IN = 13,
  EXPERIMENT_DELETED_LIST_OUT2 = 14,
  // There is no EXPERIMENT_SIMPLE_YES since this enum is used in the standard
  // backend only.
  EXPERIMENT_SIMPLE_CONTROL = 15,
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_EXPERIMENTS_H_
