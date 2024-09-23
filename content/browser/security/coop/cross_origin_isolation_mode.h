// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_ISOLATION_MODE_H_
#define CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_ISOLATION_MODE_H_

namespace content {

// This enum represents the cross-origin isolation mode as defined in the HTML
// spec:
// https://html.spec.whatwg.org/dev/document-sequences.html#cross-origin-isolation-mode
//
// The cross-origin isolation describes the cross-origin capabilities of an
// execution context.
enum class CrossOriginIsolationMode {
  // The execution context is complying with the security policies required to
  // get access to APIs gated behind crossOriginIsolation. However, some
  // platforms cannot support the process isolation required to grant safe
  // access to those APIs. In this case, the context is in a logical
  // cross-origin isolation mode, where the restrictions of the security
  // policies apply, but it does not get access to crossOriginIsolation-gated
  // APIs.
  kLogical,

  // The execution context is cross-origin isolated and process isolated, and
  // may access APIs gated behind crossOriginIsolation. Note that this access
  // might still be restricted by the PermissionPolicy set by the top-level
  // frame.
  kConcrete
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_ISOLATION_MODE_H_
