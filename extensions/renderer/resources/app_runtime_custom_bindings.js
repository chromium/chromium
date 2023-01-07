// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the chrome.app.runtime API.

var AppViewGuestInternal;
// appViewGuestInternal isn't available in lock screen contexts.
if (requireNative('v8_context').GetAvailability('appViewGuestInternal').
        is_available) {
  AppViewGuestInternal = getInternalApi('appViewGuestInternal');
}
var fileSystemHelpers = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;
var entryIdManager = require('entryIdManager');

if (AppViewGuestInternal) {
  bindingUtil.registerEventArgumentMassager('app.runtime.onEmbedRequested',
                                            function(args, dispatch) {
    var appEmbeddingRequest = args[0];
    var id = appEmbeddingRequest.guestInstanceId;
    delete appEmbeddingRequest.guestInstanceId;
    appEmbeddingRequest.allow = function(url) {
      AppViewGuestInternal.attachFrame(url, id);
    };

    appEmbeddingRequest.deny = function() {
      AppViewGuestInternal.denyRequest(id);
    };

    dispatch([appEmbeddingRequest]);
  });
}

bindingUtil.registerEventArgumentMassager('app.runtime.onLaunched',
                                          function(args, dispatch) {
  var launchData = args[0];
  if (launchData.items) {
    // An onLaunched corresponding to file_handlers in the app's manifest.
    var items = [];
    var numItems = launchData.items.length;
    var itemLoaded = function(err, item) {
      if (err) {
        console.error('Error getting fileEntry, code: ' + err.code);
      } else {
        $Array.push(items, item);
      }
      if (--numItems === 0) {
        var data = {
          isDemoSession: launchData.isDemoSession,
          isKioskSession: launchData.isKioskSession,
          isPublicSession: launchData.isPublicSession,
          source: launchData.source,
          actionData: launchData.actionData
        };
        if (items.length !== 0) {
          data.id = launchData.id;
          data.items = items;
        }
        dispatch([data]);
      }
    };
    $Array.forEach(launchData.items, function(item) {
      var fs = GetIsolatedFileSystem(item.fileSystemId);
      if (item.isDirectory) {
        fs.root.getDirectory(item.baseName, {}, function(dirEntry) {
          entryIdManager.registerEntry(item.entryId, dirEntry);
          itemLoaded(null, {entry: dirEntry});
        }, function(fileError) {
          itemLoaded(fileError);
        });
      } else {
        fs.root.getFile(item.baseName, {}, function(fileEntry) {
          entryIdManager.registerEntry(item.entryId, fileEntry);
          itemLoaded(null, {entry: fileEntry, type: item.mimeType});
        }, function(fileError) {
          itemLoaded(fileError);
        });
      }
    });
  } else {
    // Default case. This currently covers an onLaunched corresponding to
    // url_handlers in the app's manifest.
    dispatch([launchData]);
  }
});
