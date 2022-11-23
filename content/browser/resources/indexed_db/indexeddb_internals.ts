// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {BucketId} from './bucket_id.mojom-webui.js';
import {IdbTransactionMode, IdbTransactionState} from './indexed_db_bucket_types.mojom-webui.js';
import {IdbInternalsHandler, IdbInternalsHandlerInterface, IdbPartitionMetadata} from './indexed_db_internals.mojom-webui.js';
import {SchemefulSite} from './schemeful_site.mojom-webui.js';

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

// Methods to convert mojo values to strings or to objects with readable
// toString values. Accessible to jstemplate html code.
const stringifyMojo = {
  time(mojoTime: Time): Date {
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
  },

  string16(mojoString16: String16): string {
    return String.fromCharCode(...mojoString16.data);
  },

  scope(mojoScope: String16[]): string {
    return `[${mojoScope.map(s => stringifyMojo.string16(s)).join(', ')}]`;
  },

  origin(mojoOrigin: Origin): string {
    const {scheme, host, port} = mojoOrigin;
    const portSuf = (port === 0 ? '' : `:${port}`);
    return `${scheme}://${host}${portSuf}`;
  },

  schemefulSite(mojoSite: SchemefulSite): string {
    return stringifyMojo.origin(mojoSite.siteAsOrigin);
  },

  transactionState(mojoState: IdbTransactionState): string {
    return toMojoEnumName(IdbTransactionState, mojoState);
  },

  transactionMode(mojoMode: IdbTransactionMode): string {
    return toMojoEnumName(IdbTransactionMode, mojoMode);
  },

  partitionBucketCount(mojoPartition: IdbPartitionMetadata): number {
    let count = 0;
    mojoPartition.originList.forEach(
        origin => origin.storageKeys.forEach(
            storageKey => count += storageKey.buckets.length));
    return count;
  },
};

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

function initialize() {
  internalsRemote.getAllBucketsAcrossAllStorageKeys()
      .then(onStorageKeysReady)
      .catch(errorMsg => console.error(errorMsg));
}

class BucketElement extends HTMLElement {
  // this field is filled by the jstemplate annotations in the HTML code
  idbBucketId: BucketId;

  progressNode: HTMLElement;
  connectionCountNode: HTMLElement;

  constructor() {
    super();
    this.addControlListener('.download', internalsRemote.downloadBucketData);
    this.addControlListener('.force-close', internalsRemote.forceClose);

    this.progressNode = this.getNode('.download-status');
    this.connectionCountNode = this.getNode('.connection-count');
  }

  private getNode(selector: string) {
    const controlNode = this.querySelector<HTMLElement>(`${selector}`);
    assert(controlNode);
    return controlNode;
  }

  private addControlListener(
      selector: string, idbMojoFunc: (id: BucketId) => Promise<bigint>) {
    const eventHandler = () => {
      // Show loading
      this.progressNode.style.display = 'inline';

      idbMojoFunc.bind(internalsRemote)(this.idbBucketId)
          .then(this.onLoadComplete.bind(this))
          .catch(errorMsg => console.error(errorMsg));
    };

    const control = this.getNode(`.control${selector}`);
    control.addEventListener('click', eventHandler);
  }

  private onLoadComplete(connectionCount: bigint) {
    this.progressNode.style.display = 'none';
    this.connectionCountNode.innerText = connectionCount.toString();
  }
}

function onStorageKeysReady(partitions: IdbPartitionMetadata[]) {
  const template = jstGetTemplate('indexeddb-list-template');
  getRequiredElement('indexeddb-list').appendChild(template);
  jstProcess(
      new JsEvalContext({
        partitions,
        stringifyMojo,
      }),
      template);
}

customElements.define('indexeddb-bucket', BucketElement);
document.addEventListener('DOMContentLoaded', initialize);
