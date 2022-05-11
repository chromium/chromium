// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @template T
 * @abstract
 */
export class Column {
  constructor() {
    /** @type {?function(!T, !T): number} */
    this.compare;
  }

  /**
   * @param {!Element} td
   * @param {!T} row
   * @abstract
   */
  render(td, row) {}

  /**
   * @param {!Element} th
   * @abstract
   */
  renderHeader(th) {}
}

/**
 * @template T
 * @abstract
 */
export class TableModel {
  constructor() {
    /** @type {!Array<Column<T>>} */
    this.cols;

    /** @type {string} */
    this.emptyRowText;

    /** @type {number} */
    this.sortIdx = -1;

    /** @type {!Set<function()>} */
    this.rowsChangedListeners = new Set();
  }

  /**
   * @param {!Element} tr
   * @param {T} data
   */
  styleRow(tr, data) {}

  /**
   * @abstract
   * @return {!Array<!T>}
   */
  getRows() {}

  notifyRowsChanged() {
    this.rowsChangedListeners.forEach(f => f());
  }
}
