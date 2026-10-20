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
  // Whether this handler is enabled. Defaults to the manifest entry's
  // `enabled` value.
  boolean enabled;

  // Whether this handler may be used for embedded loads (iframe, embed,
  // object). Defaults to the manifest entry's `can_embed` value. Has no
  // effect while `enabled` is false.
  boolean canEmbed;
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

  // Sets the configuration options for a specified MIME type. If the extension
  // runs in split incognito mode, options set from incognito apply only to
  // incognito and last until the incognito session ends. Options not set from
  // incognito follow the values set outside incognito.
  // |mimeType|: The MIME type to configure. It must be a MIME type the
  // extension handles through mime_types_handler.
  // |options|: The options to change.
  // |Returns|: Promise resolved when the configuration has been set.
  static Promise<undefined> setMimeHandlerOptions(DOMString mimeType,
                                                  MimeHandlerOptions options);

  // Reads the options for a MIME type. Each member is the value the extension
  // set, or the manifest entry's value if the extension has not set it.
  // |mimeType|: The MIME type whose options to read. It must be a MIME type
  // the extension handles through mime_types_handler.
  // |PromiseValue|: options
  // |Returns|: Promise resolved with the options for the MIME type.
  static Promise<MimeHandlerOptions> getMimeHandlerOptions(DOMString mimeType);
};

partial interface Browser {
  static attribute MimeHandler mimeHandler;
};
