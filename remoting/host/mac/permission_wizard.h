// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_PERMISSION_WIZARD_H_
#define REMOTING_HOST_MAC_PERMISSION_WIZARD_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {
namespace mac {

// This class implements a wizard-style UI which guides the user to granting all
// needed MacOS permissions for the host process.
class PermissionWizard final {
 public:
  class Impl;

  // Callback for the Delegate to inform this class whether a permission was
  // granted. Also used to inform the caller of Start() whether the wizard
  // was completed successfully or cancelled.
  using ResultCallback = base::OnceCallback<void(bool granted)>;

  // Interface to delegate the permission-checks. This will be invoked to test
  // each permission, and will return (via callback) whether the permission was
  // granted.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the name of the bundle that needs to be granted permission, as
    // it would appear in the System Preferences applet. This will be included
    // in the dialog's instructional text.
    virtual std::string GetBundleName() = 0;

    // These checks will be invoked on the UI thread, and the result should be
    // returned to the callback on the same thread.
    // They should cause the bundle-name to be added to System Preferences
    // applet. As far as possible, the check should not trigger a system prompt,
    // but this may be unavoidable.
    virtual void CheckAccessibilityPermission(ResultCallback onResult) = 0;
    virtual void CheckScreenRecordingPermission(ResultCallback onResult) = 0;
  };

  explicit PermissionWizard(std::unique_ptr<Delegate> checker);
  PermissionWizard(const PermissionWizard&) = delete;
  PermissionWizard& operator=(const PermissionWizard&) = delete;
  ~PermissionWizard();

  // Sets an optional callback to be notified when the wizard finishes. If set,
  // the callback will be run on the ui_task_runner provided to Start(). The
  // result will be true if all permissions were granted (even if no pages were
  // actually shown). Result is false if the user cancelled the wizard (which
  // should only happen if a page was shown and the associated permission was
  // not granted).
  void SetCompletionCallback(ResultCallback callback);

  void Start(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

 private:
  // Private implementation, to hide the Objective-C and Cocoa objects from C++
  // callers.
  std::unique_ptr<Impl> impl_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

}  // namespace mac
}  // namespace remoting

#endif  // REMOTING_HOST_MAC_PERMISSION_WIZARD_H_
