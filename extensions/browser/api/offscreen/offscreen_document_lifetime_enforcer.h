// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_DOCUMENT_LIFETIME_ENFORCER_H_
#define EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_DOCUMENT_LIFETIME_ENFORCER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace extensions {
class OffscreenDocumentHost;

// A class that allows for the enforcement of an offscreen document's lifetime.
// This is responsible for tracking the activity of an offscreen document and
// notifying when it changes, as well as terminating the document if a hard
// limit is encountered.
// An offscreen document will be terminated when either no lifetime enforcers
// detect the offscreen document as active or when the document is directly
// terminated.
class OffscreenDocumentLifetimeEnforcer {
 public:
  using TerminationCallback = base::OnceClosure;
  using NotifyInactiveCallback = base::RepeatingClosure;

  virtual ~OffscreenDocumentLifetimeEnforcer();

  OffscreenDocumentHost* offscreen_document() { return offscreen_document_; }

  // Returns true if the offscreen document is currently active for the
  // designated purpose.
  virtual bool IsActive() = 0;

 protected:
  OffscreenDocumentLifetimeEnforcer(
      OffscreenDocumentHost* offscreen_document,
      TerminationCallback termination_callback,
      NotifyInactiveCallback notify_inactive_callback);

  // Immediately terminates the offscreen document. This should be used to
  // shut down the offscreen document if some hard limit has been reached.
  void TerminateDocument();

  // Notifies the managing system that the document is no longer considered
  // active for the reason associated with this lifetime enforcer.
  void NotifyInactive();

 private:
  // The associated offscreen document. Must outlive this object.
  raw_ptr<OffscreenDocumentHost> const offscreen_document_;

  // The callback to trigger immediate termination of the offscreen document.
  TerminationCallback termination_callback_;

  // The callback to notify that the document is no longer active. Note that
  // this may be called multiple times if a document goes from active to
  // inactive and back to active before being terminated (which can happen if a
  // document is created for multiple reasons).
  NotifyInactiveCallback notify_inactive_callback_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_DOCUMENT_LIFETIME_ENFORCER_H_
