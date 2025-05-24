// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {UnguessableTokenDataView, UnguessableTokenTypeMapper} from './unguessable_token.mojom-converters.js';

const HEX_BASE: number = 16;
// Token component is 64 bits, hex is 4 bits. This gives us a necessary
// string length.
const TOKEN_COMPONENT_STR_LENGTH: number = 64 / 4;

export class UnguessableTokenConverter implements
    UnguessableTokenTypeMapper<string> {
  // Field accessors for encoding a typemap to a mojo type.
  private validate(token: string) {
    if (token.length !== (2 * TOKEN_COMPONENT_STR_LENGTH)) {
      throw new Error('token is malformed: ' + token);
    }
    if (token !== token.toUpperCase()) {
      throw new Error('token is not uppercase: ' + token);
    }
  }

  high(token: string): bigint {
    this.validate(token);
    return BigInt(`0x${token.slice(0, TOKEN_COMPONENT_STR_LENGTH)}`);
  }

  low(token: string): bigint {
    this.validate(token);
    return BigInt(`0x${token.slice(TOKEN_COMPONENT_STR_LENGTH)}`);
  }

  // Converts a mojo type to the typemap.
  convert(view: UnguessableTokenDataView): string {
    return (view.high.toString(HEX_BASE).padStart(
                TOKEN_COMPONENT_STR_LENGTH, '0') +
            view.low.toString(HEX_BASE).padStart(
                TOKEN_COMPONENT_STR_LENGTH, '0'))
        .toUpperCase();
  }
}
