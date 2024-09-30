// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_IMPL_H_
#define IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_IMPL_H_

#import <vector>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/sessions/core/tab_restore_service_observer.h"
#import "ios/chrome/browser/external_files/model/external_file_remover.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

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
  ExternalFileRemoverImpl(ProfileIOS* profile,
                          sessions::TabRestoreService* tab_restore_service);

  ExternalFileRemoverImpl(const ExternalFileRemoverImpl&) = delete;
  ExternalFileRemoverImpl& operator=(const ExternalFileRemoverImpl&) = delete;

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
  // Removes all files received from other apps if `all_files` is true.
  // Otherwise, removes the unreferenced files only. `closure_runner` is called
  // when the removal finishes.
  void Remove(bool all_files, base::ScopedClosureRunner closure_runner);
  // Removes files received from other apps. If `all_files` is true, then
  // all files including files that may be referenced by tabs through restore
  // service or history. Otherwise, only the unreferenced files are removed.
  // `closure_runner` is called when the removal finishes.
  void RemoveFiles(bool all_files, base::ScopedClosureRunner closure_runner);
  // Returns all Referenced External files.
  NSSet* GetReferencedExternalFiles();
  // Vector used to store delayed requests.
  std::vector<DelayedFileRemoveRequest> delayed_file_remove_requests_;
  // Pointer to the tab restore service.
  raw_ptr<sessions::TabRestoreService> tab_restore_service_ = nullptr;
  // ProfileIOS used to get the referenced files. Must outlive this
  // object.
  raw_ptr<ProfileIOS> profile_ = nullptr;
  // Used to ensure that this class' methods are called on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  // Used to ensure `Remove()` is not run when this object is destroyed.
  base::WeakPtrFactory<ExternalFileRemoverImpl> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_IMPL_H_
