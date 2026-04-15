// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Per-MIME-type configuration for a MIME handler extension.
dictionary MimeHandlerMimeTypeConfig {
  // Relative path to the handler page within the extension.
  required DOMString handler_url;
  // Whether the handler supports being embedded in iframe/embed/object
  // elements. Defaults to false when absent.
  boolean can_embed;
};

// `mime_types_handler` manifest key definition (dict format).
// The key maps MIME type strings to per-type handler configuration.
// WebIDL has no record<K,V> type; the outer map is declared as `object`
// and the parser iterates base::DictValue, calling
// MimeHandlerMimeTypeConfig::FromValue() per entry — the same pattern
// used by file_handlers.webidl for its `accept` field.
[generate_error_messages, Namespace=mimeHandlers]
partial dictionary ExtensionManifest {
  object mime_types_handler;
};
