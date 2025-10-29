// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CHECK = requireNative('logging').CHECK;

// Maps a public `webRequest` event name to a private internal name, e.g.
// 'webRequest.onBeforeRequest' to 'webRequestInternal.onBeforeRequest'. This is
// used for the "collapsed listener" strategy for service workers, which uses a
// single aggregated listener in the browser for persistence. Using a distinct
// internal name prevents conflicts with the public API and provides a specific
// target for the browser to dispatch events to wake the worker.
function getInternalEventName(eventName) {
  if ($String.indexOf(eventName, 'webRequest.') === 0) {
    return 'webRequestInternal.' +
        $String.slice(eventName, 'webRequest.'.length);
  }
  return null;
}

function LazyWebRequestEventImpl(
    eventName, opt_argSchemas, opt_extraArgSchemas, opt_eventOptions,
    opt_webViewInstanceId) {
  if (typeof eventName != 'string') {
    throw new Error('chrome.WebRequestEvent requires an event name.');
  }

  this.eventName = eventName;
  this.argSchemas = opt_argSchemas;
  this.extraArgSchemas = opt_extraArgSchemas;

  // NOTE: kept for consistency with `WebRequestEventImpl`.
  // This class is not used for webviews at this stage.
  this.webViewInstanceId = opt_webViewInstanceId || 0;
  CHECK(this.webViewInstanceId === 0);

  this.internalEventName = getInternalEventName(eventName);
  CHECK(this.internalEventName);

  this.internalEvent = bindingUtil.createCustomEvent(
      this.internalEventName, /*supports_filter=*/ true,
      /*supports_lazy_listeners=*/ true);

  // Store the individual listeners: { callback, filter, extraInfo, matcher }.
  this.listeners = [];
  // Track aggregated filters.
  this.filters = $Object.create(null);

  // Single callback registered on the internal event, responsible for fan-out.
  this.dispatchToListeners = $Function.bind(this.dispatchToListeners_, this);
}
$Object.setPrototypeOf(LazyWebRequestEventImpl.prototype, null);

// Checks if the given callback is registered for this event.
LazyWebRequestEventImpl.prototype.hasListener = function(cb) {
  for (let i = 0; i < this.listeners.length; i++) {
    if (this.listeners[i].callback === cb) {
      return true;
    }
  }
  return false;
};

// Checks if any callbacks are registered for this event.
LazyWebRequestEventImpl.prototype.hasListeners = function() {
  return this.listeners.length > 0;
};

// Registers a callback to be called when this event is dispatched. If
// `opt_filter` is specified, then the callback is only called for events that
// match the given filters. If `opt_extraInfo` is specified, the given optional
// info will be sent to the callback during dispatch.
LazyWebRequestEventImpl.prototype.addListener = function(
    cb, opt_filter, opt_extraInfo) {
  bindingUtil.validateCustomSignature(
      this.eventName, $Array.slice(arguments, 1));

  const listener = {
    callback: cb,
    filter: opt_filter,
    extraInfo: opt_extraInfo,
    matcher:
        null,  // TODO(crbug.com/448893426): implement request filter matching.
  };
  $Array.push(this.listeners, listener);
  this.updateFilters_(listener.filter, listener.extraInfo, /*isAdd=*/ true);
};

// Unregisters a callback.
LazyWebRequestEventImpl.prototype.removeListener = function(cb) {
  // Iterate backwards to safely remove elements while looping.
  for (let i = this.listeners.length - 1; i >= 0; i--) {
    if (this.listeners[i].callback === cb) {
      const listener = this.listeners[i];
      $Array.splice(this.listeners, i, 1);
      this.updateFilters_(
          listener.filter, listener.extraInfo, /*isAdd=*/ false);
    }
  }
};

LazyWebRequestEventImpl.prototype.updateFilters_ = function(
    filter, extraInfo, isAdd) {
  // TODO(crbug.com/448893426): implement filter aggregation.
  CHECK(false);
};

LazyWebRequestEventImpl.prototype.dispatchToListeners_ =
    async function(details) {
  // TODO(crbug.com/448893426): implement listener dispatch.
  CHECK(false);
};

LazyWebRequestEventImpl.prototype.addRules = function(rules, opt_cb) {
  throw new Error('This event does not support rules.');
};

LazyWebRequestEventImpl.prototype.removeRules = function(
    ruleIdentifiers, opt_cb) {
  throw new Error('This event does not support rules.');
};

LazyWebRequestEventImpl.prototype.getRules = function(ruleIdentifiers, cb) {
  throw new Error('This event does not support rules.');
};

exports.$set('getInternalEventName', getInternalEventName);
exports.$set('LazyWebRequestEventImpl', LazyWebRequestEventImpl);
