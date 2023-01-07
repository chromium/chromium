// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file exports `gCrWeb` to be used by other
 * files to augment its functionality. `gCrWeb` is intended
 * to be used has a bridge for native code to access JavaScript functionality.
 * The functions added to `gCrWeb` are not intended to be used within
 * other JavaScript files.
 */

/**
 * This interface is intended to extend the Window object to allow the
 * TypeScript compiler to recognize that the Window has an __gCrWeb object.
 */
declare interface GCRWebInterface {
  __gCrWeb: any
};

declare type GCRWebType =
    Window & typeof globalThis & GCRWebInterface;

// Initializes window's `__gCrWeb` property. Without this step,
// the window's `__gCrWeb` property cannot be found.
if (!(window as GCRWebType).__gCrWeb) {
  (window as GCRWebType).__gCrWeb = {};
}

const gCrWeb: any = (window as GCRWebType).__gCrWeb;

export {gCrWeb};
