// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './info_view_table_row.html.js';

export class InfoViewTableRowElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    this.data = null;
  }

  setData(data) {
    const isArray = data.value instanceof Array;
    this.toggleAttribute('is-array', isArray);
    if (!isArray) {
      this.shadowRoot.querySelector('.row-title').textContent =
          data.description;
      this.shadowRoot.querySelector('#value > span').textContent = data.value;
      this.shadowRoot.querySelector('#value > span').id = data.id;
    } else {
      const array = this.shadowRoot.querySelector('#array');
      array.querySelector('span').textContent = data.description;
      data.value.forEach(value => {
        const row = document.createElement('info-view-table-row');
        row.setData(value);
        array.appendChild(row);
      });
    }
  }
}

customElements.define('info-view-table-row', InfoViewTableRowElement);
