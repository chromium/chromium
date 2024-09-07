// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './transaction_table.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import type {BucketClientInfo} from './bucket_client_info.mojom-webui.js';
import {getTemplate} from './database.html.js';
import {IdbInternalsHandler} from './indexed_db_internals.mojom-webui.js';
import type {IdbDatabaseMetadata, IdbTransactionMetadata} from './indexed_db_internals_types.mojom-webui.js';
import type {ExecutionContextToken} from './tokens.mojom-webui.js';
import type {IndexedDbTransactionTable} from './transaction_table.js';

export class IndexedDbDatabase extends CustomElement {
  clients: BucketClientInfo[];

  static override get template() {
    return getTemplate();
  }

  // Similar to CustomElement.$, but asserts that the element exists.
  $a<T extends HTMLElement = HTMLElement>(query: string): T {
    return this.getRequiredElement<T>(query);
  }

  // Setter for `data` property. Updates the component contents with the
  // provided metadata.
  set data(metadata: IdbDatabaseMetadata) {
    const openDatabasesElement = this.$a('.open-databases');
    const openConnectionElement = this.$a('.connection-count.open');
    const activeConnectionElement = this.$a('.connection-count.active');
    const pendingConnectionElement = this.$a('.connection-count.pending');

    openDatabasesElement.textContent = mojoString16ToString(metadata.name);

    openConnectionElement.hidden = metadata.connectionCount === 0n;
    openConnectionElement.querySelector('.value')!.textContent =
        metadata.connectionCount.toString();

    activeConnectionElement.hidden = metadata.activeOpenDelete === 0n;
    activeConnectionElement.querySelector('.value')!.textContent =
        metadata.activeOpenDelete.toString();

    pendingConnectionElement.hidden = metadata.pendingOpenDelete === 0n;
    pendingConnectionElement.querySelector('.value')!.textContent =
        metadata.pendingOpenDelete.toString();

    this.groupAndShowTransactionsByClient(metadata.transactions);
  }

  private groupAndShowTransactionsByClient(transactions:
                                               IdbTransactionMetadata[]) {
    const groupedTransactions = new Map<string, IdbTransactionMetadata[]>();
    for (const transaction of transactions) {
      const client = transaction.clientToken;
      if (!groupedTransactions.has(client)) {
        groupedTransactions.set(client, []);
      }
      groupedTransactions.get(client)!.push(transaction);
    }

    const transactionsBlockElement = this.$a('#transactions');
    transactionsBlockElement.textContent = '';
    for (const [clientToken, clientTransactions] of groupedTransactions) {
      const container = this.createClientTransactionsContainer(
          clientToken, clientTransactions);
      container.classList.add('metadata-list-item');
      transactionsBlockElement.appendChild(container);
    }
  }

  // Creates a div containing an instantiation of the client metadata template
  // and a table of transactions.
  private createClientTransactionsContainer(
      clientToken: string,
      transactions: IdbTransactionMetadata[]): HTMLElement {
    const clientMetadataTemplate =
        this.$a<HTMLTemplateElement>('#client-metadata');
    const clientMetadata =
        clientMetadataTemplate.content.cloneNode(true) as DocumentFragment;
    clientMetadata.querySelector('.client-id')!.textContent = clientToken;
    clientMetadata.querySelector('.control.inspect')!.addEventListener(
        'click', () => {
          // If there are non-zero clients, inspecting any of them should have
          // the same effect.
          const client = this.getClientsMatchingToken(clientToken).pop();
          if (!client) {
            return;
          }
          IdbInternalsHandler.getRemote()
              .inspectClient(client)
              .then(message => {
                if (message.error) {
                  console.error(message.error);
                }
              })
              .catch(errorMsg => console.error(errorMsg));
        });
    const transactionTable =
        document.createElement('indexeddb-transaction-table') as
        IndexedDbTransactionTable;
    transactionTable.transactions = transactions;
    const container = document.createElement('div');
    container.appendChild(clientMetadata);
    container.appendChild(transactionTable);
    return container;
  }

  // Returns the list of clients whose `documentToken` or `contextToken` matches
  // the supplied `token`.
  private getClientsMatchingToken(token: string): BucketClientInfo[] {
    const matchedClients: BucketClientInfo[] = [];
    for (const client of this.clients) {
      const tokenValue = client.documentToken ?
          client.documentToken.value :
          this.getExecutionContextTokenValue(client.contextToken);
      if (tokenValue && this.tokenToHexString(tokenValue) === token) {
        matchedClients.push(client);
      }
    }
    return matchedClients;
  }

  private getExecutionContextTokenValue(token: ExecutionContextToken):
      UnguessableToken {
    if (token.localFrameToken) {
      return token.localFrameToken.value;
    }
    if (token.dedicatedWorkerToken) {
      return token.dedicatedWorkerToken.value;
    }
    if (token.serviceWorkerToken) {
      return token.serviceWorkerToken.value;
    }
    if (token.sharedWorkerToken) {
      return token.sharedWorkerToken.value;
    }
    throw new Error('Unrecognized ExecutionContextToken');
  }

  // This is the equivalent of `base::UnguessableToken::ToString()`.
  private tokenToHexString(token: UnguessableToken) {
    // Return the concatenation of the upper-case hexadecimal representations
    // of high and low, both padded to be 16 characters long.
    return token.high.toString(16).padStart(16, '0').toUpperCase() +
        token.low.toString(16).padStart(16, '0').toUpperCase();
  }
}

customElements.define('indexeddb-database', IndexedDbDatabase);
