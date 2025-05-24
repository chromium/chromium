// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {JsErrorDataView, JsErrorTypeMapper} from './jserror.mojom-converters.js';

export class JsErrorConverter implements JsErrorTypeMapper<Object> {
  // Encoding
  name(e: Object): (string|null) {
    return (e as any).name || null;
  }

  cause(e: Object): (string|null) {
    return (e as any).cause || null;
  }

  message(e: Object): (string|null) {
    return (e as any).message || 'unknown error has occured';
  }

  // Decoding
  convert(view: JsErrorDataView): Object {
    const error = new Error(view.message || '', {
      cause: view.cause,
    });
    if (view.name) {
      error.name = view.name;
    }
    return error;
  }
}
