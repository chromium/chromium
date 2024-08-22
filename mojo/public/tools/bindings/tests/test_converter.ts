// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { SimpleStructDataView, SimpleStructTypeMapper } from './test.test-mojom-converters.js';

export class SimpleStructConverter implements SimpleStructTypeMapper<string> {
  str(_: string): string {
    return '';
  }

  number(_: string): number {
    return 888;
  }

  convert(_: SimpleStructDataView): string {
    return 'hihi';
  }
}

