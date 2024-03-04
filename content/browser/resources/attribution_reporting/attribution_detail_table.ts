// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './attribution_detail_table.html.js';

export type RenderFunc<T> = (e: HTMLElement, data: T) => void;

export interface DataColumn<T> {
  readonly label: string;
  readonly render: RenderFunc<T>;
}

export class AttributionDetailTableElement<T> extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.hidden = true;
  }

  private cols_?: Array<RenderFunc<T>>;

  init(dataCols: Iterable<DataColumn<T>>): void {
    this.cols_ = [];

    const tbody = this.$<HTMLTableSectionElement>('tbody')!;
    for (const col of dataCols) {
      this.cols_.push(col.render);

      const tr = tbody.insertRow();

      const th = document.createElement('th');
      th.scope = 'row';
      th.innerText = col.label;
      tr.append(th);

      tr.insertCell();
    }

    this.$('button')!.addEventListener('click', () => {
      this.update(undefined);
      this.dispatchEvent(new CustomEvent('close'));
    });
  }

  update(data: T|undefined): void {
    if (data === undefined) {
      this.hidden = true;
    } else {
      const trs = this.$<HTMLTableSectionElement>('tbody')!.rows;
      this.cols_!.forEach((render, i) => render(trs[i]!.cells[1]!, data));
      this.hidden = false;
    }
  }
}

customElements.define(
    'attribution-detail-table', AttributionDetailTableElement);
