// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Converts `dataBase64` from a base-64 encoded string to a Blob.
function base64toBlob(dataBase64: string): Blob {
  const sliceSize = 512;
  const byteCharacters = atob(dataBase64);
  const byteArrays: Uint8Array[] = [];
  for (let offset = 0; offset < byteCharacters.length; offset += sliceSize) {
    const slice = byteCharacters.slice(offset, offset + sliceSize);
    const byteNumbers = new Array(slice.length);
    for (let i = 0; i < slice.length; i++) {
      byteNumbers[i] = slice.charCodeAt(i);
    }
    byteArrays.push(new Uint8Array(byteNumbers));
  }
  return new Blob(byteArrays);
};

// Converts `dataBlob` from a Blob to a base-64 encoded string asynchronously.
function blobToBase64(dataBlob: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    dataBlob.arrayBuffer().then(arrayBuffer => {
      const byteNumbers = new Uint8Array(arrayBuffer);
      let byteCharacters = '';
      for (let i = 0; i < byteNumbers.length; i++) {
        byteCharacters += String.fromCharCode(byteNumbers[i]!);
      }
      resolve(btoa(byteCharacters));
    }, reject);
  });
};

// Decodes the image data `sourceDataBase64` and reencodes it to the selected
// destination type, width and height. If the destination type supports lossy
// compression, then `destinationQuality` is used as the destination quality.
// If `width` or `height` are missing, then the original width or height will
// be used instead. If the destination quality is not set, the default quality
// for the user-agent is used instead.
export function transcodeImage(
    sourceDataBase64: string, destinationType: string,
    destinationWidth: number|undefined,
    destinationHeight: number|undefined,
    destinationQuality: number|undefined): Promise<string> {
  return new Promise((resolve, reject) => {
    const imageElement = document.createElement('img');
    // Handle successful loading.
    imageElement.addEventListener('load', _ => {
      const canvas = document.createElement('canvas');
      canvas.width = destinationWidth ?? imageElement.width;
      canvas.height = destinationHeight ?? imageElement.height;
      const ctx = canvas.getContext('2d')!;
      ctx.drawImage(imageElement, 0, 0, canvas.width, canvas.height);
      canvas.toBlob(dataBlob => {
        blobToBase64(dataBlob!).then(resolve, reject);
      }, destinationType, destinationQuality);
    });
    // Handle failed loading.
    imageElement.addEventListener('error', reject);
    // Start loading image.
    imageElement.src = URL.createObjectURL(base64toBlob(sourceDataBase64));
  });
}
