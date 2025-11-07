// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CHECK = requireNative('logging').CHECK;

// Maps a public `webRequest` event name to a private internal name, e.g.
// 'webRequest.onBeforeRequest' to 'webRequestInternal.onBeforeRequest'. This is
// used for the "collapsed listener" strategy for service workers, which uses a
// single aggregated listener in the browser for persistence. Using a distinct
// internal name prevents conflicts with the public API and provides a specific
// target for the browser to dispatch events to wake the worker.
function getInternalEventName(eventName) {
  if (eventName.startsWith('webRequest.')) {
    return 'webRequestInternal.' + eventName.substring('webRequest.'.length);
  }
  return null;
}

function LazyWebRequestEventImpl(
    eventName, opt_argSchemas, opt_extraArgSchemas, opt_eventOptions,
    opt_webViewInstanceId) {
  // TODO(crbug.com/448893426): implement.
  CHECK(false);
}
$Object.setPrototypeOf(LazyWebRequestEventImpl.prototype, null);

exports.$set('getInternalEventName', getInternalEventName);
exports.$set('LazyWebRequestEventImpl', LazyWebRequestEventImpl);
