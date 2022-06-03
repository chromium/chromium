// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#include <set>

#include "base/memory/weak_ptr.h"

class OverlayRequest;
class OverlayResponse;
class OverlayRequestSupport;

// Helper object, intended to be subclassed, that installs callbacks for
// OverlayRequests the first time their UI is presented.
class OverlayRequestCallbackInstaller {
 public:
  OverlayRequestCallbackInstaller();
  virtual ~OverlayRequestCallbackInstaller();

  // Installs callbacks for |request|.  Will only install callbacks once per
  // supported request if called more than once.  |request| must be non-null.
  void InstallCallbacks(OverlayRequest* request);

  // Returns the request support for this installer.  InstallCallbacksInternal()
  // will only be called for supported requests.  By default, all requests are
  // supported.
  virtual const OverlayRequestSupport* GetRequestSupport() const;

 protected:
  // Called from InstallCallbacks() if |request| is supported by
  // GetRequestSupport().  Subclasses should override to supply callbacks for a
  // specific kind of request.  Does nothing by default.
  virtual void InstallCallbacksInternal(OverlayRequest* request);

 private:
  // Called as a completion callback for |request| to remove the completed
  // request from |requests_|.
  void OverlayCompleted(OverlayRequest* request, OverlayResponse* response);

  // Set containing the requests for which callbacks have already been
  // installed.  Requests are removed from the set when their completion
  // callbacks are executed.
  std::set<OverlayRequest*> requests_;

  base::WeakPtrFactory<OverlayRequestCallbackInstaller> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
