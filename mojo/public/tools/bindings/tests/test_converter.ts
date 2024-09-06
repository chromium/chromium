// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { SimpleStructDataView, SimpleStructTypeMapper } from './test.test-mojom-converters.js';
import type {
  String16
} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';


export class SimpleStructConverter implements SimpleStructTypeMapper<String16> {
  str(_: String16): string {
    return '';
  }

  number(_: String16): number {
    return 888;
  }

  convert(_: SimpleStructDataView): String16 {
    return {data: []};
  }
}

