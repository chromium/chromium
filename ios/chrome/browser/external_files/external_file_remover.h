// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_H_
#define IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_H_

#include "components/keyed_service/core/keyed_service.h"

// ExternalFileRemover is responsible for removing documents received from
// other applications that are not in the list of recently closed tabs, open
// tabs or bookmarks.
class ExternalFileRemover : public KeyedService {
 public:
  ExternalFileRemover() = default;
  ~ExternalFileRemover() override = default;
  // Post a delayed task to clean up the files received from other applications.
  // |callback| is called when the clean up has finished; it may be null.
  virtual void RemoveAfterDelay(base::TimeDelta delay,
                                base::OnceClosure callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalFileRemover);
};

#endif  // IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_H_
