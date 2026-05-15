// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary StreamInfo {
  // The MIME type of the intercepted content.
  required DOMString mimeType;

  // The original URL the user navigated to.
  required DOMString originalUrl;

  // The URL to fetch the stream data from.
  required DOMString streamUrl;

  // The tab ID containing the document.
  required long tabId;

  // HTTP response headers as key-value pairs.
  required object responseHeaders;

  // True if loaded in an embedded context (iframe/embed/object).
  required boolean embedded;
};

// Persisted options for a MIME handler registered for one MIME type.
dictionary MimeHandlerOptions {
  // Whether this handler is active for the given MIME type.
  required boolean enabled;
};

// Use the <code>chrome.mimeHandler</code> API to handle MIME type streams
// in third-party extensions.
interface MimeHandler {
  // Retrieves stream information for the current MIME handler context.
  // Must be called from within a MIME handler extension page.
  // |PromiseValue|: info
  static Promise<StreamInfo> getStreamInfo();

  // Aborts current stream handling and hands the content off to the
  // user agent's native handler. After this call the extension frame
  // will be torn down; callers should not expect further execution.
  static Promise<undefined> abortAndFallbackToNativeHandler();

  // Sets the configuration options for a specified MIME type.
  // |mimeType|: The MIME type to configure.
  // |options|: The new options to use.
  // |Returns|: Promise resolved when the configuration has been set.
  static Promise<undefined> setMimeHandlerOptions(DOMString mimeType,
                                                  MimeHandlerOptions options);

  // Reads the persisted options for a MIME type. Returns defaults
  // (enabled=true) if none have been stored.
  // |mimeType|: The MIME type whose options to read.
  // |PromiseValue|: options
  // |Returns|: Promise resolved with the persisted options for the MIME type.
  static Promise<MimeHandlerOptions> getMimeHandlerOptions(DOMString mimeType);
};

partial interface Browser {
  static attribute MimeHandler mimeHandler;
};
