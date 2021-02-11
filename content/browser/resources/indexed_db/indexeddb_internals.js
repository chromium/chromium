// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

function initialize() {
  addWebUIListener('origins-ready', onOriginsReady);

  chrome.send('getAllOrigins');
}

  function progressNodeFor(link) {
    return link.parentNode.querySelector('.download-status');
  }

  function downloadOriginData(event) {
    const link = event.target;
    progressNodeFor(link).style.display = 'inline';
    const path = link.idb_partition_path;
    const origin = link.idb_origin_url;
    sendWithPromise('downloadOriginData', path, origin)
        .then(count => onOriginDownloadReady(path, origin, count), () => {
          console.error('Error downloading data for origin ' + origin);
        });
    return false;
  }

  function forceClose(event) {
    const link = event.target;
    progressNodeFor(link).style.display = 'inline';
    const path = link.idb_partition_path;
    const origin = link.idb_origin_url;
    sendWithPromise('forceClose', path, origin)
        .then(count => onForcedClose(path, origin, count));
    return false;
  }

  function withNode(selector, partition_path, origin_url, callback) {
    const links = document.querySelectorAll(selector);
    for (let i = 0; i < links.length; ++i) {
      const link = links[i];
      if (partition_path === link.idb_partition_path &&
          origin_url === link.idb_origin_url) {
        callback(link);
      }
    }
  }
  // Fired from the backend after the data has been zipped up, and the
  // download manager has begun downloading the file.
  function onOriginDownloadReady(partition_path, origin_url, connection_count) {
    withNode('a.download', partition_path, origin_url, function(link) {
      progressNodeFor(link).style.display = 'none';
    });
    withNode('.connection-count', partition_path, origin_url, function(span) {
      span.innerText = connection_count;
    });
  }

  function onForcedClose(partition_path, origin_url, connection_count) {
    withNode('a.force-close', partition_path, origin_url, function(link) {
      progressNodeFor(link).style.display = 'none';
    });
    withNode('.connection-count', partition_path, origin_url, function(span) {
      span.innerText = connection_count;
    });
  }

  // Fired from the backend with a single partition's worth of
  // IndexedDB metadata.
  function onOriginsReady(origins, partition_path) {
    const template = jstGetTemplate('indexeddb-list-template');
    const container = $('indexeddb-list');
    container.appendChild(template);
    jstProcess(
        new JsEvalContext({idbs: origins, partition_path: partition_path}),
        template);

    const downloadLinks = container.querySelectorAll('a.download');
    for (let i = 0; i < downloadLinks.length; ++i) {
      downloadLinks[i].addEventListener('click', downloadOriginData, false);
    }
    const forceCloseLinks = container.querySelectorAll('a.force-close');
    for (let i = 0; i < forceCloseLinks.length; ++i) {
      forceCloseLinks[i].addEventListener('click', forceClose, false);
    }
  }

  document.addEventListener('DOMContentLoaded', initialize);
