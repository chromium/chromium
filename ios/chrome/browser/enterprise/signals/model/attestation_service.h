// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_ATTESTATION_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_ATTESTATION_SERVICE_H_

#import <map>
#import <string>

#import "base/functional/callback.h"

// Service interface for device attestation evaluations.
class AttestationService {
 public:
  using InitializeCallback = base::OnceCallback<void(bool success)>;
  using SnapshotCallback =
      base::OnceCallback<void(const std::string& snapshot)>;

  virtual ~AttestationService() = default;

  // Initializes attestation features and fetches any required challenges.
  virtual void Initialize(InitializeCallback callback) = 0;

  // Returns whether the attestation challenge is ready for snapshots.
  virtual bool IsReady() const = 0;

  // Produces a cryptographic snapshot of the attestation challenge, bound to
  // the given `content_binding` key-value pairs. Invokes `callback` with the
  // snapshot data, or an empty string on failure.
  virtual void GetSnapshot(
      const std::map<std::string, std::string>& content_binding,
      SnapshotCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_ATTESTATION_SERVICE_H_

