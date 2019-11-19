// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var printerProviderInternal = getInternalApi('printerProviderInternal');
var blobNatives = requireNative('blob_natives');

var printerProviderSchema =
    requireNative('schema_registry').GetSchema('printerProvider')

var utils = require('utils');

// Custom bindings for chrome.printerProvider API.
// The bindings are used to implement callbacks for the API events. Internally
// each event is passed requestId argument used to identify the callback
// associated with the event. This argument is massaged out from the event
// arguments before dispatching the event to consumers. A callback is appended
// to the event arguments. The callback wraps an appropriate
// chrome.printerProviderInternal API function that is used to report the event
// result from the extension. The function is passed requestId and values
// provided by the extension. It validates that the values provided by the
// extension match chrome.printerProvider event callback schemas. It also
// ensures that a callback is run at most once. In case there is an exception
// during event dispatching, the chrome.printerProviderInternal function
// is called with a default error value.
//

// Handles a chrome.printerProvider event as described in the file comment.
// |eventName|: The event name.
// |prepareArgsForDispatch|: Function called before dispatching the event to
//     the extension. It's called with original event |args| list and callback
//     that should be called when the |args| are ready for dispatch. The
//     callbacks should report whether the argument preparation was successful.
//     The function should not change the first argument, which contains the
//     request id.
// |resultreporter|: The function that should be called to report event result.
//     One of chrome.printerProviderInternal API functions.
function handleEvent(eventName, prepareArgsForDispatch, resultReporter) {
  var eventSchema =
      utils.lookup(printerProviderSchema.events, 'name', eventName);
  var callbackSchema =
      utils.lookup(eventSchema.parameters, 'type', 'function').parameters;
  var fullEventName = 'printerProvider.' + eventName;

  bindingUtil.addCustomSignature(fullEventName, callbackSchema);

  bindingUtil.registerEventArgumentMassager(fullEventName,
                                            function(args, dispatch) {
    var responded = false;

    // Function provided to the extension as the event callback argument.
    // It makes sure that the event result hasn't previously been returned
    // and that the provided result matches the callback schema. In case of
    // an error it throws an exception.
    var reportResult = function(result) {
      if (responded)
        throw new Error('Event callback must not be called more than once.');

      var finalResult = null;
      try {
        // throws on failure
        bindingUtil.validateCustomSignature(fullEventName, [result]);
        finalResult = result;
      } finally {
        responded = true;
        resultReporter(args[0] /* requestId */, finalResult);
      }
    };

    prepareArgsForDispatch(args, function(success) {
      if (!success) {
        // Do not throw an exception since the extension should not yet be
        // aware of the event.
        resultReporter(args[0] /* requestId */, null);
        return;
      }
      dispatch(args.slice(1).concat(reportResult));
    });
  });
}

// Sets up printJob.document property for a print request.
function createPrintRequestBlobArguments(args, callback) {
  printerProviderInternal.getPrintData(args[0] /* requestId */,
                                       function(blobInfo) {
    if (chrome.runtime.lastError) {
      callback(false);
      return;
    }

    // |args[1]| is printJob.
    args[1].document = blobNatives.TakeBrowserProcessBlob(
        blobInfo.blobUuid, blobInfo.type, blobInfo.size);
    callback(true);
  });
}

handleEvent('onGetPrintersRequested',
            function(args, callback) { callback(true); },
            printerProviderInternal.reportPrinters);

handleEvent('onGetCapabilityRequested',
            function(args, callback) { callback(true); },
            printerProviderInternal.reportPrinterCapability);

handleEvent('onPrintRequested',
            createPrintRequestBlobArguments,
            printerProviderInternal.reportPrintResult);

handleEvent('onGetUsbPrinterInfoRequested',
            function(args, callback) { callback(true); },
            printerProviderInternal.reportUsbPrinterInfo);
