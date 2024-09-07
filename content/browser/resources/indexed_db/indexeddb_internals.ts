// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';
import './database.js';

import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import type {BucketId} from './bucket_id.mojom-webui.js';
import type {IndexedDbDatabase} from './database.js';
import type {IdbInternalsHandlerInterface, IdbPartitionMetadata} from './indexed_db_internals.mojom-webui.js';
import {IdbInternalsHandler} from './indexed_db_internals.mojom-webui.js';
import type {IdbBucketMetadata} from './indexed_db_internals_types.mojom-webui.js';
import type {SchemefulSite} from './schemeful_site.mojom-webui.js';

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
    return mojoString16ToString(mojoString16);
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
};

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
  stopMetadataRecording(bucketId: BucketId): Promise<IdbBucketMetadata[]> {
    return promisifyMojoResult(
        this.handler.stopMetadataRecording(bucketId), 'metadata');
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
  seriesCurrentSnapshotIndex: number|null;
  seriesData: IdbBucketMetadata[]|null;

  constructor() {
    super();
    this.getNode(`.control.download`).addEventListener('click', () => {
      // Show loading
      this.progressNode.style.display = 'inline';

      IdbInternalsHandler.getRemote()
          .downloadBucketData(this.idbBucketId)
          .then(this.onLoadComplete.bind(this))
          .catch(errorMsg => console.error(errorMsg));
    });

    this.getNode(`.control.force-close`).addEventListener('click', () => {
      // Show loading
      this.progressNode.style.display = 'inline';

      IdbInternalsHandler.getRemote()
          .forceClose(this.idbBucketId)
          .then(this.onLoadComplete.bind(this))
          .catch(errorMsg => console.error(errorMsg));
    });

    this.getNode(`.control.start-record`).addEventListener('click', () => {
      this.getNode(`.control.stop-record`)!.hidden = false;
      this.getNode(`.control.start-record`)!.hidden = true;

      IdbInternalsHandler.getRemote()
          .startMetadataRecording(this.idbBucketId)
          .then(this.onLoadComplete.bind(this))
          .catch(errorMsg => console.error(errorMsg));
      if (!this.getNode('.snapshots').hidden) {
        this.getNode('.snapshots').hidden = true;
        this.setRecordingSnapshot(null);
      }
    });
    this.getNode(`.control.stop-record`).addEventListener('click', () => {
      // Show loading
      this.progressNode.style.display = 'inline';
      this.getNode(`.control.start-record`)!.hidden = false;
      this.getNode(`.control.stop-record`)!.hidden = true;

      new IdbInternalsRemote()
          .stopMetadataRecording(this.idbBucketId)
          .then(this.onMetadataRecordingReady.bind(this))
          .catch(errorMsg => console.error(errorMsg));
    });

    this.getNode('.snapshots input.slider')
        .addEventListener('input', (event: Event) => {
          const input = event.target as HTMLInputElement;
          this.setRecordingSnapshot(parseInt(input.value));
        });
    this.getNode('.snapshots .prev').addEventListener('click', () => {
      this.setRecordingSnapshot((this.seriesCurrentSnapshotIndex || 0) - 1);
    });
    this.getNode('.snapshots .next').addEventListener('click', () => {
      this.setRecordingSnapshot((this.seriesCurrentSnapshotIndex || 0) + 1);
    });

    this.progressNode = this.getNode('.download-status');
    this.connectionCountNode = this.getNode('.connection-count');
  }

  private setRecordingSnapshot(idx: number | null) {
    this.getNode('.database-view').textContent = '';
    if (!this.seriesData || idx === null ||
      (idx < 0 || idx > this.seriesData.length - 1)) {
      return;
    }
    const slider =
      this.getNode<HTMLInputElement>('.snapshots input.slider');
    this.seriesCurrentSnapshotIndex = idx;
    const snapshot = this.seriesData[this.seriesCurrentSnapshotIndex];
    if (snapshot === undefined) {
      return;
    }
    slider.value = idx.toString();
    slider.max = (this.seriesData.length - 1).toString();
    this.getNode('.snapshots .current-snapshot')!.textContent =
        slider.value;
    this.getNode('.snapshots .total-snapshots')!.textContent = slider.max;
    this.getNode('.snapshots .snapshot-delta')!.textContent =
        `+${snapshot.deltaRecordingStartMs}ms`;

    for (const db of snapshot.databases || []) {
      const dbView = document.createElement('indexeddb-database');
      const dbElement = this.getNode('.database-view').appendChild(dbView) as
          IndexedDbDatabase;
      dbElement.clients = snapshot.clients;
      dbElement.data = db;
    }
  }

  private getNode<T extends HTMLElement>(selector: string) {
    const controlNode = this.querySelector<T>(`${selector}`);
    assert(controlNode);
    return controlNode;
  }

  private onLoadComplete() {
    this.progressNode.style.display = 'none';
    this.connectionCountNode.innerText = '0';
  }

  private onMetadataRecordingReady(metadata: IdbBucketMetadata[]) {
    this.seriesData = metadata;
    this.onLoadComplete();
    this.getNode('.snapshots').hidden = false;
    this.getNode('.snapshots .controls').hidden = metadata.length === 0;
    if (metadata.length === 0) {
      this.setRecordingSnapshot(null);
      this.getNode('.snapshots .message').innerText =
          'No snapshots were captured.';
      return;
    }
    this.getNode('.snapshots .message').innerText = '';
    this.setRecordingSnapshot(0);
  }
}

function onStorageKeysReady(partitions: IdbPartitionMetadata[]) {
  const template = jstGetTemplate('indexeddb-list-template');
  getRequiredElement('indexeddb-list').appendChild(template);
  const currentOriginFilter = () => window.location.hash.replace('#', '');
  const processTemplate = () => jstProcess(
      new JsEvalContext({
        partitions,
        stringifyMojo,
        originFilter: currentOriginFilter(),
      }),
      template);
  processTemplate();

  // Re process the template when the origin filter is updated.
  const originFilterInput =
      document
      .querySelector<HTMLInputElement>('#origin-filter')!;
      originFilterInput.value = currentOriginFilter();
  originFilterInput.addEventListener('input', (event: Event) => {
    const input = event.target as HTMLInputElement;
    window.location.hash = input.value;
    processTemplate();
  });
}

customElements.define('indexeddb-bucket', BucketElement);
document.addEventListener('DOMContentLoaded', initialize);
