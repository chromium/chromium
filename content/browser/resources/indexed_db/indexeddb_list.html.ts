// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IdbPartitionMetadata} from './indexed_db_internals.mojom-webui.js';
import {origin as mojoOriginToString, schemefulSite, time} from './mojo_utils.js';

export function getHtml(
    partitions: IdbPartitionMetadata[], originFilter: string) {
  // clang-format off
  return html`
    ${partitions.map(partition => html`
      <div id="indexeddb-partition">
        <div class="indexeddb-summary">
          ${partition.partitionPath.path ? html`
            <span>
              <span>Instances in: </span>
              <span>${partition.partitionPath.path}</span>
            </span>
          ` : html`
            <span>
              <span>Instances: Incognito </span>
            </span>
          `}
        </div>
        ${partition.originList.map(origin => html`
          ${originFilter === '' ||
              mojoOriginToString(origin.origin).includes(originFilter) ? html `
            <div class="indexeddb-origin">
              <span>
                Origin:
                <a class="indexeddb-url"
                    href="${mojoOriginToString(origin.origin)}" target="_blank">
                  ${mojoOriginToString(origin.origin)}
                </a>
              </span>
              ${origin.storageKeys.map(key => html`
                <div class="metadata-list-item">
                  <span>Storage partition - top level site:
                    <span>
                      <a class="indexeddb-url" target="_blank">
                        ${schemefulSite(key.topLevelSite)}
                      </a>
                    </span>
                  </span>
                  <div>
                    <span>Storage key:</span>
                    <span>${key.serializedStorageKey}</span>
                  </div>
                  ${key.buckets.map(bucket => html`
                    <indexeddb-bucket class="metadata-list-item"
                        .idbBucketId="${bucket.bucketLocator.id}">
                      <div>
                        <span>Bucket:</span>
                        <span>${bucket.name}</span>
                      </div>
                      <div class="indexeddb-size">
                        <span>Size:</span>
                        <span>${bucket.size}</span>
                      </div>
                      <div class="indexeddb-last-modified">
                        <span>Last modified:</span>
                        <span>${time(bucket.lastModified)}</span>
                      </div>
                      <div>
                        <span>Open connections:</span>
                        <span class="connection-count">
                          ${bucket.connectionCount}
                        </span>
                      </div>
                      <div class="indexeddb-paths">
                        <span>Paths:</span>
                        ${bucket.paths.map(path => html`
                          <span class="indexeddb-path">
                            <span>${path.path}</span>
                          </span>
                        `)}
                      </div>
                      <div class="controls">
                        <span class="control force-close">Force close</span>
                        <span class="control download">Download</span>
                        <span class="control start-record">
                          Start Recording
                        </span>
                        <span class="control stop-record" hidden>
                          Stop Recording
                        </span>
                        <span class="download-status" style="display: none">
                          Loading...
                        </span>
                      </div>

                      <div class="snapshots" hidden>
                        <div class="message"></div>
                        <div class="controls">
                          <button class="prev">←</button>
                          <input type="range" min="0" max="100" value="50"
                              class="slider">
                          <button class="next">→</button>
                          <span>Snapshot:
                            <span class="current-snapshot"></span> /
                            <span class="total-snapshots"></span>
                            (<span class="snapshot-delta"></span>)
                            </span>
                        </div>
                      </div>
                      <div class="database-view">
                        ${bucket.databases.map(database => html`
                          <indexeddb-database class="metadata-list-item"
                              .clients="${bucket.clients}" .data="${database}">
                          </indexeddb-database>
                        `)}
                      </div>
                    </indexeddb-bucket>
                  `)}
                </div>
              `)}
            </div>
          `: ''}
        `)}
      </div>
    `)}
  `;
  // clang-format on
}
