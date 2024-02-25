// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#include <set>

#import "base/memory/raw_ptr.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#include "testing/gmock/include/gmock/gmock.h"

class OverlayResponseSupport;

// Interface for a test object whose interface is called by callbacks added
// by FakeOverlayRequestCallbackInstaller.
class FakeOverlayRequestCallbackReceiver {
 public:
  // Function used as the completion callback for `request`.  `response` is
  // `request`'s completion response.  Executes CompletionCallback() with
  // `request`.
  void RunCompletionCallback(OverlayRequest* request,
                             OverlayResponse* response);
  // Function used as the callback when `response` is dispatched through
  // `request`.  Only executed if `response` is supported by `response_support`.
  // Executes DispatchCallback() with `request` and `response_support`.
  void RunDispatchCallback(OverlayRequest* request,
                           const OverlayResponseSupport* response_support,
                           OverlayResponse* response);

  // Function called by RunCompletionCallback().  Can be overridden to verify
  // the execution of the completion callback when the completion response is
  // uninteresting.
  virtual void CompletionCallback(OverlayRequest* request) = 0;
  // Function called by RunDispatchCallback().  Can be overridden to verify the
  // successful dispatch of stateless responses.
  virtual void DispatchCallback(
      OverlayRequest* request,
      const OverlayResponseSupport* response_support) = 0;
};

// Mock version of the fake callback receiver receiver.
// MockOverlayRequestCallbackReceiver can be used to verify the execution of
// installed callbacks.
class MockOverlayRequestCallbackReceiver
    : public FakeOverlayRequestCallbackReceiver {
 public:
  MockOverlayRequestCallbackReceiver();
  ~MockOverlayRequestCallbackReceiver();

  MOCK_METHOD1(CompletionCallback, void(OverlayRequest* request));
  MOCK_METHOD2(DispatchCallback,
               void(OverlayRequest* request,
                    const OverlayResponseSupport* response_support));
};

// OverlayRequestCallbackInstaller subclass used for testing.  Sets up callbacks
// that execute on a receiver object.
class FakeOverlayRequestCallbackInstaller
    : public OverlayRequestCallbackInstaller {
 public:
  // Constructor for a fake callback installer that creates callbacks that are
  // forwarded to `receiver`.  `receiver` must be non-null, and must outlive any
  // requests passed to InstallCallback().  `dispatch_supports` is a list of
  // OverlayResponseSupports for each dispatch response InfoType being tested
  // by this fake installer.  Every supported OverlayResponse dispatched through
  // a request with callbacks installed by this instance will trigger
  // DispatchCallback() on `receiver` with the response support for that
  // response.
  FakeOverlayRequestCallbackInstaller(
      FakeOverlayRequestCallbackReceiver* receiver,
      const std::set<const OverlayResponseSupport*>& dispatch_supports);

  ~FakeOverlayRequestCallbackInstaller() override;

  // Sets the request support for the callback installer.
  void set_request_support(const OverlayRequestSupport* request_support) {
    request_support_ = request_support ?: OverlayRequestSupport::All();
  }

 private:
  // OverlayRequestCallbackInstaller:
  const OverlayRequestSupport* GetRequestSupport() const override;
  void InstallCallbacksInternal(OverlayRequest* request) override;

  raw_ptr<FakeOverlayRequestCallbackReceiver> receiver_ = nullptr;
  raw_ptr<const OverlayRequestSupport> request_support_ =
      OverlayRequestSupport::All();
  const std::set<const OverlayResponseSupport*> dispatch_supports_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
