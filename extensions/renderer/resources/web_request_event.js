// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CHECK = requireNative('logging').CHECK;
var idGeneratorNatives = requireNative('id_generator');
var utils = require('utils');
var webRequestInternal = getInternalApi('webRequestInternal');
var webRequestNatives = requireNative('web_request_natives');
const allowAsyncResponsesForAllEvents =
    webRequestNatives.AllowAsyncResponsesForAllEvents();
const isServiceWorkerContext =
    requireNative('service_worker_natives').IsServiceWorkerContext();
const usePerContextEventDispatch =
    webRequestNatives.IsPerContextEventDispatchEnabled();

// Returns an ID that is either globally unique (in this process) or unique
// within this given context. Note that we use separate prefixes ('g' and 's')
// to ensure there are no collisions between these two groups.
function getGloballyUniqueSubEventName(eventName) {
  return eventName + '/g' + idGeneratorNatives.GetNextId();
}
function getScopedUniqueSubEventName(eventName) {
  return eventName + '/s' + idGeneratorNatives.GetNextScopedId();
}

// A sub-event-name uses a suffix with an additional identifier. For service
// worker contexts, we use a context-specific identifier; this allows multiple
// runs of the service worker script to produce subevents with the same IDs.
// For non-service worker contexts, we need to use a global identifier. This is
// because there may be multiple contexts, each with listeners (such as multiple
// webviews [https://crbug.com/1309302] or multiple frames
// [https://crbug.com/1297276]) that run in the same process. This would result
// in collisions between the event listener IDs in the webRequest API. This
// isn't an issue with service worker contexts because, even though they run in
// the same process, they have additional identifiers of the service worker
// thread and version.
function getUniqueSubEventName(eventName) {
  return isServiceWorkerContext ? getScopedUniqueSubEventName(eventName) :
                                  getGloballyUniqueSubEventName(eventName);
}

function hasExtraInfo(extraInfo, option) {
  return !!extraInfo && $Array.indexOf(extraInfo, option) >= 0;
}

// ----------------------------------------------------------------------------
// Per-context dispatch.
//
// When WebRequestPerContextEventDispatch is enabled, the browser dispatches
// matching events once per context, and JS matches context listeners locally.
//
// For events the browser awaits, this reports each response through
// "webRequestInternal.eventHandled" and signals completion when every
// listener was notified and the block count is zero; see
// `maybeReportEventHandlingDone()`.
// ----------------------------------------------------------------------------

// Listener IDs for the `ParsedFilter` cache (see `WebRequestNatives`).
let nextListenerId = 0;

// Listener records for all events in this context, keyed by the listener ID
// registered with the `TrackListener()` native.
const trackedListeners = {
  __proto__: null
};

// One custom event per parent event name; see `getOrCreateParentEvent()`.
const parentEvents = {
  __proto__: null
};

// Header names delivered only to "extraHeaders" listeners.
// NOTE: Keep in sync with `kExtra{Request,Response}HeaderNames` in
// //extensions/common/api/web_request/web_request_constants.h.
const kExtraRequestHeaderNames =
    ['accept-encoding', 'accept-language', 'cookie', 'origin', 'referer'];
const kExtraResponseHeaderNames = ['set-cookie'];

// Returns a copy of `headers` ({name, value} pairs) excluding any entries
// whose name is in `hiddenNames` (case-insensitive).
function eraseHeaders(headers, hiddenNames) {
  const result = [];
  for (let i = 0; i < headers.length; ++i) {
    if ($Array.indexOf(hiddenNames, $String.toLowerCase(headers[i].name)) < 0) {
      $Array.push(result, headers[i]);
    }
  }
  return result;
}

// Removes rawDER from the first certificate in `details.securityInfo`.
// The browser sets rawDER only on the leaf certificate (see `SetSecurityInfo`).
// NOTE: Keep in sync with `FilterSecurityInfo()` in
// //extensions/browser/api/web_request/web_request_event_details.cc.
function filterSecurityInfo(details) {
  const securityInfo = details.securityInfo;
  if (!securityInfo || !securityInfo.certificates ||
      !securityInfo.certificates[0]) {
    return;
  }
  delete securityInfo.certificates[0].rawDER;
}

