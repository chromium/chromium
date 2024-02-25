// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from "//ios/web/public/js_messaging/resources/gcrweb.js";

/**
 * @fileoverview Functions used to parse RSS links from a web page.
 */

/* Gets RSS links. */
function getRSSLinks(): string[] {
  const linkTags = document.head.getElementsByTagName('link');
  const rssLinks: string[] = [];

  for (const linkTag of linkTags) {
      if (linkTag.rel === 'alternate' ||
          linkTag.rel === 'service.feed') {
          const type = linkTag.type;
          if (type === 'application/rss+xml'||
              type === 'application/rss+atom' ||
              type === 'application/atom+xml') {
              rssLinks.push(linkTag.href);
          }
      }
  }
  return rssLinks;
}

gCrWeb.rssLink = {getRSSLinks};
