// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ImportExportFileProxy {
  downloadFile(a: HTMLAnchorElement): void;
  selectFile(input: HTMLInputElement): void;
  copyToClipboard(text: string): Promise<void>;
  readFromClipboard(): Promise<string>;
}

export class ImportExportFileProxyImpl implements ImportExportFileProxy {
  downloadFile(a: HTMLAnchorElement) {
    a.click();
  }

  selectFile(input: HTMLInputElement) {
    input.click();
  }

  copyToClipboard(text: string): Promise<void> {
    return navigator.clipboard.writeText(text);
  }

  readFromClipboard(): Promise<string> {
    return navigator.clipboard.readText();
  }

  static getInstance(): ImportExportFileProxy {
    return instance || (instance = new ImportExportFileProxyImpl());
  }

  static setInstance(obj: ImportExportFileProxy) {
    instance = obj;
  }
}

let instance: ImportExportFileProxy|null = null;
