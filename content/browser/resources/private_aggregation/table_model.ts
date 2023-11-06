// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface Column<T> {
  compare: ((a: T, b: T) => number)|null;

  render(td: HTMLElement, row: T): void;

  renderHeader(th: HTMLElement): void;

  header: string|null;
}

export class TableModel<T> {
  cols: Array<Column<T>> = [];
  emptyRowText: string = '';
  sortIdx: number = -1;
  rowsChangedListeners: Set<() => void>;

  constructor() {
    this.rowsChangedListeners = new Set();
  }

  styleRow(_tr: Element, _data: T) {}

  getRows(): T[] {
    return [];
  }

  notifyRowsChanged() {
    this.rowsChangedListeners.forEach(f => f());
  }
}
