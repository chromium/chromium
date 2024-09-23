// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './attribution_internals_table.html.js';

export type CompareFunc<T> = (a: T, b: T) => number;

function reverse<T>(f: CompareFunc<T>): CompareFunc<T> {
  return (a, b) => f(b, a);
}

export type RenderFunc<T> = (e: HTMLElement, data: T) => void;

export interface DataColumn<T> {
  readonly label: string;
  readonly render: RenderFunc<T>;
  readonly compare?: CompareFunc<T>;
  readonly defaultSort?: boolean;
}

export type GetIdFunc<T> = (data: T, updated: boolean) => bigint|undefined;

export interface InitOpts<T> {
  readonly getId?: GetIdFunc<T>;
  readonly isSelectable?: boolean;
}

export class AttributionInternalsTableElement<T> extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private cols_?: Array<RenderFunc<T>>;
  private compare_?: CompareFunc<T>;
  private getId_?: GetIdFunc<T>;
  private styleNewRow_?: (tr: DataRowElement<T>) => void;

  init(dataCols: Iterable<DataColumn<T>>, {
    getId,
    isSelectable,
  }: InitOpts<T> = {}): void {
    this.cols_ = [];
    this.getId_ = getId;

    const tr = this.getRequiredElement('thead > tr');
    tr.addEventListener('click', e => this.onSortButtonClick_(e));

    const addTh = (content: Node|string, render: RenderFunc<T>) => {
      const th = document.createElement('th');
      th.scope = 'col';
      th.append(content);
      tr.append(th);
      this.cols_!.push(render);
      return th;
    };

    if (isSelectable) {
      const tbody = this.getRequiredElement('tbody');
      tbody.addEventListener('click', e => this.onTbodyClick(e));
      tbody.addEventListener('keydown', e => {
        if (e.code === 'Enter' || e.code === 'Space') {
          this.onTbodyClick(e);
        }
      });

      this.addEventListener(
          'rows-change',
          () => this.dispatchSelectionChange_(this.selectedData()));

      this.styleNewRow_ = tr => {
        tr.ariaSelected = 'false';
        tr.tabIndex = 0;
      };
    }

    for (const col of dataCols) {
      if (col.compare) {
        const button = new SortButtonElement(col.compare);
        button.innerText = col.label;
        button.title = `Sort by ${col.label} ascending`;

        addTh(button, col.render).ariaSort = 'none';

        if (col.defaultSort) {
          button.click();
        }
      } else {
        addTh(col.label, col.render);
      }
    }

    this.dispatchRowsChange_();
  }

  private onSortButtonClick_(e: Event): void {
    if (!(e.target instanceof SortButtonElement)) {
      return;
    }

    // Matches `ascending` and `descending` but not `none` or missing.
    const currentButton =
        this.$<HTMLElement>('thead > tr > th[aria-sort$="g"] > button');
    if (currentButton && currentButton !== e.target) {
      currentButton.title = `Sort by ${currentButton.innerText} ascending`;
      currentButton.parentElement!.ariaSort = 'none';
    }

    const th = e.target.parentElement! as HTMLElement;
    if (th.ariaSort === 'ascending') {
      th.ariaSort = 'descending';
      e.target.title = `Sort by ${e.target.innerText} ascending`;
      this.setCompare_(reverse(e.target.compare));
    } else {
      th.ariaSort = 'ascending';
      e.target.title = `Sort by ${e.target.innerText} descending`;
      this.setCompare_(e.target.compare);
    }
  }

  private rowCount_(): number {
    return this.getRequiredElement('tbody').rows.length;
  }

  private dispatchRowsChange_(): void {
    const td = this.getRequiredElement<HTMLTableCellElement>('tfoot td');
    td.colSpan = this.cols_!.length - 1;

    const rowCount = this.rowCount_();
    td.innerText = `${rowCount}`;

    this.dispatchEvent(new CustomEvent('rows-change', {
      bubbles: true,
      detail: {rowCount},
    }));
  }

  private setCompare_(f: CompareFunc<T>): void {
    this.compare_ = f;

    const tbody = this.$('tbody')!;
    Array.from(this.dataRows_())
        .sort((a, b) => f(a.data, b.data))
        .forEach(tr => tbody.append(tr));
  }

  private dataRows_(): NodeListOf<DataRowElement<T>> {
    return this.$all('tbody > tr');
  }

  private newRow_(data: T): DataRowElement<T> {
    const tr = new DataRowElement(data);
    for (const render of this.cols_!) {
      render(tr.insertCell(), data);
    }
    if (this.styleNewRow_) {
      this.styleNewRow_(tr);
    }
    return tr;
  }

  addRow(data: T): void {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    // TODO(apaseltiner): This should really remove the oldest rather than clear
    // out everything.
    if (this.rowCount_() >= 1000) {
      this.clearRows();
    }

    let tr: DataRowElement<T>|undefined;

    const id = this.getId_ ? this.getId_(data, /*updated=*/ true) : undefined;
    if (id !== undefined) {
      tr = Array.prototype.find.call(
          this.dataRows_(),
          tr => id === this.getId_!(tr.data, /*updated=*/ false));

      if (tr !== undefined) {
        tr.data = data;
        this.cols_!.forEach((render, idx) => render(tr!.cells[idx]!, data));
      }
    }

    if (tr === undefined) {
      tr = this.newRow_(data);
    }

    let nextTr: DataRowElement<T>|undefined;
    if (this.compare_) {
      // TODO(apaseltiner): Use binary search.
      nextTr = Array.prototype.find.call(
          this.dataRows_(), tr => this.compare_!(tr.data, data) > 0);
    }

    if (nextTr) {
      nextTr.before(tr);
    } else {
      this.$('tbody')!.append(tr);
    }

    this.dispatchRowsChange_();
  }

  updateRows(updatedDatas: Iterable<T>): void {
    const updatedDatasById = new Map<bigint, T>();
    const trs: Array<DataRowElement<T>> = [];

    for (const data of updatedDatas) {
      const id = this.getId_!(data, /*updated=*/ true);
      if (id === undefined) {
        trs.push(this.newRow_(data));
      } else {
        updatedDatasById.set(id, data);
      }
    }

    for (const tr of this.dataRows_()) {
      const id = this.getId_!(tr.data, /*updated=*/ false);
      if (id === undefined) {
        trs.push(tr);
      } else {
        const updatedData = updatedDatasById.get(id);
        if (updatedData === undefined) {
          tr.remove();
        } else {
          updatedDatasById.delete(id);
          tr.data = updatedData;
          this.cols_!.forEach(
              (render, idx) => render(tr.cells[idx]!, updatedData));
          trs.push(tr);
        }
      }
    }

    for (const data of updatedDatasById.values()) {
      trs.push(this.newRow_(data));
    }

    if (this.compare_) {
      trs.sort((a, b) => this.compare_!(a.data, b.data));
    }

    const tbody = this.$('tbody')!;
    for (const tr of trs) {
      tbody.append(tr);
    }

    this.dispatchRowsChange_();
  }

  clearRows(shouldDelete?: (data: T) => boolean): void {
    if (shouldDelete) {
      for (const tr of this.dataRows_()) {
        if (shouldDelete(tr.data)) {
          tr.remove();
        }
      }
    } else {
      this.$('tbody')!.replaceChildren();
    }
    this.dispatchRowsChange_();
  }

  private selectedRow_(): DataRowElement<T>|null {
    return this.$('tbody > tr[aria-selected="true"]');
  }

  selectedData(): T|undefined {
    return this.selectedRow_()?.data;
  }

  clearSelection(): void {
    const tr = this.selectedRow_();
    if (tr) {
      tr.ariaSelected = 'false';
      this.dispatchSelectionChange_(undefined);
    }
  }

  private dispatchSelectionChange_(data: T|undefined): void {
    this.dispatchEvent(new CustomEvent('selection-change', {detail: {data}}));
  }

  private onTbodyClick(e: Event): void {
    if (!(e.target instanceof HTMLElement) ||
        e.target instanceof HTMLAnchorElement) {
      return;
    }
    const tr = e.target.closest('tr');
    const selectedTr = this.selectedRow_();
    if (!(tr instanceof DataRowElement) || tr === selectedTr) {
      return;
    }
    if (selectedTr) {
      selectedTr.ariaSelected = 'false';
    }
    tr.ariaSelected = 'true';
    this.dispatchSelectionChange_(tr.data);
  }
}

customElements.define(
    'attribution-internals-table', AttributionInternalsTableElement);

class DataRowElement<T> extends HTMLTableRowElement {
  constructor(public data: T) {
    super();
  }
}

customElements.define(
    'attribution-internals-data-row', DataRowElement, {extends: 'tr'});

class SortButtonElement<T> extends HTMLButtonElement {
  constructor(readonly compare: CompareFunc<T>) {
    super();
  }
}

customElements.define(
    'attribution-internals-sort-button', SortButtonElement,
    {extends: 'button'});

declare global {
  interface HTMLElementEventMap {
    'rows-change': CustomEvent<{rowCount: number}>;
    'selection-change': CustomEvent<{data: any | undefined}>;
  }
}
