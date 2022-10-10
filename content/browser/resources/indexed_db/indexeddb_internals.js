// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {$} from 'chrome://resources/js/util.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {BucketId} from './bucket_id.mojom-webui.js';
import {IdbBucketMetadata, IdbTransactionMode, IdbTransactionState} from './indexed_db_bucket_types.mojom-webui.js';
import {IdbInternalsHandler, IdbPartitionMetadata} from './indexed_db_internals.mojom-webui.js';


// Converts a mojo time to a JS time.
function convertMojoTimeToJS(mojoTime) {
  // The JS Date() is based off of the number of milliseconds since
  // the UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue|
  // of the base::Time (represented in mojom.Time) represents the
  // number of microseconds since the Windows FILETIME epoch
  // (1601-01-01 00:00:00 UTC). This computes the final JS time by
  // computing the epoch delta and the conversion from microseconds to
  // milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to
  // base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

function convertMojoString16ToJS(mojoString16) {
  return String.fromCharCode(...mojoString16.data);
}

function scopeToString(scope) {
  return `[${scope.map(s => convertMojoString16ToJS(s)).join(', ')}]`;
}

function toMojoEnumName(mojoEnum, value) {
  const name = Object.keys(mojoEnum).find(key => mojoEnum[key] === value);
  // Assert that we found a string and that it starts with the letter k.
  if (name === undefined || name.length < 1 || name[0] !== 'k') {
    throw new Error('toMojoEnumName failed');
  }
  // Remove the letter k.
  return name.slice(1);
}

function toIdbTransactionStateName(state) {
  return toMojoEnumName(IdbTransactionState, state);
}

function toIdbTransactionModeName(mode) {
  return toMojoEnumName(IdbTransactionMode, mode);
}

function definedOrError(value) {
  if (value === null || value === undefined) {
    throw new Error();
  }
  return value;
}

function promisifyMojoResult(remotePromise, valueProp) {
  return new Promise((resolve, reject) => {
    remotePromise.then((response) => {
      if (response.error !== null) {
        reject(response.error);
      } else {
        resolve(response[valueProp]);
      }
    });
  });
}

class IdbInternalsRemote {
  constructor() {
    this.handler_ = IdbInternalsHandler.getRemote();
  }

  getAllBucketsAcrossAllStorageKeys() {
    return promisifyMojoResult(
        this.handler_.getAllBucketsAcrossAllStorageKeys(), 'partitions');
  }

  downloadBucketData(bucketId) {
    return promisifyMojoResult(
        this.handler_.downloadBucketData(bucketId), 'connectionCount');
  }

  forceClose(bucketId) {
    return promisifyMojoResult(
        this.handler_.forceClose(bucketId), 'connectionCount');
  }
}

const internalsRemote = new IdbInternalsRemote();

function initialize() {
  internalsRemote.getAllBucketsAcrossAllStorageKeys()
      .then(partitions => {
        for (const partition of partitions) {
          onStorageKeysReady(partition.bucketList, partition.partitionPath);
        }
      })
      .catch(errorMsg => console.error(errorMsg));
}

function progressNodeFor(link) {
  const parentNode = definedOrError(link.parentNode);
  const progressNode =
      definedOrError(parentNode.querySelector('.download-status'));
  return progressNode;
}

function downloadBucketData(event) {
  const link = definedOrError(event.target);
  progressNodeFor(link).style.display = 'inline';
  const path = {path: link.idbPartitionPath};
  const bucketId = link.idbBucketId;
  internalsRemote.downloadBucketData(bucketId)
      .then(
          connectionCount =>
              onStorageKeyDownloadReady(path, bucketId, connectionCount))
      .catch(errorMsg => console.error(errorMsg));
  return false;
}

function forceClose(event) {
  const link = definedOrError(event.target);
  progressNodeFor(link).style.display = 'inline';
  const path = {path: link.idbPartitionPath};
  const bucketId = link.idbBucketId;
  internalsRemote.forceClose(bucketId)
      .then(connectionCount => onForcedClose(path, bucketId, connectionCount))
      .catch(errorMsg => console.error(errorMsg));
  return false;
}

function withNode(selector, partitionPath, bucketId, callback) {
  const links = document.querySelectorAll(selector);
  for (let i = 0; i < links.length; ++i) {
    const link = links[i];
    if (partitionPath.path === link.idbPartitionPath &&
        bucketId.value === link.idbBucketId.value) {
      callback(link);
    }
  }
}
// Fired from the backend after the data has been zipped up, and the
// download manager has begun downloading the file.
function onStorageKeyDownloadReady(partitionPath, bucketId, connectionCount) {
  withNode('a.download', partitionPath, bucketId, link => {
    progressNodeFor(link).style.display = 'none';
  });
  withNode('.connection-count', partitionPath, bucketId, span => {
    span.innerText = connectionCount.toString();
  });
}

function onForcedClose(partitionPath, bucketId, connectionCount) {
  withNode('a.force-close', partitionPath, bucketId, (link) => {
    progressNodeFor(link).style.display = 'none';
  });
  withNode('.connection-count', partitionPath, bucketId, (span) => {
    span.innerText = connectionCount.toString();
  });
}

// Fired from the backend with a single partition's worth of
// IndexedDB metadata.
function onStorageKeysReady(storageKeys, partitionPath) {
  const template = jstGetTemplate('indexeddb-list-template');
  const container = $('indexeddb-list');
  container.appendChild(template);
  jstProcess(
      new JsEvalContext({
        storageKeys,
        partitionPath,
        convertMojoTimeToJS,
        convertMojoString16ToJS,
        toIdbTransactionStateName,
        toIdbTransactionModeName,
        scopeToString,
      }),
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
