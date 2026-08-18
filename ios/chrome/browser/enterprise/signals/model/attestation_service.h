// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_ATTESTATION_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_ATTESTATION_SERVICE_H_

#import <map>
#import <string>

#import "base/callback_list.h"
#import "base/functional/callback.h"

// Interface for retrieving device attestation snapshots on iOS.
class AttestationService {
 public:
  using ContentBinding = std::map<std::string, std::string>;
  using InitializeCallback = base::OnceCallback<void(bool)>;
  using SnapshotCallback = base::OnceCallback<void(std::string)>;

  AttestationService() = default;
  virtual ~AttestationService() = default;

  AttestationService(const AttestationService&) = delete;
  AttestationService& operator=(const AttestationService&) = delete;

  // Initializes the attestation service asynchronously.
  // Runs `callback` when initialization is complete with a boolean indicating
  // success or failure. Returns a subscription to manage the callback lifetime.
  virtual base::CallbackListSubscription Initialize(
      InitializeCallback callback) = 0;

  // Returns true if the service is initialized and ready to create snapshots.
  virtual bool IsReady() = 0;

  // Requests an attestation snapshot with `content_binding`. If the service is
  // not ready, it will asynchronously initialize first. Returns a subscription
  // to manage the callback lifetime.
  virtual base::CallbackListSubscription GetSnapshot(
      const ContentBinding& content_binding,
      SnapshotCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_ATTESTATION_SERVICE_H_
