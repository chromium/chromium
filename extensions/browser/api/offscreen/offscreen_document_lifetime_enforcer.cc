// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_document_lifetime_enforcer.h"

namespace extensions {

OffscreenDocumentLifetimeEnforcer::OffscreenDocumentLifetimeEnforcer(
    OffscreenDocumentHost* offscreen_document,
    TerminationCallback termination_callback,
    NotifyInactiveCallback notify_inactive_callback)
    : offscreen_document_(offscreen_document),
      termination_callback_(std::move(termination_callback)),
      notify_inactive_callback_(std::move(notify_inactive_callback)) {}

OffscreenDocumentLifetimeEnforcer::~OffscreenDocumentLifetimeEnforcer() =
    default;

void OffscreenDocumentLifetimeEnforcer::TerminateDocument() {
  DCHECK(termination_callback_);
  std::move(termination_callback_).Run();
}

void OffscreenDocumentLifetimeEnforcer::NotifyInactive() {
  notify_inactive_callback_.Run();
}

}  // namespace extensions
