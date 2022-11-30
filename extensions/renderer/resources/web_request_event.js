// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CHECK = requireNative('logging').CHECK;
var idGeneratorNatives = requireNative('id_generator');
var utils = require('utils');
var webRequestInternal = getInternalApi('webRequestInternal');
const isServiceWorkerContext =
    requireNative('service_worker_natives').IsServiceWorkerContext();

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
  return isServiceWorkerContext ?
      getScopedUniqueSubEventName(eventName) :
      getGloballyUniqueSubEventName(eventName);
}

// WebRequestEventImpl object. This is used for special webRequest events
// with extra parameters. Each invocation of addListener creates a new named
// sub-event. That sub-event is associated with the extra parameters in the
// browser process, so that only it is dispatched when the main event occurs
// matching the extra parameters.
// Note: this is not used for the onActionIgnored event.
//
// Example:
//   chrome.webRequest.onBeforeRequest.addListener(
//       callback, {urls: 'http://*.google.com/*'});
//   ^ callback will only be called for onBeforeRequests matching the filter.
function WebRequestEventImpl(eventName, opt_argSchemas, opt_extraArgSchemas,
                             opt_eventOptions, opt_webViewInstanceId) {
  if (typeof eventName != 'string')
    throw new Error('chrome.WebRequestEvent requires an event name.');

  bindingUtil.addCustomSignature(eventName, opt_extraArgSchemas);

  this.eventName = eventName;
  this.argSchemas = opt_argSchemas;
  this.extraArgSchemas = opt_extraArgSchemas;
  this.webViewInstanceId = opt_webViewInstanceId || 0;
  this.subEvents = [];
}
$Object.setPrototypeOf(WebRequestEventImpl.prototype, null);

// Test if the given callback is registered for this event.
WebRequestEventImpl.prototype.hasListener = function(cb) {
  return this.findListener_(cb) > -1;
};

// Test if any callbacks are registered fur thus event.
WebRequestEventImpl.prototype.hasListeners = function() {
  return this.subEvents.length > 0;
};

// Registers a callback to be called when this event is dispatched. If
// opt_filter is specified, then the callback is only called for events that
// match the given filters. If opt_extraInfo is specified, the given optional
// info is sent to the callback.
WebRequestEventImpl.prototype.addListener =
    function(cb, opt_filter, opt_extraInfo) {
  // NOTE(benjhayden) New APIs should not use this subEventName trick! It does
  // not play well with event pages. See downloads.onDeterminingFilename and
  // ExtensionDownloadsEventRouter for an alternative approach.
  var subEventName = getUniqueSubEventName(this.eventName);
  // Note: this could fail to validate, in which case we would not add the
  // subEvent listener.
  bindingUtil.validateCustomSignature(this.eventName,
                                      $Array.slice(arguments, 1));
  webRequestInternal.addEventListener(
      cb, opt_filter, opt_extraInfo, this.eventName, subEventName,
      this.webViewInstanceId);

  var supportsFilters = false;
  var supportsLazyListeners = true;
  var subEvent =
      bindingUtil.createCustomEvent(subEventName, supportsFilters,
                                    supportsLazyListeners);

  var subEventCallback = cb;
  if (opt_extraInfo && $Array.indexOf(opt_extraInfo, 'blocking') >= 0) {
    var eventName = this.eventName;
    var webViewInstanceId = this.webViewInstanceId;
    subEventCallback = function() {
      var requestId = arguments[0].requestId;
      try {
        var result = $Function.apply(cb, null, arguments);
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, webViewInstanceId, result);
      } catch (e) {
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, webViewInstanceId);
        throw e;
      }
    };
  } else if (
      opt_extraInfo && $Array.indexOf(opt_extraInfo, 'asyncBlocking') >= 0) {
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
  $Array.push(this.subEvents,
      {subEvent: subEvent, callback: cb, subEventCallback: subEventCallback});
  subEvent.addListener(subEventCallback);
};

// Unregisters a callback.
WebRequestEventImpl.prototype.removeListener = function(cb) {
  var idx;
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
  for (var i in this.subEvents) {
    var e = this.subEvents[i];
    if (e.callback === cb) {
      if (e.subEvent.hasListener(e.subEventCallback))
        return i;
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
