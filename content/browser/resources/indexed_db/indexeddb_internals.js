// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.js';

function initialize() {
  addWebUIListener('origins-ready', onOriginsReady);

  chrome.send('getAllBucketsAcrossAllOrigins');
}

function progressNodeFor(link) {
  return link.parentNode.querySelector('.download-status');
}

function downloadBucketData(event) {
  const link = event.target;
  progressNodeFor(link).style.display = 'inline';
  const path = link.idb_partition_path;
  const bucketId = link.idb_bucket_id;
  sendWithPromise('downloadBucketData', path, bucketId)
      .then(count => onOriginDownloadReady(path, bucketId, count), () => {
        console.error('Error downloading data');
      });
  return false;
}

function forceClose(event) {
  const link = event.target;
  progressNodeFor(link).style.display = 'inline';
  const path = link.idb_partition_path;
  const bucketId = link.idb_bucket_id;
  sendWithPromise('forceClose', path, bucketId)
      .then(count => onForcedClose(path, bucketId, count));
  return false;
}

function withNode(selector, partition_path, bucketId, callback) {
  const links = document.querySelectorAll(selector);
  for (let i = 0; i < links.length; ++i) {
    const link = links[i];
    if (partition_path === link.idb_partition_path &&
        bucketId === link.idb_bucket_id) {
      callback(link);
    }
  }
}
// Fired from the backend after the data has been zipped up, and the
// download manager has begun downloading the file.
function onOriginDownloadReady(partition_path, bucketId, connection_count) {
  withNode('a.download', partition_path, bucketId, function(link) {
    progressNodeFor(link).style.display = 'none';
  });
  withNode('.connection-count', partition_path, bucketId, function(span) {
    span.innerText = connection_count;
  });
}

function onForcedClose(partition_path, bucketId, connection_count) {
  withNode('a.force-close', partition_path, bucketId, function(link) {
    progressNodeFor(link).style.display = 'none';
  });
  withNode('.connection-count', partition_path, bucketId, function(span) {
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
    downloadLinks[i].addEventListener('click', downloadBucketData, false);
  }
  const forceCloseLinks = container.querySelectorAll('a.force-close');
  for (let i = 0; i < forceCloseLinks.length; ++i) {
    forceCloseLinks[i].addEventListener('click', forceClose, false);
  }
}

document.addEventListener('DOMContentLoaded', initialize);
