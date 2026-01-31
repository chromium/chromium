// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A ProtocolHandler registers the register a Custom Hanlder for an unknown
// URL scheme.
// https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/protocol_handlers
dictionary ProtocolHandler {
  // A string definition of the protocol to handle.
  required DOMString protocol;
  // A string representation of the protocol handlers, displayed to the user
  // when prompting for permissions.
  required DOMString name;
  // A string representing the URL of the protocol handler (must be a
  // localizable property).
  required DOMString uriTemplate;
};

// `protocol_handlers` manifest key defintion. Protocol Handlers allow
// developers to let extensions register Custom Handlers for URL's schemes
// unknown to the Browser. This manifest key provides a similar behavior than
// the Web API implementing the Custom Handlers section of the HTML
// specification.
// https://html.spec.whatwg.org/#custom-handlers
[generate_error_messages, Namespace=protocolHandlers]
partial dictionary ExtensionManifest {
  sequence<ProtocolHandler> protocol_handlers;
};
