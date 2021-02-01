// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
#define CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_

namespace content {

// This enum specifies flag values for the types of JavaScript bindings exposed
// to renderers.
enum BindingsPolicy {
  BINDINGS_POLICY_NONE = 0,
  // HTML-based UI bindings that allows the JS content to send JSON-encoded
  // data back to the browser process.
  // These bindings should not be exposed to normal web content.
  BINDINGS_POLICY_WEB_UI = 1 << 0,
  // HTML-based UI bindings that allows access to Mojo system API. The Mojo
  // system API provides the ability to create Mojo primitives such as message
  // and data pipes, as well as connecting to named services exposed by the
  // browser.
  // These bindings should not be exposed to normal web content.
  BINDINGS_POLICY_MOJO_WEB_UI = 1 << 1,
  // DOM automation bindings that allows the JS content to send JSON-encoded
  // data back to automation in the parent process.  (By default this isn't
  // allowed unless the app has been started up with the --dom-automation
  // switch.)
  BINDINGS_POLICY_DOM_AUTOMATION = 1 << 2,
  // Bindings that allows the JS content to retrieve a variety of internal
  // metrics. (By default this isn't allowed unless the app has been started up
  // with the --enable-stats-collection-bindings switch.)
  BINDINGS_POLICY_STATS_COLLECTION = 1 << 3,
};

constexpr int kWebUIBindingsPolicyMask =
    BINDINGS_POLICY_WEB_UI | BINDINGS_POLICY_MOJO_WEB_UI;
}

#endif  // CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
