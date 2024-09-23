// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common custom element registration code for the various guest view
// containers.

var $CustomElementRegistry =
    require('safeMethods').SafeMethods.$CustomElementRegistry;
var $Element = require('safeMethods').SafeMethods.$Element;
var $EventTarget = require('safeMethods').SafeMethods.$EventTarget;
var $HTMLElement = require('safeMethods').SafeMethods.$HTMLElement;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var logging = requireNative('logging');

// Conceptually, these are methods on GuestViewContainerElement.prototype.
// However, since that is exposed to users, we only set these callbacks on
// the prototype temporarily during the custom element registration.
var customElementCallbacks = {
  connectedCallback: function() {
    var internal = privates(this).internal;
    if (!internal)
      return;

    internal.elementAttached = true;
    internal.willAttachElement();
    internal.onElementAttached();
  },

  attributeChangedCallback: function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal)
      return;

    // Let the changed attribute handle its own mutation.
    internal.attributes[name].maybeHandleMutation(oldValue, newValue);
  },

  disconnectedCallback: function() {
    var internal = privates(this).internal;
    if (!internal)
      return;

    internal.elementAttached = false;
    internal.internalInstanceId = 0;
    internal.guest.destroy();
    internal.onElementDetached();
  }
};

// Registers the guestview as a custom element.
// |containerElementType| is a GuestViewContainerElement (e.g. WebViewElement)
function registerElement(elementName, containerElementType) {
  GuestViewInternalNatives.AllowGuestViewElementDefinition(() => {
    // We set the lifecycle callbacks so that they're available during
    // registration. Once that's done, we'll delete them so developers cannot
    // call them and produce unexpected behaviour.
    GuestViewContainerElement.prototype.connectedCallback =
        customElementCallbacks.connectedCallback;
    GuestViewContainerElement.prototype.disconnectedCallback =
        customElementCallbacks.disconnectedCallback;
    GuestViewContainerElement.prototype.attributeChangedCallback =
        customElementCallbacks.attributeChangedCallback;

    $CustomElementRegistry.define(
        window.customElements, $String.toLowerCase(elementName),
        containerElementType);
    $Object.defineProperty(window, elementName, {
      value: containerElementType,
    });

    delete GuestViewContainerElement.prototype.connectedCallback;
    delete GuestViewContainerElement.prototype.disconnectedCallback;
    delete GuestViewContainerElement.prototype.attributeChangedCallback;

    // Now that |observedAttributes| has been retrieved, we can hide it from
    // user code as well.
    delete containerElementType.observedAttributes;
  });
}

function getMethodType(containerElementType, containerType, internalApi, name) {
  if (containerElementType.prototype[name]) {
    return 'CONTAINER_ELEMENT';
  } else if (containerType.prototype[name]) {
    return 'CONTAINER';
  } else if (internalApi && internalApi[name]) {
    return 'INTERNAL';
  }
  return 'UNKNOWN';
};

function createMethodHandler(
    containerElementType, containerType, internalApi, methodName) {
  switch (getMethodType(containerElementType, containerType, internalApi,
                        methodName)) {
    case 'CONTAINER_ELEMENT':
      return containerElementType.prototype[methodName];

    case 'CONTAINER':
      return function(var_args) {
        const internal = privates(this).internal;
        return $Function.apply(internal[methodName], internal, arguments);
      };

    case 'INTERNAL':
      return function(var_args) {
        const internal = privates(this).internal;
        const instanceId = internal.guest.getId();
        if (!instanceId) {
          return false;
        }
        const args = $Array.concat([instanceId], $Array.slice(arguments));
        $Function.apply(internalApi[methodName], null, args);
        return true;
      };

    default:
      logging.DCHECK(false, `${methodName} has no implementation.`);
  }
};

function promisifyMethodHandler(
    containerElementType, containerType, internalApi, methodDetails, handler) {
  const methodType = getMethodType(containerElementType, containerType,
                                   internalApi, methodDetails.name);
  return function(var_args) {
    const args = $Array.slice(arguments);
    if (args[methodDetails.callbackIndex] !== undefined) {
      throw new Error('Callback form deprecated, see API doc ' +
                      'for correct usage.');
    }
    return new $Promise.self((resolve, reject) => {
      if (methodType === 'INTERNAL') {
        if (!privates(this).internal.guest.getId()) {
          reject('The embedded page has been destroyed.');
          return;
        }
      }
      const callback = function(result) {
        if (bindingUtil.hasLastError()) {
          reject(bindingUtil.getLastErrorMessage());
          bindingUtil.clearLastError();
          return;
        }
        resolve(result);
      };
      args[methodDetails.callbackIndex] = callback;
      $Function.apply(handler, this, args);
    });
  };
};

// Forward public API methods from |containerElementType|'s prototype to their
// internal implementations. If the method is defined on |containerType|, we
// forward to that. Otherwise, we forward to the method on |internalApi|. For
// APIs in |promiseMethodDetails|, the forwarded API will have a handler
// created based on the original function reference on the original object. This
// is to support any callers that want to get a reference to that handle before
// it's promise-wrapped in a later stage when |promiseWrap| is called.
function forwardApiMethods(
    containerElementType, containerType, internalApi, methodNames,
    promiseMethodDetails) {
  for (const methodName of methodNames) {
    containerElementType.prototype[methodName] =
        createMethodHandler(containerElementType, containerType, internalApi,
                            methodName);
  }

  // If `promiseMethodDetails` is defined, create handlers for each of those
  // functions. They'll later be promisified by a subsequent call.
  for (const methodDetails of promiseMethodDetails) {
    const handler = createMethodHandler(containerElementType, containerType,
                                        internalApi, methodDetails.name);
    containerElementType.prototype[methodDetails.name] = handler;
  }
}

// For APIs in |promiseMethodDetails|, the forwarded API will return a Promise
// that resolves with the result of the API or rejects with the error that is
// produced if the callback parameter is not defined.
function promiseWrap(
    containerElementType, containerType, internalApi, promiseMethodDetails) {
  for (const methodDetails of promiseMethodDetails) {
    const handler = containerElementType.prototype[methodDetails.name];
    containerElementType.prototype[methodDetails.name] =
        promisifyMethodHandler(containerElementType, containerType,
                               internalApi, methodDetails, handler);
  }
}

class GuestViewContainerElement extends HTMLElement {}

// Override |focus| to let |internal| handle it.
GuestViewContainerElement.prototype.focus = function() {
  var internal = privates(this).internal;
  if (!internal)
    return;

  internal.focus();
};

// Exports.
exports.$set('GuestViewContainerElement', GuestViewContainerElement);
exports.$set('registerElement', registerElement);
exports.$set('forwardApiMethods', forwardApiMethods);
exports.$set('promiseWrap', promiseWrap);
