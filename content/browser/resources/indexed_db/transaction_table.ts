// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {IdbTransactionMode, IdbTransactionState} from './indexed_db_internals_types.mojom-webui.js';
import type {IdbTransactionMetadata} from './indexed_db_internals_types.mojom-webui.js';
import {getTemplate} from './transaction_table.html.js';

// Joins a list of Mojom strings to a comma separated JS string.
function scope(mojoScope: String16[]): string {
  return `[${mojoScope.map(s => mojoString16ToString(s)).join(', ')}]`;
}

// Converts IdbTransactionState enum into a readable string.
function transactionState(mojoState: IdbTransactionState): string {
  switch (mojoState) {
    case IdbTransactionState.kBlocked:
      return 'Blocked';
    case IdbTransactionState.kRunning:
      return 'Running';
    case IdbTransactionState.kStarted:
      return 'Started';
    case IdbTransactionState.kCommitting:
      return 'Comitting';
    case IdbTransactionState.kFinished:
      return 'Finished';
    default:
      return 'Unknown';
  }
}

// Converts IdbTransactionMode enum into a readable string.
function transactionMode(mojoMode: IdbTransactionMode): string {
  switch (mojoMode) {
    case IdbTransactionMode.kReadOnly:
      return 'ReadOnly';
    case IdbTransactionMode.kReadWrite:
      return 'ReadWrite';
    case IdbTransactionMode.kVersionChange:
      return 'VersionChange';
    default:
      return 'Unknown';
  }
}

export class IndexedDbTransactionTable extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  // Similar to CustomElement.$, but asserts that the element exists.
  $a<T extends HTMLElement = HTMLElement>(query: string): T {
    return this.getRequiredElement<T>(query);
  }

  // Setter for `data` property. Updates the component contents with the
  // provided metadata.
  set transactions(transactions: IdbTransactionMetadata[]) {
    const transactionTableBodyElement = this.$a('.transaction-list tbody');
    const transactionRowTemplateElement =
        this.$a<HTMLTemplateElement>(`#transaction-row`);

    transactionTableBodyElement.textContent = '';
    for (const transaction of transactions) {
      const row = (transactionRowTemplateElement.content.cloneNode(true) as
                   DocumentFragment)
                      .firstElementChild!;
      row.classList.add(transactionState(transaction.state).toLowerCase());
      row.querySelector('td.tid')!.textContent = transaction.tid.toString();
      row.querySelector('td.mode')!.textContent =
          transactionMode(transaction.mode);
      row.querySelector('td.scope')!.textContent = scope(transaction.scope);
      row.querySelector('td.requests-complete')!.textContent =
          transaction.tasksCompleted.toString();
      row.querySelector('td.requests-pending')!.textContent =
          (transaction.tasksScheduled - transaction.tasksCompleted).toString();
      row.querySelector('td.age')!.textContent =
          Math.round(transaction.age).toString();
      if (transaction.state === IdbTransactionState.kStarted ||
          transaction.state === IdbTransactionState.kRunning ||
          transaction.state === IdbTransactionState.kCommitting) {
        row.querySelector('td.runtime')!.textContent =
            Math.round(transaction.runtime).toString();
      }
      row.querySelector('td.state .text')!.textContent =
          transactionState(transaction.state);
      for (const state of transaction.stateHistory) {
        const li = document.createElement('li');
        li.textContent =
            `${transactionState(state.state)}: ${Math.round(state.duration)}ms`;
        row.querySelector('td.state ul')!.appendChild(li);
      }

      transactionTableBodyElement.appendChild(row);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indexeddb-transaction-table': IndexedDbTransactionTable;
  }
}

customElements.define('indexeddb-transaction-table', IndexedDbTransactionTable);
