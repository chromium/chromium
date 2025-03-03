// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a function which collects favicon
 * details from the current document
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

declare interface FaviconData {
  rel: string;
  href: string|undefined;
  sizes?: string;
}

/**
 * Retrieves favicon information.
 *
 * @return {FaviconData[]} An array of objects containing favicon data.
 */
function getFavicons(): FaviconData[] {
  const favicons: FaviconData[] = [];
  const links = document.getElementsByTagName('link');
  const linkCount = links.length;
  for (let i = 0; i < linkCount; ++i) {
    const link = links[i];
    const rel = link?.rel.toLowerCase();

    if (rel === 'shortcut icon' ||
        rel === 'icon' ||
        rel === 'apple-touch-icon' ||
        rel === 'apple-touch-icon-precomposed') {
      const favicon: FaviconData = {rel: rel, href: link?.href};
      const size_value = link?.sizes?.value;

      if (size_value) {
        favicon.sizes = size_value;
      }
      favicons.push(favicon);
    }
  }
  return favicons;
}

export function sendFaviconUrls(): void {
  sendWebKitMessage('FaviconUrlsHandler', getFavicons());
}