// Returns optional `details` properties that `extraInfoSpec` doesn't request.
// NOTE: Keep in sync with `WebRequestEventDetails::GetFilteredDict()`.
function computeDroppedKeys(extraInfoSpec) {
  const droppedKeys = [];
  if (!hasExtraInfo(extraInfoSpec, 'requestHeaders')) {
    $Array.push(droppedKeys, 'requestHeaders');
  }
  if (!hasExtraInfo(extraInfoSpec, 'responseHeaders')) {
    $Array.push(droppedKeys, 'responseHeaders');
  }
  if (!hasExtraInfo(extraInfoSpec, 'requestBody')) {
    $Array.push(droppedKeys, 'requestBody');
  }
  if (!hasExtraInfo(extraInfoSpec, 'securityInfo') &&
      !hasExtraInfo(extraInfoSpec, 'securityInfoRawDer')) {
    $Array.push(droppedKeys, 'securityInfo');
  }
  return droppedKeys;
}

// Returns a deep copy of `details` filtered down to `listener`'s
// extraInfoSpec. Dispatched details contain the union of options across all
// listeners in the context, so each listener must filter out fields requested
// by others.
function getFilteredDetails(details, listener) {
  // TODO(crbug.com/494684626): Avoid deep-copying fields that are immediately
  // filtered out below (e.g. raw certificates in `securityInfoRawDer`).
  const copy = utils.deepCopy(details);
  for (let i = 0; i < listener.droppedKeys.length; ++i) {
    delete copy[listener.droppedKeys[i]];
  }
  if (!listener.hasExtraHeaders) {
    if (copy.requestHeaders) {
      copy.requestHeaders =
          eraseHeaders(copy.requestHeaders, kExtraRequestHeaderNames);
    }
    if (copy.responseHeaders) {
      copy.responseHeaders =
          eraseHeaders(copy.responseHeaders, kExtraResponseHeaderNames);
    }
  }
  if (!listener.hasSecurityInfoRawDer) {
    filterSecurityInfo(copy);
  }
  return copy;
}

// Reports a listener exception without aborting dispatch. Uses the bindings'
// exception handler to report each listener error separately, matching
// `EventEmitter::DispatchSync()`.
function reportError(e) {
  // TODO(crbug.com/494684626): Add a test that multiple listener errors are
  // reported correctly.
  // NOTE: Keep in sync with `EventEmitter::DispatchSync()` in
  // //extensions/renderer/bindings/event_emitter.cc.
  const kEventHandlerErrorMessage = 'Error in event handler';
  bindingUtil.handleException(kEventHandlerErrorMessage, e);
}

// Sends the completion signal once every listener was notified and no
// response is pending.
function maybeReportEventHandlingDone(dispatch) {
  if (dispatch.allListenersNotified && dispatch.blockCount === 0) {
    webRequestNatives.ReportEventHandlingDone(
        dispatch.eventName, dispatch.requestId, dispatch.instanceId);
  }
}

// Decrements the block count, reporting `response` if the listener produced
// one; may send the completion signal.
function decrementBlockCount(dispatch, response, extraInfoSpec) {
  try {
    if (response !== undefined && response !== null) {
      // TODO(crbug.com/494684626): Drop the subEventName parameter with the
      // legacy per-listener path. Until then, pass `eventName` for both.
      webRequestInternal.eventHandled(
          dispatch.eventName, dispatch.eventName, dispatch.requestId,
          dispatch.instanceId, response, extraInfoSpec || []);
    }
  } finally {
    // Decrement the block count even if `eventHandled()` throws (e.g., on an
    // invalid response) so the request does not hang.
    dispatch.blockCount--;
    maybeReportEventHandlingDone(dispatch);
  }
}

// Reports `response` and decrements the block count. Does nothing if the
// listener already responded.
function onEventHandled(listener, dispatch, response) {
  const idx = $Array.indexOf(listener.blockedDispatches, dispatch);
  if (idx < 0) {
    return;
  }
  $Array.splice(listener.blockedDispatches, idx, 1);
  decrementBlockCount(dispatch, response, listener.extraInfoSpec);
}

