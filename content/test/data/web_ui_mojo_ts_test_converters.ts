// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StringDictType, TestNode} from './web_ui_mojo_ts_test_mapped_types.js';
import {MappedDictType} from './web_ui_mojo_ts_test_other_mapped_types.js';
import {NestedMappedTypeDataView, NestedMappedTypeTypeMapper, SimpleMappedTypeDataView, SimpleMappedTypeTypeMapper, StringDictDataView, StringDictTypeMapper} from './web_ui_ts_test.test-mojom-converters.js';
import {MappedDictDataView, MappedDictTypeMapper} from './web_ui_ts_test_other_types.test-mojom-converters.js';

export class SimpleTypeConverter implements SimpleMappedTypeTypeMapper<string> {
  value(mappedType: string): string {
    return mappedType;
  }

  convert(dataView: SimpleMappedTypeDataView): string {
    return dataView.value();
  }
}

export class NestedTypeConverter implements
    NestedMappedTypeTypeMapper<TestNode> {
  nested(mappedType: TestNode): TestNode|null {
    return mappedType.next;
  }

  convert(dataView: NestedMappedTypeDataView): TestNode {
    return new TestNode(dataView.nested());
  }
}

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
    for (let k in view.data()) {
      converted.set(k, view.data()[k]!);
    }
    return converted;
  }
}

// This should be trivial to implement because StringDict is synonymous
// with mapped type. If typemapping is working correctly, we should be
// able to switch back and forth between MappedDictType and StringDictType
// freely.
export class StringDictConverter implements
    StringDictTypeMapper<StringDictType> {
  data(dict: StringDictType): MappedDictType {
    return dict;
  }

  convert(view: StringDictDataView): StringDictType {
    return view.data();
  }
}
