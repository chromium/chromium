// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './attribution_internals_table.html.js';
import type {TableModel} from './table_model.js';

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

export type RenderFunc<T> = (e: HTMLElement, data: T) => void;

export interface Column<T> {
  compare?(a: T, b: T): number;

  render(td: HTMLElement, row: T): void;

  renderHeader(th: HTMLElement): void;
}

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
  private cols_?: ReadonlyArray<Column<T>>;
  private sortIdx_: number = -1;
  private sortDesc_: boolean = false;
  private styleRow_: RenderFunc<T> = () => {};

  init(
      model: TableModel<T>,
      cols: ReadonlyArray<Column<T>&{defaultSort?: boolean}>,
      styleRow: RenderFunc<T> = () => {}): void {
    this.model_ = model;
    this.cols_ = cols;
    this.sortIdx_ = -1;
    this.sortDesc_ = false;
    this.styleRow_ = styleRow;

    const tr = this.$<HTMLElement>('thead > tr')!;
    cols.forEach((col, idx) => {
      const th = document.createElement('th');
      th.scope = 'col';

      if (col.compare) {
        const button = document.createElement('button');
        col.renderHeader(button);
        th.append(button);
        setSortAttrs(th, col.defaultSort ? this.sortDesc_ : null);
        button.addEventListener('click', () => this.changeSortHeader_(idx));
        if (col.defaultSort) {
          this.sortIdx_ = idx;
        }
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
    td.colSpan = this.cols_!.length;
    td.innerText = `Rows: ${this.model_!.rowCount()}`;
  }

  private changeSortHeader_(idx: number): void {
    const ths = this.$all<HTMLElement>('thead > tr > th');

    if (idx === this.sortIdx_) {
      this.sortDesc_ = !this.sortDesc_;
    } else {
      this.sortDesc_ = false;
      if (this.sortIdx_ >= 0) {
        setSortAttrs(ths[this.sortIdx_]!, /*descending=*/ null);
      }
    }

    this.sortIdx_ = idx;
    setSortAttrs(ths[this.sortIdx_]!, this.sortDesc_);
    this.updateTbody_();
  }

  private sort_(rows: T[]): void {
    if (this.sortIdx_ < 0) {
      return;
    }

    const multiplier = this.sortDesc_ ? -1 : 1;
    const sortCol = this.cols_![this.sortIdx_]!;
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
      for (const col of this.cols_!) {
        col.render(tr.insertCell(), row);
      }
      this.styleRow_(tr, row);
      tbody.append(tr);
    }
  }
}

customElements.define(
    'attribution-internals-table', AttributionInternalsTableElement);
