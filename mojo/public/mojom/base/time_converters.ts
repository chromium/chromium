// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {JSTimeDataView, JSTimeTypeMapper} from './time.mojom-converters.js';

export class JsTimeConverter implements JSTimeTypeMapper<Date> {
  // Encoding
  msec(date: Date): number {
    return date.valueOf();
  }

  // Decoding
  convert(view: JSTimeDataView): Date {
    return new Date(view.msec());
  }
}
