// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MappedDictType} from './web_ui_mojo_ts_test_other_mapped_types.js';
import {MappedDictDataView, MappedDictTypeMapper} from './web_ui_ts_test_other_types.test-mojom-converters.js';

export class MappedDictConverter implements
    MappedDictTypeMapper<MappedDictType> {
  data(dict: MappedDictType): {[key: string]: string} {
    const converted: {[key: string]: string} = {};
    for (let k of dict.keys()) {
      converted[k] = dict.get(k) || '';
    }
    return converted;
  }

  convert(view: MappedDictDataView): MappedDictType {
    const converted = new Map<string, string>();
    for (let k in view.data) {
      converted.set(k, view.data[k]!);
    }
    return converted;
  }
}

