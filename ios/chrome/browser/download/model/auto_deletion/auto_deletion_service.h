// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_

class PrefRegistrySimple;

namespace auto_deletion {

// Service responsible for the orchestration of the various functionality within
// the auto-deletion system.
class AutoDeletionService {
 public:
  AutoDeletionService();
  ~AutoDeletionService();

  // Registers the auto deletion Chrome settings status.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
};

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_SERVICE_H_