// Runs a blocking listener, which responds through its return value (either a
// response object or, if allowed, a promise).
function runBlockingListener(dispatch, listener, filteredDetails) {
  dispatch.blockCount++;
  $Array.push(listener.blockedDispatches, dispatch);
  try {
    const result = $Function.apply(listener.callback, null, [filteredDetails]);
    if (allowAsyncResponsesForAllEvents && result instanceof $Promise.self) {
      // The dispatch stays tracked until the promise settles or the listener
      // is removed. Promise rejections unblock the dispatch without a
      // response; rethrowing logs the error to the console.
      $Promise.catch($Promise.then(result, function(asyncResult) {
        onEventHandled(listener, dispatch, asyncResult);
      }), function(e) {
        onEventHandled(listener, dispatch, undefined);
        throw e;
      });
    } else {
      // Synchronous return value.
      onEventHandled(listener, dispatch, result);
    }
  } catch (e) {
    // Report the error and unblock the dispatch with no response.
    reportError(e);
    onEventHandled(listener, dispatch, undefined);
  }
}

// Runs an async blocking listener, which responds through a callback rather
// than a return value.
function runAsyncBlockingListener(dispatch, listener, filteredDetails) {
  // TODO(crbug.com/494684626): Add a test for a listener that returns a
  // promise and removes itself inside its callback.
  dispatch.blockCount++;
  $Array.push(listener.blockedDispatches, dispatch);
  const handledCallback = function(response) {
    onEventHandled(listener, dispatch, response);
  };
  try {
    $Function.apply(
        listener.callback, null, [filteredDetails, handledCallback]);
  } catch (e) {
    reportError(e);
    onEventHandled(listener, dispatch, undefined);
  }
}

// Returns the handler for the (parent) `eventName` events that the browser
// sends to this context.
function createEventDispatchHandler(eventName) {
  return function(args) {
    // This handler replaces the default dispatch to the parent event's own
    // listeners, which are placeholders that never run; the real callbacks
    // come from `trackedListeners`.
    const details = args[0];
    const payload = args[1];

    // Per-dispatch state, shared with the helpers above.
    const dispatch = {
      __proto__: null,
      eventName: eventName,
      requestId: details.requestId,
      instanceId: payload.instanceId,
      blockCount: 0,
      allListenersNotified: false,
    };

    const matchingIds = webRequestNatives.GetMatchingListeners(
        eventName, details.url, details.type, details.tabId, payload.windowId,
        payload.instanceId, payload.awaitResponse);

    for (let i = 0; i < matchingIds.length; ++i) {
      let listener = trackedListeners[matchingIds[i]];
      if (!listener) {
        continue;
      }
      let filteredDetails = getFilteredDetails(details, listener);

      if (listener.isBlocking) {
        // Blocking listener (return value).
        runBlockingListener(dispatch, listener, filteredDetails);
      } else if (listener.isAsyncBlocking) {
        // Async blocking listener (callback).
        runAsyncBlockingListener(dispatch, listener, filteredDetails);
      } else {
        // Non-blocking listener.
        try {
          $Function.apply(listener.callback, null, [filteredDetails]);
        } catch (e) {
          reportError(e);
        }
      }
    }

    // Every listener was notified; the completion signal now waits only for
    // the pending responses (and goes out at once if there are none).
    if (payload.awaitResponse) {
      dispatch.allListenersNotified = true;
      maybeReportEventHandlingDone(dispatch);
    }
  };
}

// Returns the custom event that carries `eventName`'s listener
// registrations to the browser.
function getOrCreateParentEvent(eventName) {
  let parentEvent = parentEvents[eventName];
  if (parentEvent) {
    return parentEvent;
  }
  parentEvent = bindingUtil.createCustomEvent(
      eventName, /*supportsFilters=*/ true,
      /*supportsLazyListeners=*/ true);
  parentEvents[eventName] = parentEvent;
  bindingUtil.registerEventDispatchHandler(
      eventName, createEventDispatchHandler(eventName));
  return parentEvent;
}

// ----------------------------------------------------------------------------

