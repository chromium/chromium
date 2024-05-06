// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './private_aggregation_internals_table.html.js';
import type {TableModel} from './table_model.js';

/**
 * Helper function for setting sort attributes on |th|.
 */
function setSortAttrs(th: HTMLElement, sortDesc: boolean|null) {
  let nextDir;
  if (sortDesc === null) {
    th.ariaSort = 'none';
    nextDir = 'ascending';
  } else if (sortDesc) {
    th.ariaSort = 'descending';
    nextDir = 'ascending';
  } else {
    th.ariaSort = 'ascending';
    nextDir = 'descending';
  }

  th.title = `Sort by ${th.innerText} ${nextDir}`;
  th.ariaLabel = th.title;
}

/**
 * Table abstracts the logic for rendering and sorting a table. The table's
 * columns are supplied by a TableModel supplied to the decorate function. Each
 * Column knows how to render the underlying value of the row type T, and
 * optionally sort rows of type T by that value.
 */
export class PrivateAggregationInternalsTableElement<T> extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private model_: TableModel<T>|null = null;
  private sortDesc_: boolean = false;

  setModel(model: TableModel<T>) {
    this.model_ = model;
    this.sortDesc_ = false;

    const tr = this.getRequiredElement('tr');
    model.cols.forEach((col, idx) => {
      const th = document.createElement('th');
      th.scope = 'col';
      col.renderHeader(th);

      if (col.compare) {
        th.setAttribute('role', 'button');
        setSortAttrs(th, /*sortDesc=*/ null);
        th.addEventListener('click', () => this.changeSortHeader_(idx));
      }

      tr.appendChild(th);
    });

    this.addEmptyStateRow_();
    this.model_.rowsChangedListeners.add(() => this.updateTbody());
  }

  private addEmptyStateRow_() {
    const td = document.createElement('td');
    assert(this.model_);
    td.textContent = this.model_.emptyRowText;
    td.colSpan = this.model_.cols.length;
    const tr = document.createElement('tr');
    tr.appendChild(td);
    const tbody = this.getRequiredElement('tbody');
    tbody.appendChild(tr);
  }

  private changeSortHeader_(idx: number) {
    const ths = this.$all<HTMLElement>('thead th');

    assert(this.model_);
    if (idx === this.model_.sortIdx) {
      this.sortDesc_ = !this.sortDesc_;
    } else {
      this.sortDesc_ = false;
      if (this.model_.sortIdx >= 0) {
        setSortAttrs(ths[this.model_.sortIdx]!, /*descending=*/ null);
      }
    }

    this.model_.sortIdx = idx;
    setSortAttrs(ths[this.model_.sortIdx]!, this.sortDesc_);
    this.updateTbody();
  }

  private sort_(rows: T[]) {
    assert(this.model_);
    if (this.model_.sortIdx < 0) {
      return;
    }

    const multiplier = this.sortDesc_ ? -1 : 1;
    rows.sort(
        (a, b) => this.model_!.cols[this.model_!.sortIdx]!.compare!(a, b) *
            multiplier);
  }

  updateTbody() {
    const tbody = this.getRequiredElement('tbody');
    tbody.innerText = '';

    assert(this.model_);
    const rows = this.model_.getRows();
    if (rows.length === 0) {
      this.addEmptyStateRow_();
      return;
    }

    this.sort_(rows);

    rows.forEach((row) => {
      const tr = document.createElement('tr');
      assert(this.model_);
      this.model_.cols.forEach((col) => {
        const td = document.createElement('td');
        col.render(td, row);
        tr.appendChild(td);
      });
      this.model_.styleRow(tr, row);
      tbody.appendChild(tr);
    });
  }
}

customElements.define(
    'aggregation-service-internals-table',
    PrivateAggregationInternalsTableElement);
