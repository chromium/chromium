// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRIVACY_SANDBOX_ATTESTATIONS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_PRIVACY_SANDBOX_ATTESTATIONS_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace content {

// Observes privacy sandbox attestations loading events.
class CONTENT_EXPORT PrivacySandboxAttestationsObserver
    : public base::CheckedObserver {
 public:
  // Called when attestations are loaded.
  virtual void OnAttestationsLoaded() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRIVACY_SANDBOX_ATTESTATIONS_OBSERVER_H_
