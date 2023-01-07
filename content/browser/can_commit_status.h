// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CAN_COMMIT_STATUS_H_
#define CONTENT_BROWSER_CAN_COMMIT_STATUS_H_

namespace content {
// Status type used by functions that check whether it is ok to commit
// a particular combination of URL and Origin to a specific process.
// It indicates that a commit is allowed or signals whether the URL or
// origin prevented the commit.
enum class CanCommitStatus {
  CAN_COMMIT_ORIGIN_AND_URL,
  CANNOT_COMMIT_ORIGIN,
  CANNOT_COMMIT_URL
};

}  // namespace content
#endif  // CONTENT_BROWSER_CAN_COMMIT_STATUS_H_
