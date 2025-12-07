// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SimpleMappedTypeDataView, SimpleMappedTypeTypeMapper} from './web_ui_ts_test.test-mojom-converters.js';

export class SimpleTypeConverter implements SimpleMappedTypeTypeMapper<string> {
  value(mappedType: string): string {
    return mappedType;
  }

  convert(dataView: SimpleMappedTypeDataView): string {
    return dataView.value;
  }
}
