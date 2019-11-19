// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom bindings for the mime handler API.
 */

var utils = require('utils');

var NO_STREAM_ERROR =
    'Streams are only available from a mime handler view guest.';
var STREAM_ABORTED_ERROR = 'Stream has been aborted.';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
loadScript('extensions/common/api/mime_handler.mojom');

var servicePtr = new extensions.mimeHandler.MimeHandlerServicePtr;
Mojo.bindInterface(extensions.mimeHandler.MimeHandlerService.name,
                   mojo.makeRequest(servicePtr).handle, "context", true);
var beforeUnloadControlPtr =
    new extensions.mimeHandler.BeforeUnloadControlPtr;
Mojo.bindInterface(
    extensions.mimeHandler.BeforeUnloadControl.name,
    mojo.makeRequest(beforeUnloadControlPtr).handle, "context", true);

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
  utils.handleRequestWithPromiseDoNotUse(
      apiFunctions, 'mimeHandlerPrivate', 'getStreamInfo',
      function() {
    if (!streamInfoPromise)
      streamInfoPromise = createStreamInfoPromise();
    return streamInfoPromise.then(constructStreamInfoDict);
  });

  utils.handleRequestWithPromiseDoNotUse(
      apiFunctions, 'mimeHandlerPrivate', 'setShowBeforeUnloadDialog',
      function(showDialog) {
    return beforeUnloadControlPtr.setShowBeforeUnloadDialog(showDialog);
  });
});
