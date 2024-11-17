// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './database.js';

import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BucketId} from './bucket_id.mojom-webui.js';
import type {IndexedDbDatabase} from './database.js';
import type {IdbInternalsHandlerInterface, IdbPartitionMetadata} from './indexed_db_internals.mojom-webui.js';
import {IdbInternalsHandler} from './indexed_db_internals.mojom-webui.js';
import type {IdbBucketMetadata} from './indexed_db_internals_types.mojom-webui.js';
import {getHtml} from './indexeddb_list.html.js';


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
  const currentOriginFilter = () => window.location.hash.replace('#', '');
  const processTemplate = () => {
    render(
        getHtml(partitions, currentOriginFilter()),
        getRequiredElement('indexeddb-list'));
  };
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
