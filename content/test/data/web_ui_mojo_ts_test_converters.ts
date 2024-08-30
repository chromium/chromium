// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NestedMappedTypeDataView, NestedMappedTypeTypeMapper, SimpleMappedTypeDataView, SimpleMappedTypeTypeMapper,} from './web_ui_ts_test.test-mojom-converters.js';

export class SimpleTypeConverter implements SimpleMappedTypeTypeMapper<string> {
  value(mappedType: string): string {
    return mappedType;
  }

  convert(dataView: SimpleMappedTypeDataView): string {
    return dataView.value();
  }
}

export class NestedTypeConverter implements NestedMappedTypeTypeMapper<Object> {
  nested(mappedType: any): Object|null {
    return mappedType?.nested || null;
  }

  convert(dataView: NestedMappedTypeDataView): Object {
    return dataView.nested() ? {'nested': dataView.nested()} : {};
  }
}
