// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

import {Column, TableModel} from './table_model.js';

/**
 * Helper function for setting sort attributes on |th|.
 * @param {!Element} th
 * @param {?boolean} sortDesc
 */
function setSortAttrs(th, sortDesc) {
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
 * @template T
 */
class AttributionInternalsTableElement extends CustomElement {
  static get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!TableModel<T>} */
    this.model;

    /** @private {boolean} */
    this.sortDesc;
  }

  /**
   * @param {!TableModel<T>} model
   */
  setModel(model) {
    this.model = model;
    this.sortDesc = false;

    const tr = this.$('tr');
    model.cols.forEach((col, idx) => {
      const th = document.createElement('th');
      th.scope = 'col';
      col.renderHeader(th);

      if (col.compare) {
        th.role = 'button';
        setSortAttrs(th, /*sortDesc=*/ null);
        th.addEventListener('click', () => this.changeSortHeader(idx));
      }

      tr.appendChild(th);
    });

    this.addSpanningText();
    this.model.rowsChangedListeners.add(() => this.updateTbody());
  }

  addSpanningText() {
    const td = document.createElement('td');
    td.textContent = this.model.emptyRowText;
    td.colSpan = this.model.cols.length;
    const tr = document.createElement('tr');
    tr.appendChild(td);
    this.$('tbody').appendChild(tr);
  }

  /**
   * @param {number} idx
   * @private
   */
  changeSortHeader(idx) {
    const ths = this.$all('thead th');

    if (idx === this.model.sortIdx) {
      this.sortDesc = !this.sortDesc;
    } else {
      this.sortDesc = false;
      if (this.model.sortIdx >= 0) {
        setSortAttrs(ths[this.model.sortIdx], /*descending=*/ null);
      }
    }

    this.model.sortIdx = idx;
    setSortAttrs(ths[this.model.sortIdx], this.sortDesc);
    this.updateTbody();
  }

  /**
   * @param {!Array<T>} rows
   * @private
   */
  sort(rows) {
    if (this.model.sortIdx < 0) {
      return;
    }

    const multiplier = this.sortDesc ? -1 : 1;
    rows.sort(
        (a, b) =>
            this.model.cols[this.model.sortIdx].compare(a, b) * multiplier);
  }

  updateTbody() {
    const tbody = this.$('tbody');
    tbody.innerText = '';

    const rows = this.model.getRows();
    if (rows.length === 0) {
      this.addSpanningText();
      return;
    }

    this.sort(rows);

    rows.forEach((row) => {
      const tr = document.createElement('tr');
      this.model.cols.forEach((col) => {
        const td = document.createElement('td');
        col.render(td, row);
        tr.appendChild(td);
      });
      this.model.styleRow(tr, row);
      tbody.appendChild(tr);
    });
  }
}

customElements.define(
    'attribution-internals-table', AttributionInternalsTableElement);
