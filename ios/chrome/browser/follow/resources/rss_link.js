// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Functions used to parse RSS links from a web page.
 */

__gCrWeb['rssLink'] = {};

/* Gets RSS links. */
__gCrWeb.rssLink.getRSSLinks = function() {
  const linkTags = document.head.getElementsByTagName('link');
  const rssLinks = [];
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
