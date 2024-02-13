// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './attribution_internals_table.html.js';
import type {TableModel} from './table_model.js';

/**
 * Helper function for setting sort attributes on |th|.
 */
function setSortAttrs(th: HTMLElement, sortDesc: boolean|null): void {
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

  const button = th.querySelector('button')!;
  button.title = `Sort by ${button.innerText} ${nextDir}`;
}

export type StyleRowFunc<T> = (tr: HTMLTableRowElement, row: T) => void;

/**
 * Table abstracts the logic for rendering and sorting a table. The table's
 * columns are supplied by a TableModel. Each Column knows how to render the
 * underlying value of the row type T, and optionally sort rows of type T by
 * that value.
 */
export class AttributionInternalsTableElement<T> extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private model_?: TableModel<T>;
  private sortDesc_: boolean = false;
  private styleRow_: StyleRowFunc<T> = () => {};

  setModel(model: TableModel<T>, styleRow: StyleRowFunc<T> = () => {}): void {
    this.model_ = model;
    this.sortDesc_ = false;
    this.styleRow_ = styleRow;

    const tr = this.$<HTMLElement>('thead > tr')!;
    model.cols.forEach((col, idx) => {
      const th = document.createElement('th');
      th.scope = 'col';

      if (col.compare) {
        const button = document.createElement('button');
        col.renderHeader(button);
        th.append(button);
        setSortAttrs(th, idx === model.sortIdx ? this.sortDesc_ : null);
        button.addEventListener('click', () => this.changeSortHeader_(idx));
      } else {
        col.renderHeader(th);
      }

      tr.append(th);
    });

    this.updateRowCount_();

    this.model_.rowsChangedListeners.add(() => {
      this.updateRowCount_();
      this.updateTbody_();
    });
  }

  private updateRowCount_(): void {
    const td = this.$<HTMLTableCellElement>('tfoot td')!;
    td.colSpan = this.model_!.cols.length;
    td.innerText = `Rows: ${this.model_!.rowCount()}`;
  }

  private changeSortHeader_(idx: number): void {
    const ths = this.$all<HTMLElement>('thead > tr > th');

    if (idx === this.model_!.sortIdx) {
      this.sortDesc_ = !this.sortDesc_;
    } else {
      this.sortDesc_ = false;
      if (this.model_!.sortIdx >= 0) {
        setSortAttrs(ths[this.model_!.sortIdx]!, /*descending=*/ null);
      }
    }

    this.model_!.sortIdx = idx;
    setSortAttrs(ths[this.model_!.sortIdx]!, this.sortDesc_);
    this.updateTbody_();
  }

  private sort_(rows: T[]): void {
    if (this.model_!.sortIdx < 0) {
      return;
    }

    const multiplier = this.sortDesc_ ? -1 : 1;
    const sortCol = this.model_!.cols[this.model_!.sortIdx]!;
    rows.sort((a, b) => multiplier * sortCol.compare!(a, b));
  }

  private updateTbody_(): void {
    const tbody = this.$<HTMLElement>('tbody')!;
    tbody.innerText = '';

    const rows = this.model_!.getRows();
    if (rows.length === 0) {
      return;
    }

    this.sort_(rows);

    for (const row of rows) {
      const tr = document.createElement('tr');
      for (const col of this.model_!.cols) {
        const td = document.createElement('td');
        col.render(td, row);
        tr.append(td);
      }
      this.styleRow_(tr, row);
      tbody.append(tr);
    }
  }
}

customElements.define(
    'attribution-internals-table', AttributionInternalsTableElement);
