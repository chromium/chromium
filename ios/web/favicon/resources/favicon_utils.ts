// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a function which collects favicon
 * details from the current document
 */

import { sendWebKitMessage } from '//ios/web/public/js_messaging/resources/utils.js'

declare interface FaviconData {
  rel: string,
  href: string | undefined,
  sizes?: string
}

/**
 * Retrieves favicon information.
 *
 * @return {FaviconData[]} An array of objects containing favicon data.
 */
function getFavicons(): FaviconData[] {
  let favicons: FaviconData[] = [];
  let links = document.getElementsByTagName('link');
  let linkCount = links.length;
  for (var i = 0; i < linkCount; ++i) {
    let link = links[i];
    let rel = link?.rel.toLowerCase();

    if (rel === 'shortcut icon' ||
        rel === 'icon' ||
        rel === 'apple-touch-icon' ||
        rel === 'apple-touch-icon-precomposed') {
      let favicon: FaviconData =
          {rel: rel, href: link?.href};
      let size_value = link?.sizes?.value;

      if (size_value) {
        favicon.sizes = size_value;
      }
      favicons.push(favicon);
    }
  }
  return favicons;
}

function sendFaviconUrls(): void {
  sendWebKitMessage('FaviconUrlsHandler', getFavicons());
}

export {sendFaviconUrls}