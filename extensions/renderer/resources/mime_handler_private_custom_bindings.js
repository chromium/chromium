// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom bindings for the mime handler API.
 */

var exceptionHandler = require('uncaught_exception_handler');
var logActivity = requireNative('activityLogger');

var NO_STREAM_ERROR =
    'Streams are only available from a mime handler view guest.';
var STREAM_ABORTED_ERROR = 'Stream has been aborted.';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
loadScript('extensions/common/api/mime_handler.mojom');

// DO NOT USE. This causes problems with safe builtins, and makes migration to
// native bindings more difficult.
function handleRequestWithPromiseDoNotUse(
    binding, apiName, methodName, customizedFunction) {
  var fullName = apiName + '.' + methodName;
  var extensionId = requireNative('process').GetExtensionId();
  binding.setHandleRequest(methodName, function() {
    logActivity.LogAPICall(extensionId, fullName, $Array.slice(arguments));
    var stack = exceptionHandler.getExtensionStackTrace();
    var callback = arguments[arguments.length - 2];
    var args = $Array.slice(arguments, 0, arguments.length - 2);
    var keepAlive = require('keep_alive').createKeepAlive();
    $Function.apply(customizedFunction, this, args).then(function(result) {
      if (callback) {
        exceptionHandler.safeCallbackApply(
            fullName, callback, [result], stack);
      }
    }).catch(function(error) {
      if (callback) {
        var message = exceptionHandler.safeErrorToString(error, true);
        bindingUtil.runCallbackWithLastError(message, callback);
      }
    }).then(function() {
      keepAlive.close();
    });
  });
};

var servicePtr = new extensions.mimeHandler.MimeHandlerServicePtr;
Mojo.bindInterface(
    extensions.mimeHandler.MimeHandlerService.name,
    mojo.makeRequest(servicePtr).handle);
var beforeUnloadControlPtr =
    new extensions.mimeHandler.BeforeUnloadControlPtr;
Mojo.bindInterface(
    extensions.mimeHandler.BeforeUnloadControl.name,
    mojo.makeRequest(beforeUnloadControlPtr).handle);

// Stores a promise to the GetStreamInfo() result to avoid making additional
// calls in response to getStreamInfo() calls.
var streamInfoPromise;

function throwNoStreamError() {
  throw new Error(NO_STREAM_ERROR);
}

function createStreamInfoPromise() {
  return servicePtr.getStreamInfo().then(function(result) {
    if (!result.streamInfo)
      throw new Error(STREAM_ABORTED_ERROR);
    return result.streamInfo;
  }, throwNoStreamError);
}

function constructStreamInfoDict(streamInfo) {
  var headers = {};
  for (var header of streamInfo.responseHeaders) {
    headers[header[0]] = header[1];
  }
  return {
    mimeType: streamInfo.mimeType,
    originalUrl: streamInfo.originalUrl,
    streamUrl: streamInfo.streamUrl,
    tabId: streamInfo.tabId,
    embedded: !!streamInfo.embedded,
    responseHeaders: headers,
  };
}

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  handleRequestWithPromiseDoNotUse(
      apiFunctions, 'mimeHandlerPrivate', 'getStreamInfo',
      function() {
    if (!streamInfoPromise)
      streamInfoPromise = createStreamInfoPromise();
    return streamInfoPromise.then(constructStreamInfoDict);
  });

  apiFunctions.setHandleRequest(
      'setPdfPluginAttributes', function(pdfPluginAttributes) {
        servicePtr.setPdfPluginAttributes(pdfPluginAttributes);
      });

  handleRequestWithPromiseDoNotUse(
      apiFunctions, 'mimeHandlerPrivate', 'setShowBeforeUnloadDialog',
      function(showDialog) {
    return beforeUnloadControlPtr.setShowBeforeUnloadDialog(showDialog);
  });
});
