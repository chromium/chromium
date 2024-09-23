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

  init(cols: Iterable<string|DataColumn<T>>): void {
    this.cols_ = [];

    const tbody = this.getRequiredElement('tbody');
    for (const col of cols) {
      const tr = tbody.insertRow();
      const th = document.createElement('th');
      tr.append(th);

      if (typeof col === 'string') {
        th.scope = 'col';
        th.colSpan = 2;

        const span = document.createElement('span');
        span.innerText = col;
        th.append(span);

        tbody.classList.add('sectioned');
      } else {
        th.scope = 'row';
        th.innerText = col.label;
        tr.insertCell();
        this.cols_.push(col.render);
      }
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
      const tds = this.$all<HTMLElement>('tbody > tr > td');
      this.cols_!.forEach((render, i) => render(tds[i]!, data));
      this.hidden = false;
    }
  }
}

customElements.define(
    'attribution-detail-table', AttributionDetailTableElement);
