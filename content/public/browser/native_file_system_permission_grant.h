// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace content {

// A ref-counted permission grant. This is needed so the implementation of
// this class can keep track of references to permissions, and clean up state
// when no more references remain. Multiple NativeFileSystemHandle instances
// can share the same permission grant. For example a directory and all its
// children will use the same grant.
//
// Instances of this class can be retrieved via a
// NativeFileSystemPermissionContext instance.
//
// NativeFileSystemPermissionGrant instances are not thread safe, and should
// only be used (and referenced) on the same sequence as the PermssionContext
// that created them, i.e. the UI thread.
class CONTENT_EXPORT NativeFileSystemPermissionGrant
    : public base::RefCounted<NativeFileSystemPermissionGrant> {
 public:
  using PermissionStatus = blink::mojom::PermissionStatus;

  virtual PermissionStatus GetStatus() = 0;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PermissionRequestOutcome {
    kBlockedByContentSetting = 0,
    kInvalidFrame = 1,
    kNoUserActivation = 2,
    kThirdPartyContext = 3,
    kUserGranted = 4,
    kUserDenied = 5,
    kUserDismissed = 6,
    kRequestAborted = 7,
    kMaxValue = kRequestAborted
  };

  // Call this method to request permission for this grant. The |callback|
  // should be called after the status of this grant has been updated with
  // the outcome of the request.
  virtual void RequestPermission(
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) = 0;

  // This observer can be used to be notified of changes to the permission
  // status of a grant.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPermissionStatusChanged() = 0;
  };

  // Per base::ObserverList, it is an error to attempt to add an observer more
  // than once.
  void AddObserver(Observer* observer);
  // Does nothing if the observer isn't in the list of observers.
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCounted<NativeFileSystemPermissionGrant>;
  NativeFileSystemPermissionGrant();
  virtual ~NativeFileSystemPermissionGrant();

  void NotifyPermissionStatusChanged();

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
