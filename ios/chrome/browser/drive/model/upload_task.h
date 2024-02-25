// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_UPLOAD_TASK_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_UPLOAD_TASK_H_

#import "base/observer_list.h"

class GURL;
@protocol SystemIdentity;
class UploadTaskObserver;

// Provides API for a single browser upload task. This is the model class that
// stores all the state for an upload.
class UploadTask {
 public:
  // Possible states of the upload task.
  enum class State {
    // Upload has not started yet.
    kNotStarted = 0,
    // Upload is actively progressing.
    kInProgress,
    // Upload is cancelled.
    kCancelled,
    // Upload is completely finished.
    kComplete,
    // Upload has failed but can be retried.
    kFailed,
  };

  UploadTask();
  virtual ~UploadTask();

  // Returns the upload task state.
  virtual State GetState() const = 0;
  // Returns whether the task is complete.
  bool IsDone() const;

  // Starts the upload.
  virtual void Start() = 0;
  // Cancels the upload.
  virtual void Cancel() = 0;

  // Returns the identity used to upload the file, if any.
  virtual id<SystemIdentity> GetIdentity() const = 0;
  // Returns the upload progress from 0 to 1.
  virtual float GetProgress() const = 0;
  // Returns the response link to the uploaded file, if any.
  // If `add_user_identifier` is `true`, then the returned link is modified to
  // include the identifier of the uploading identity.
  virtual std::optional<GURL> GetResponseLink(
      bool add_user_identifier = false) const = 0;
  // Returns the error object for this upload task, in case of failure.
  virtual NSError* GetError() const = 0;

  // Adds and removes UploadTaskObserver. Clients must remove self from
  // observers before the task is destroyed.
  void AddObserver(UploadTaskObserver* observer);
  void RemoveObserver(UploadTaskObserver* observer);

 protected:
  // Called when upload task was updated.
  void OnUploadUpdated();

 private:
  // A list of observers. At destruction, it *will* check that all observers
  // have removed themselves from this list. Weak references.
  base::ObserverList<UploadTaskObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_UPLOAD_TASK_H_