// WebRequestEventImpl object. This is used for special webRequest events
// with extra parameters.
//
// With per-context dispatch (usePerContextEventDispatch), listeners register
// with the browser under the parent event name, and renderer bindings match
// and dispatch to listeners locally.
//
// Otherwise, each invocation of addListener creates a new named sub-event.
// That sub-event is associated with the extra parameters in the browser
// process, so that only it is dispatched when the main event occurs matching
// the extra parameters.
//
// Note: this is not used for the onActionIgnored event.
//
// Example:
//   chrome.webRequest.onBeforeRequest.addListener(
//       callback, {urls: 'http://*.google.com/*'});
//   ^ callback will only be called for onBeforeRequests matching the filter.
function WebRequestEventImpl(eventName, opt_argSchemas, opt_extraArgSchemas,
                             opt_eventOptions, opt_webViewInstanceId) {
  if (typeof eventName != 'string') {
    throw new Error('chrome.WebRequestEvent requires an event name.');
  }

  bindingUtil.addCustomSignature(eventName, opt_extraArgSchemas);

  this.eventName = eventName;
  this.argSchemas = opt_argSchemas;
  this.extraArgSchemas = opt_extraArgSchemas;
  this.webViewInstanceId = opt_webViewInstanceId || 0;
  this.subEvents = [];  // Legacy sub-event dispatch.
  this.listeners = [];  // Per-context dispatch.
}
$Object.setPrototypeOf(WebRequestEventImpl.prototype, null);

// Test if the given callback is registered for this event.
WebRequestEventImpl.prototype.hasListener = function(cb) {
  return this.findListener_(cb) > -1;
};

// Test if any callbacks are registered fur thus event.
WebRequestEventImpl.prototype.hasListeners = function() {
  if (usePerContextEventDispatch) {
    return this.listeners.length > 0;
  }
  return this.subEvents.length > 0;
};

// Registers a callback to be called when this event is dispatched. If
// opt_filter is specified, then the callback is only called for events that
// match the given filters. If opt_extraInfo is specified, the given optional
// info is sent to the callback.
WebRequestEventImpl.prototype.addListener = function(
    cb, opt_filter, opt_extraInfo) {
  if (usePerContextEventDispatch) {
    this.addListenerContextDispatch_(cb, opt_filter, opt_extraInfo);
    return;
  }

  // NOTE(benjhayden) New APIs should not use this subEventName trick! It does
  // not play well with event pages. See downloads.onDeterminingFilename and
  // ExtensionDownloadsEventRouter for an alternative approach.
  var subEventName = getUniqueSubEventName(this.eventName);
  // Note: this could fail to validate, in which case we would not add the
  // subEvent listener.
  bindingUtil.validateCustomSignature(this.eventName,
                                      $Array.slice(arguments, 1));

  var supportsFilters = true;
  var supportsLazyListeners = true;
  var subEvent = bindingUtil.createCustomEvent(
      subEventName, supportsFilters, supportsLazyListeners);

  var subEventCallback = cb;
  if (hasExtraInfo(opt_extraInfo, 'blocking')) {
    var eventName = this.eventName;
    var webViewInstanceId = this.webViewInstanceId;
    subEventCallback = function() {
      var requestId = arguments[0].requestId;

      function sendEventHandledWithResult(result) {
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, webViewInstanceId, result);
      }
      function handleHandlerError(e) {
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, webViewInstanceId);
        throw e;
      }

      try {
        let result = $Function.apply(cb, null, arguments);
        if (allowAsyncResponsesForAllEvents &&
            result instanceof $Promise.self) {
          $Promise.catch(
              $Promise.then(result, (asyncResult) => {
                sendEventHandledWithResult(asyncResult);
              }),
              (e) => {
                handleHandlerError(e);
              });
        } else {
          sendEventHandledWithResult(result);
        }
      } catch (e) {
        handleHandlerError(e);
      }
    };
  } else if (hasExtraInfo(opt_extraInfo, 'asyncBlocking')) {
    var eventName = this.eventName;
    var webViewInstanceId = this.webViewInstanceId;
    subEventCallback = function() {
      var details = arguments[0];
      var requestId = details.requestId;
      var handledCallback = function(response) {
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, webViewInstanceId, response);
      };
      $Function.apply(cb, null, [details, handledCallback]);
    };
  }
  $Array.push(
      this.subEvents,
      {subEvent: subEvent, callback: cb, subEventCallback: subEventCallback});

  subEvent.addListener(
      subEventCallback, opt_filter,
      {extraInfo: opt_extraInfo, webViewInstanceId: this.webViewInstanceId});
};

