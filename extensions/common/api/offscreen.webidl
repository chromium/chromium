// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum Reason {
  // A reason used for testing purposes only.
  "TESTING",
  // Specifies that the offscreen document is responsible for playing audio.
  "AUDIO_PLAYBACK",
  // Specifies that the offscreen document needs to embed and script an
  // iframe in order to modify the iframe's content.
  "IFRAME_SCRIPTING",
  // Specifies that the offscreen document needs to embed an iframe and
  // scrape its DOM to extract information.
  "DOM_SCRAPING",
  // Specifies that the offscreen document needs to interact with Blob
  // objects (including <code>URL.createObjectURL()</code>).
  "BLOBS",
  // Specifies that the offscreen document needs to use the
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/DOMParser">DOMParser
  // API</a>.
  "DOM_PARSER",
  // Specifies that the offscreen document needs to interact with
  // media streams from user media (e.g. <code>getUserMedia()</code>).
  "USER_MEDIA",
  // Specifies that the offscreen document needs to interact with
  // media streams from display media (e.g. <code>getDisplayMedia()</code>).
  "DISPLAY_MEDIA",
  // Specifies that the offscreen document needs to use
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API">WebRTC
  // APIs</a>.
  "WEB_RTC",
  // Specifies that the offscreen document needs to interact with the
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/Clipboard_API">Clipboard
  // API</a>.
  "CLIPBOARD",
  // Specifies that the offscreen document needs access to
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/Window/localStorage">localStorage</a>.
  "LOCAL_STORAGE",
  // Specifies that the offscreen document needs to spawn workers.
  "WORKERS",
  // Specifies that the offscreen document needs to use
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/Battery_Status_API">navigator.getBattery</a>.
  "BATTERY_STATUS",
  // Specifies that the offscreen document needs to use
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/Window/matchMedia">window.matchMedia</a>.
  "MATCH_MEDIA",
  // Specifies that the offscreen document needs to use
  // <a
  // href="https://developer.mozilla.org/en-US/docs/Web/API/Navigator/geolocation">navigator.geolocation</a>.
  "GEOLOCATION"
};

dictionary CreateParameters {
  // The reason(s) the extension is creating the offscreen document.
  required sequence<Reason> reasons;
  // The (relative) URL to load in the document.
  required DOMString url;
  // A developer-provided string that explains, in more detail, the need for
  // the background context. The user agent _may_ use this in display to the
  // user.
  required DOMString justification;
};

// Use the <code>offscreen</code> API to create and manage offscreen documents.
interface Offscreen {
  // Creates a new offscreen document for the extension.
  // |parameters|: The parameters describing the offscreen document to create.
  // |Returns|: Promise that resolves when the offscreen document is created
  // and has completed its initial page load.
  static Promise<undefined> createDocument(CreateParameters parameters);

  // Closes the currently-open offscreen document for the extension.
  // |Returns|: Promise that resolves when the offscreen document has been
  // closed.
  static Promise<undefined> closeDocument();

  // Determines whether the extension has an active document.
  // TODO(crbug.com/40849649): This probably isn't something we want to
  // ship in its current form (hence the nodoc). Instead of this, we should
  // integrate offscreen documents into a service worker-compatible getViews()
  // alternative. But this is pretty useful in testing environments.
  // |Returns|: Promise that resolves with the result of whether the
  // extension has an active offscreen document.
  // |PromiseValue|: result
  [ nodoc, requiredCallback ] static Promise<boolean> hasDocument();
};

partial interface Browser {
  static attribute Offscreen offscreen;
};
