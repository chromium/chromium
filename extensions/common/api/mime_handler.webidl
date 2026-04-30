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

// Use the <code>chrome.mimeHandler</code> API to handle MIME type streams
// in third-party extensions.
interface MimeHandler {
  // Retrieves stream information for the current MIME handler context.
  // Must be called from within a MIME handler extension page.
  // |PromiseValue|: info
  [requiredCallback] static Promise<StreamInfo> getStreamInfo();
};

partial interface Browser {
  static attribute MimeHandler mimeHandler;
};
