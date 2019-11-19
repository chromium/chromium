// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_IMPL_H_
#define IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#import "ios/chrome/browser/external_files/external_file_remover.h"

namespace ios {
class ChromeBrowserState;
}
namespace sessions {
class TabRestoreService;
}

// Concrete implementation of ExternalFileRemover.
class ExternalFileRemoverImpl : public ExternalFileRemover,
                                public sessions::TabRestoreServiceObserver {
 public:
  // Creates an ExternalFileRemoverImpl to remove external documents not
  // referenced by the specified BrowserViewController. Use Remove to initiate
  // the removal.
  ExternalFileRemoverImpl(ios::ChromeBrowserState* browser_state,
                          sessions::TabRestoreService* tab_restore_service);
  ~ExternalFileRemoverImpl() override;

  // ExternalFileRemover methods.
  void RemoveAfterDelay(base::TimeDelta delay,
                        base::OnceClosure callback) override;

  // KeyedService methods
  void Shutdown() override;

  // sessions::TabRestoreServiceObserver methods
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  // Struct used to save information for delayed requests.
  struct DelayedFileRemoveRequest {
    bool remove_all_files;
    base::ScopedClosureRunner closure_runner;
  };
  // Removes all files received from other apps if |all_files| is true.
  // Otherwise, removes the unreferenced files only. |closure_runner| is called
  // when the removal finishes.
  void Remove(bool all_files, base::ScopedClosureRunner closure_runner);
  // Removes files received from other apps. If |all_files| is true, then
  // all files including files that may be referenced by tabs through restore
  // service or history. Otherwise, only the unreferenced files are removed.
  // |closure_runner| is called when the removal finishes.
  void RemoveFiles(bool all_files, base::ScopedClosureRunner closure_runner);
  // Returns all Referenced External files.
  NSSet* GetReferencedExternalFiles();
  // Vector used to store delayed requests.
  std::vector<DelayedFileRemoveRequest> delayed_file_remove_requests_;
  // Pointer to the tab restore service.
  sessions::TabRestoreService* tab_restore_service_ = nullptr;
  // ChromeBrowserState used to get the referenced files. Must outlive this
  // object.
  ios::ChromeBrowserState* browser_state_ = nullptr;
  // Used to ensure that this class' methods are called on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  // Used to ensure |Remove()| is not run when this object is destroyed.
  base::WeakPtrFactory<ExternalFileRemoverImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalFileRemoverImpl);
};

#endif  // IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_IMPL_H_