// addListener() for per-context dispatch: registers the listener with the
// browser under the parent event and records it locally for dispatch matching.
WebRequestEventImpl.prototype.addListenerContextDispatch_ = function(
    cb, opt_filter, opt_extraInfo) {
  bindingUtil.validateCustomSignature(
      this.eventName, $Array.slice(arguments, 1));

  const parentEvent = getOrCreateParentEvent(this.eventName);
  const listener = {
    __proto__: null,
    id: nextListenerId++,
    callback: cb,
    // Attaches to the shared parent event to forward filters and options to
    // the browser once per addListener call. A new function keeps each call
    // a separate registration.
    placeholder: function() {},
    extraInfoSpec: opt_extraInfo,
    isBlocking: hasExtraInfo(opt_extraInfo, 'blocking'),
    isAsyncBlocking: hasExtraInfo(opt_extraInfo, 'asyncBlocking'),
    // Precalculated to avoid work during event dispatch.
    droppedKeys: computeDroppedKeys(opt_extraInfo),
    hasExtraHeaders: hasExtraInfo(opt_extraInfo, 'extraHeaders'),
    hasSecurityInfoRawDer: hasExtraInfo(opt_extraInfo, 'securityInfoRawDer'),
    // Dispatches that still await this listener's asynchronous response.
    blockedDispatches: [],
  };

  // NOTE: Throws if validation fails, preventing native listener tracking
  // below.
  parentEvent.addListener(listener.placeholder, opt_filter, {
    extraInfo: opt_extraInfo,
    webViewInstanceId: this.webViewInstanceId,
  });

  // Registers listener filter rules in the C++ cache to avoid re-parsing
  // filters on dispatch.
  webRequestNatives.TrackListener(
      this.eventName, listener.id, opt_filter, this.webViewInstanceId,
      listener.isBlocking, listener.isAsyncBlocking);
  trackedListeners[listener.id] = listener;
  $Array.push(this.listeners, listener);
};

// Unregisters a callback.
WebRequestEventImpl.prototype.removeListener = function(cb) {
  var idx;
  if (usePerContextEventDispatch) {
    const parentEvent = parentEvents[this.eventName];
    while ((idx = this.findListener_(cb)) >= 0) {
      const listener = this.listeners[idx];
      parentEvent.removeListener(listener.placeholder);
      webRequestNatives.UntrackListener(listener.id);
      $Array.splice(this.listeners, idx, 1);
      delete trackedListeners[listener.id];
    }
    return;
  }

  while ((idx = this.findListener_(cb)) >= 0) {
    var e = this.subEvents[idx];
    e.subEvent.removeListener(e.subEventCallback);
    if (e.subEvent.hasListeners()) {
      console.error(
          'Internal error: webRequest subEvent has orphaned listeners.');
    }
    $Array.splice(this.subEvents, idx, 1);
  }
};

WebRequestEventImpl.prototype.findListener_ = function(cb) {
  if (usePerContextEventDispatch) {
    for (let i = 0; i < this.listeners.length; ++i) {
      if (this.listeners[i].callback === cb) {
        return i;
      }
    }
    return -1;
  }

  for (var i in this.subEvents) {
    var e = this.subEvents[i];
    if (e.callback === cb) {
      if (e.subEvent.hasListener(e.subEventCallback)) {
        return i;
      }
      console.error('Internal error: webRequest subEvent has no callback.');
    }
  }

  return -1;
};

WebRequestEventImpl.prototype.addRules = function(rules, opt_cb) {
  throw new Error('This event does not support rules.');
};

WebRequestEventImpl.prototype.removeRules =
    function(ruleIdentifiers, opt_cb) {
  throw new Error('This event does not support rules.');
};

WebRequestEventImpl.prototype.getRules = function(ruleIdentifiers, cb) {
  throw new Error('This event does not support rules.');
};

function WebRequestEvent() {
  privates(WebRequestEvent).constructPrivate(this, arguments);
}

// Our util code requires we construct a new WebRequestEvent via a call to
// 'new WebRequestEvent', which wouldn't work well with calling a v8::Function.
// Provide a wrapper for native bindings to call into.
function createWebRequestEvent(eventName, opt_argSchemas, opt_extraArgSchemas,
                               opt_eventOptions, opt_webViewInstanceId) {
  return new WebRequestEvent(eventName, opt_argSchemas, opt_extraArgSchemas,
                             opt_eventOptions, opt_webViewInstanceId);
}

utils.expose(WebRequestEvent, WebRequestEventImpl, {
  functions: [
    'hasListener',
    'hasListeners',
    'addListener',
    'removeListener',
    'addRules',
    'removeRules',
    'getRules',
  ],
});

exports.$set('WebRequestEvent', WebRequestEvent);
exports.$set('createWebRequestEvent', createWebRequestEvent);
