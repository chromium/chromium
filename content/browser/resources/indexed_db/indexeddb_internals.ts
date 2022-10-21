// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {$} from 'chrome://resources/js/util.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {BucketId} from './bucket_id.mojom-webui.js';
import {IdbBucketMetadata, IdbTransactionMode, IdbTransactionState} from './indexed_db_bucket_types.mojom-webui.js';
import {IdbInternalsHandler, IdbInternalsHandlerInterface, IdbPartitionMetadata} from './indexed_db_internals.mojom-webui.js';

// TODO: This comes from components/flags_ui/resources/flags.ts. It should be
// extracted into a tools/typescript/definitions/jstemplate.d.ts file, and
// include that as part of build_webui()'s ts_definitions, instead of copying it
// here.
declare global {
  class JsEvalContext {
    constructor(data: any);
  }

  function jstProcess(context: JsEvalContext, template: HTMLElement): void;
  function jstGetTemplate(templateName: string): HTMLElement;
}

// Converts a mojo time to a JS time.
function convertMojoTimeToJS(mojoTime: Time): Date {
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

function convertMojoString16ToJS(mojoString16: String16): string {
  return String.fromCharCode(...mojoString16.data);
}

function scopeToString(scope: String16[]): string {
  return `[${scope.map(s => convertMojoString16ToJS(s)).join(', ')}]`;
}

interface MojoEnum {
  [key: string]: number;
}

function toMojoEnumName(mojoEnum: MojoEnum, value: number): string {
  const name: string|undefined =
      Object.keys(mojoEnum).find(key => mojoEnum[key] === value);
  // Assert that we found a string and that it starts with the letter k.
  assert(
      name !== undefined && name.length > 0 && name[0] === 'k',
      'toMojoEnumName failed');
  // Remove the letter k.
  return name.slice(1);
}

function toIdbTransactionStateName(state: IdbTransactionState): string {
  return toMojoEnumName(IdbTransactionState, state);
}

function toIdbTransactionModeName(mode: IdbTransactionMode): string {
  return toMojoEnumName(IdbTransactionMode, mode);
}

interface MojomResponse<T> {
  error: string|null;
  [key: string]: T|string|null;
}

function promisifyMojoResult<T>(
    remotePromise: Promise<MojomResponse<T>>,
    valueProp: keyof MojomResponse<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    remotePromise.then((response: MojomResponse<T>) => {
      if (response.error !== null) {
        reject(response.error);
      } else {
        resolve(response[valueProp] as T);
      }
    });
  });
}

class IdbInternalsRemote {
  private handler: IdbInternalsHandlerInterface =
      IdbInternalsHandler.getRemote();

  getAllBucketsAcrossAllStorageKeys(): Promise<IdbPartitionMetadata[]> {
    return promisifyMojoResult(
        this.handler.getAllBucketsAcrossAllStorageKeys(), 'partitions');
  }

  downloadBucketData(bucketId: BucketId): Promise<bigint> {
    return promisifyMojoResult(
        this.handler.downloadBucketData(bucketId), 'connectionCount');
  }

  forceClose(bucketId: BucketId): Promise<bigint> {
    return promisifyMojoResult(
        this.handler.forceClose(bucketId), 'connectionCount');
  }
}

const internalsRemote = new IdbInternalsRemote();

type BucketLinkElement = HTMLAnchorElement&{
  // These fields are filled by the jstemplate annotations in the HTML code
  idbPartitionPath: string,
  idbBucketId: BucketId,
};

function initialize() {
  internalsRemote.getAllBucketsAcrossAllStorageKeys()
      .then(partitions => {
        for (const partition of partitions) {
          onStorageKeysReady(partition.bucketList, partition.partitionPath);
        }
      })
      .catch(errorMsg => console.error(errorMsg));
}

function progressNodeFor(link: BucketLinkElement) {
  return withNode(link, '.download-status');
}

function downloadBucketData(event: Event) {
  const link = event.target as BucketLinkElement;
  progressNodeFor(link).style.display = 'inline';
  const bucketId = link.idbBucketId;
  internalsRemote.downloadBucketData(bucketId)
      .then(connectionCount => onStorageKeyDownloadReady(link, connectionCount))
      .catch(errorMsg => console.error(errorMsg));
  return false;
}

function forceClose(event: Event) {
  const link = event.target as BucketLinkElement;
  progressNodeFor(link).style.display = 'inline';
  const bucketId = link.idbBucketId;
  internalsRemote.forceClose(bucketId)
      .then(connectionCount => onForcedClose(link, connectionCount))
      .catch(errorMsg => console.error(errorMsg));
  return false;
}

function withNode(link: BucketLinkElement, selector: string) {
  const bucketElement = link.closest<HTMLElement>('.indexeddb-item');
  assert(bucketElement);
  const selectedElement = bucketElement.querySelector<HTMLElement>(selector);
  assert(selectedElement);
  return selectedElement;
}
// Fired from the backend after the data has been zipped up, and the
// download manager has begun downloading the file.
function onStorageKeyDownloadReady(
    link: BucketLinkElement, connectionCount: bigint) {
  progressNodeFor(link).style.display = 'none';
  withNode(link, '.connection-count').innerText = connectionCount.toString();
}

function onForcedClose(link: BucketLinkElement, connectionCount: bigint) {
  progressNodeFor(link).style.display = 'none';
  withNode(link, '.connection-count').innerText = connectionCount.toString();
}

// Fired from the backend with a single partition's worth of
// IndexedDB metadata.
function onStorageKeysReady(
    storageKeys: IdbBucketMetadata[], partitionPath: FilePath) {
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
    downloadLinks[i]!.addEventListener('click', downloadBucketData, false);
  }
  const forceCloseLinks = container.querySelectorAll('a.force-close');
  for (let i = 0; i < forceCloseLinks.length; ++i) {
    forceCloseLinks[i]!.addEventListener('click', forceClose, false);
  }
}

document.addEventListener('DOMContentLoaded', initialize);
