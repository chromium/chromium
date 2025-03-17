// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MappedOptionalContainer, StringDictType, TestNode} from './web_ui_mojo_ts_test_mapped_types.js';
import {MappedDictType} from './web_ui_mojo_ts_test_other_mapped_types.js';
import {NestedMappedTypeDataView, NestedMappedTypeTypeMapper, OptionalTypemapDataView, OptionalTypemapTypeMapper, StringDictDataView, StringDictTypeMapper} from './web_ui_ts_test.test-mojom-converters.js';

export class NestedTypeConverter implements
    NestedMappedTypeTypeMapper<TestNode> {
  nested(mappedType: TestNode): TestNode|null {
    return mappedType.next;
  }

  convert(dataView: NestedMappedTypeDataView): TestNode {
    return new TestNode(dataView.nested);
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
    return view.data;
  }
}

export class OptionalTypemapConverter implements
    OptionalTypemapTypeMapper<MappedOptionalContainer> {
  optionalInt(mapped: MappedOptionalContainer): (number|null) {
    return mapped.optionalInt;
  }

  bools(mapped: MappedOptionalContainer): Array<(boolean | null)> {
    return mapped.bools;
  }

  optionalMap(mapped: MappedOptionalContainer):
      {[key: string]: (boolean|null)} {
    return mapped.optionalMap;
  }

  convert(view: OptionalTypemapDataView): MappedOptionalContainer {
    return {
      optionalInt: view.optionalInt,
      bools: view.bools,
      optionalMap: view.optionalMap,
    };
  }
}
