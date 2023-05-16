// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './info_view_table_row.html.js';

export interface Data {
  description: string;
  id?: string;
  value: string;
}

export interface ArrayData {
  description: string;
  value: Data[];
}

export class InfoViewTableRowElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  setData(data: Data|ArrayData) {
    const isArray = data.value instanceof Array;
    this.toggleAttribute('is-array', isArray);
    if (!isArray) {
      this.shadowRoot!.querySelector('.row-title')!.textContent =
          data.description;
      this.shadowRoot!.querySelector('#value > span')!.textContent =
          (data as Data).value;
      this.shadowRoot!.querySelector('#value > span')!.id =
          String((data as Data).id);
    } else {
      const array = this.shadowRoot!.querySelector('#array')!;
      array.querySelector('span')!.textContent = data.description;
      (data as ArrayData).value.forEach(value => {
        const row = document.createElement('info-view-table-row');
        row.setData(value);
        array.appendChild(row);
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'info-view-table-row': InfoViewTableRowElement;
  }
}

customElements.define('info-view-table-row', InfoViewTableRowElement);
