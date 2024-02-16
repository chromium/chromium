// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export abstract class TableModel<T> {
  readonly rowsChangedListeners: Set<() => void> = new Set();

  abstract getRows(): T[];

  rowCount(): number {
    return this.getRows().length;
  }

  protected notifyRowsChanged(): void {
    this.rowsChangedListeners.forEach(f => f());
  }
}

export class ArrayTableModel<T> extends TableModel<T> {
  private rows_: T[] = [];

  constructor() {
    super();
  }

  override getRows(): T[] {
    return this.rows_;
  }

  setRows(rows: T[]): void {
    this.rows_ = rows;
    this.notifyRowsChanged();
  }

  addRow(row: T): void {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    // TODO(apaseltiner): This should really remove the oldest rather than clear
    // out everything.
    if (this.rows_.length >= 1000) {
      this.rows_ = [];
    }

    this.rows_.push(row);
    this.notifyRowsChanged();
  }

  clear(): void {
    this.setRows([]);
  }
}
