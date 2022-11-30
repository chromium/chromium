// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting image data.
 */

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.imageFetch = {};

/**
 * Store the imageFetch namespace object in a global __gCrWeb object referenced
 * by a string, so it does not get renamed by closure compiler during the
 * minification.
 */
__gCrWeb['imageFetch'] = __gCrWeb.imageFetch;

/**
 * Returns image data as base64 string, because WKWebView does not support BLOB
 * on messages to native code. Try getting data directly from <img> first, and
 * if failed try downloading by XMLHttpRequest.
 *
 * @param {number} id The ID for curent call. It should be attached to the
 *     message sent back.
 * @param {string} url The URL of the requested image.
 */
__gCrWeb.imageFetch.getImageData = function(id, url) {
  // |from| indicates where the |data| is fetched from.
  var onData = function(data, from) {
      __gCrWeb.common.sendWebKitMessage('ImageFetchMessageHandler',
        {'id': id,
         'data': data,
         'from': from
        });
  };
  var onError = function() {
      __gCrWeb.common.sendWebKitMessage('ImageFetchMessageHandler',
        {'id': id});
  };

  var data = getImageDataByCanvas(url);
  if (data) {
    onData(data, 'canvas');
  } else {
    getImageDataByXMLHttpRequest(url, 100, onData, onError);
  }
};

/**
 * Returns image data directly from <img> by drawing it to <canvas> and export
 * it. If the <img> is cross-origin without "crossorigin=anonymous", this would
 * be prevented by the browser. The exported image is in a resolution of 96 dpi.
 *
 * @param {string} url The URL of the requested image.
 * @return {string|null} Image data in base64 string, or null in these cases:
 *   1. Image is a GIF because GIFs will become static after drawn to <canvas>;
 *   2. No <img> with "src=|url|" is found;
 *   3. Exporting data from <img> failed.
 */
function getImageDataByCanvas(url) {
  var extension = url.split('.').pop().toLowerCase();
  if (extension == 'gif')
    return null;

  for (var key in document.images) {
    var img = document.images[key];
    if (img.src == url) {
      var canvas = document.createElement('canvas');
      canvas.width = img.naturalWidth;
      canvas.height = img.naturalHeight;
      var ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0);
      var data;
      try {
        // If the <img> is cross-domain without "crossorigin=anonymous", an
        // exception will be thrown.
        data = canvas.toDataURL('image/' + extension);
      } catch (error) {
        return null;
      }
      // Remove the "data:type/subtype;base64," header.
      return data.split(',')[1];
    }
  }
  return null;
};

/**
 * Returns image data by downloading it using XMLHttpRequest.
 *
 * @param {string} url The URL of the requested image.
 * @param {number} timeout The timeout in milliseconds for XMLHttpRequest.
 * @param {Function} onData Callback when fetching image data succeeded.
 * @param {Function} onError Callback when fetching image data failed.
 */
function getImageDataByXMLHttpRequest(url, timeout, onData, onError) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.timeout = timeout;
  xhr.responseType = 'blob';

  xhr.onload = function() {
    if (xhr.status != 200) {
      onError();
      return;
    }
    var fr = new FileReader();

    fr.onload = function() {
      onData(btoa(/** @type{string} */ (fr.result)), 'xhr');
    };
    fr.onabort = onError;
    fr.onerror = onError;

    fr.readAsBinaryString(/** @type{!Blob} */ (xhr.response));
  };
  xhr.onabort = onError;
  xhr.onerror = onError;
  xhr.ontimeout = onError;

  xhr.send();
};
