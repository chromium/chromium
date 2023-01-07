// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaSourceUtils provides basic functionality to load content using MSE API.
var MediaSourceUtils = new function() {
}

MediaSourceUtils.loadMediaSourceFromTestConfig = function(testConfig,
                                                          appendCallbackFn) {
  return this.loadMediaSource(testConfig.mediaFile,
                              testConfig.mediaType,
                              appendCallbackFn);
};

MediaSourceUtils.loadMediaSource = function(mediaFiles,
                                            mediaTypes,
                                            appendCallbackFn) {
  if (!mediaFiles || !mediaTypes)
    Utils.failTest('Missing parameters in loadMediaSource().');

  var mediaFiles = Utils.convertToArray(mediaFiles);
  var mediaTypes = Utils.convertToArray(mediaTypes);
  var totalAppended = 0;
  function onSourceOpen(e) {
    Utils.timeLog('onSourceOpen', e);
    // We can load multiple media files using the same media type. However, if
    // more than one media type is used, we expect to have a media type entry
    // for each corresponding media file.
    var srcBuffer = null;
    for (var i = 0; i < mediaFiles.length; i++) {
      if (i == 0 || mediaFiles.length == mediaTypes.length) {
        Utils.timeLog('Creating a source buffer for type ' + mediaTypes[i]);
        try {
          srcBuffer = mediaSource.addSourceBuffer(mediaTypes[i]);
        } catch (e) {
          Utils.failTest('Exception adding source buffer: ' + e.message);
          return;
        }
      }
      doAppend(mediaFiles[i], srcBuffer);
    }
  }

  function doAppend(mediaFile, srcBuffer) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', mediaFile);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('load', function(e) {
      var onUpdateEnd = function(e) {
        Utils.timeLog('End of appending buffer from ' + mediaFile);
        srcBuffer.removeEventListener('updateend', onUpdateEnd);
        totalAppended++;
        if (totalAppended == mediaFiles.length) {
          if (appendCallbackFn)
            appendCallbackFn(mediaSource);
          else
            mediaSource.endOfStream();
        }
      };
      srcBuffer.addEventListener('updateend', onUpdateEnd);
      srcBuffer.appendBuffer(new Uint8Array(e.target.response));
    });
    xhr.send();
  }

  var mediaSource = new MediaSource();
  mediaSource.addEventListener('sourceopen', onSourceOpen);
  return mediaSource;
};
