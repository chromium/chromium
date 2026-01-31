// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary WebAccessibleResource {
  // Relative paths within the extension package representing web accessible
  // resources.
  required sequence<DOMString> resources;

  // List of <a
  // href="https://developer.chrome.com/docs/extensions/develop/concepts/match-patterns">
  // match patterns</a> to which "resources" are accessible. These patterns should
  // have an effective path of "*". Each match will be checked against the
  // initiating origin.
  sequence<DOMString> matches;

  // List of extension IDs the "resources" are accessible to. A wildcard can
  // be used, denoted by "*".
  sequence<DOMString> extension_ids;

  // If true, the web accessible resources will only be accessible through a
  // dynamic ID. This is an identifier that uniquely identifies the extension
  // and is generated each session. The corresponding dynamic extension URL
  // is available through $(ref:runtime.getURL).
  // Dynamic resources can be loaded regardless of the value. However, if
  // true, resources can only be loaded using the dynamic URL.
  boolean use_dynamic_url;
};

// Stub namespace for the "web_accessible_resources" manifest key.
[generate_error_messages, Namespace=webAccessibleResources]
partial dictionary ExtensionManifest {
  sequence<WebAccessibleResource> web_accessible_resources;
};
