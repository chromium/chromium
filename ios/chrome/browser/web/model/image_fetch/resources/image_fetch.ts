// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting image data.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Returns image data as base64 string, because WKWebView does not support BLOB
 * on messages to native code.
 * @param id The ID for current call. It should be attached to the message
 *    sent back.
 * @param url The URL of the requested image.
 */
function getImageData(id: number, url: string): void {
  // `from` indicates where the `data` is fetched from.
  const onData = function(data: string, from: string) {
    sendWebKitMessage('ImageFetchMessageHandler',
        {'id': id,
         'data': data,
         'from': from
        });
  };
  const onError = function() {
    sendWebKitMessage('ImageFetchMessageHandler', {'id': id});
  };

  // Try getting data directly from <img> first.
  // If unsuccessful, download the data via XMLHttpRequest.
  const data = getImageDataByCanvas(url);
  if (data) {
    onData(data, 'canvas');
    return;
  }

  getImageDataByXMLHttpRequest(url, 100, onData, onError);
};

/**
 * Returns image data directly from <img> by drawing it to <canvas> and export
 * it. If the <img> is cross-origin without "crossorigin=anonymous", this would
 * be prevented by the browser. The exported image is in a resolution of 96 dpi.
 * The function returns image data in base64 string, or null in these cases:
 *   1. Image is a GIF because GIFs will become static after drawn to <canvas>;
 *   2. No <img> with "src=`url`" is found;
 *   3. Exporting data from <img> failed.
 */
function getImageDataByCanvas(url: string): string | null {
  const extension = url.split('.').pop();
  if (!extension || extension.toLowerCase() === 'gif')
    return null;

  for (const img of document.images) {
    if (img && img.src == url) {
      const canvas = document.createElement('canvas');
      canvas.width = img.naturalWidth;
      canvas.height = img.naturalHeight;
      const ctx = canvas.getContext('2d');
      if (!ctx) continue;
      ctx.drawImage(img, 0, 0);
      let data;
      try {
        // If the <img> is cross-domain without "crossorigin=anonymous", an
        // exception will be thrown.
        data = canvas.toDataURL('image/' + extension);
      } catch (error) {
        return null;
      }
      // Remove the "data:type/subtype;base64," header.
      return data.split(',')[1] as string;
    }
  }
  return null;
};

/**
 * Returns image data by downloading it using XMLHttpRequest.
 *
 * @param url The URL of the requested image.
 * @param timeout The timeout in milliseconds for XMLHttpRequest.
 * @param onData Callback when fetching image data succeeded.
 * @param onError Callback when fetching image data failed.
 */
function getImageDataByXMLHttpRequest(
    url: string, timeout: number,
    onData: Function, onError: (() => void)): void {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.timeout = timeout;
  xhr.responseType = 'blob';

  xhr.onload = function() {
    if (xhr.status != 200) {
      onError();
      return;
    }
    const fr = new FileReader();

    fr.onload = function() {
      onData(btoa(fr.result as string), 'xhr');
    };
    fr.onabort = onError;
    fr.onerror = onError;

    fr.readAsBinaryString(xhr.response);
  };
  xhr.onabort = onError;
  xhr.onerror = onError;
  xhr.ontimeout = onError;

  xhr.send();
};

gCrWeb.imageFetch = {getImageData}
