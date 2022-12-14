// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface Column<T> {
  readonly compare?: (a: T, b: T) => number;

  render(td: HTMLElement, row: T): void;

  renderHeader(th: HTMLElement): void;
}

export abstract class TableModel<T> {
  readonly rowsChangedListeners: Set<() => void> = new Set();

  constructor(
      public readonly cols: Array<Column<T>>,
      public sortIdx: number,
      public readonly emptyRowText: string,
  ) {}

  styleRow(_tr: Element, _data: T) {}

  abstract getRows(): T[];

  notifyRowsChanged() {
    this.rowsChangedListeners.forEach(f => f());
  }
}
