/* Copyright 2025 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {assert} from '//resources/js/assert.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {Token} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

export function getTokenAsUuidString(uuid: Token): string {
  const highHex = uuid.high.toString(16).padStart(16, '0');
  const lowHex = uuid.low.toString(16).padStart(16, '0');
  return `${lowHex.slice(0, 8)}-${lowHex.slice(8, 12)}-${
      lowHex.slice(12, 16)}-${highHex.slice(0, 4)}-${highHex.slice(4)}`;
}

function getArrayFromBigBuffer(bigBuffer: BigBuffer): Uint8Array<ArrayBuffer> {
  if (Array.isArray(bigBuffer.bytes)) {
    return new Uint8Array(bigBuffer.bytes);
  }
  assert(!!bigBuffer.sharedMemory, 'sharedMemory must be defined here');
  const sharedMemory = bigBuffer.sharedMemory;
  const {buffer, result} =
      sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
  assert(result === Mojo.RESULT_OK, 'Could not map buffer');
  return new Uint8Array(buffer);
}

// Create the temporary element here to hold the data to download the trace
// since it is only obtained after downloadData_ is called. This way we can
// perform a download directly in JS without touching the element that
// triggers the action. Initiate download a resource identified by |url| into
// |filename|.
function downloadUrl(fileName: string, url: string): void {
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
}

export function downloadTraceData(data: BigBuffer, uuid: Token): void {
  const bytes = getArrayFromBigBuffer(data);
  const blob = new Blob([bytes], {
    type: 'application/octet-stream',
  });
  downloadUrl(`${getTokenAsUuidString(uuid)}.gz`, URL.createObjectURL(blob));
}
