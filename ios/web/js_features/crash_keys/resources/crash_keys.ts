// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an API for managing crash keys attached to reports.
 * * The data is stored on the `document` using a Global Symbol to ensure shared
 * state across bundles while remaining hidden from property enumeration.
 */

const CRASH_KEYS_MAP_SYMBOL = Symbol.for('__gCrJSCrashKeysMap');

declare global {
  interface Document {
    [CRASH_KEYS_MAP_SYMBOL]?: Map<string, string>;
  }
}

/**
 * Returns the internal map, initializing it on the document if necessary.
 */
function getInternalMap(): Map<string, string> {
  if (!document[CRASH_KEYS_MAP_SYMBOL]) {
    document[CRASH_KEYS_MAP_SYMBOL] = new Map<string, string>();
  }
  return document[CRASH_KEYS_MAP_SYMBOL];
}

/**
 * Returns the internal map of crash keys as an object so it can be passed
 * across the JS->native bridge.
 */
export function getCrashKeys(): Object {
  return Object.fromEntries(getInternalMap());
}

/**
 * Sets a crash key. Both name and value must be non-empty.
 * @param name Unique identifier for the key.
 * @param value Value to associate with the key.
 */
export function setCrashKey(name: string, value: string): void {
  if (name.length > 0 && value.length > 0) {
    getInternalMap().set(name, value);
  }
}

/**
 * Clears a specific crash key by name.
 */
export function clearCrashKey(name: string): void {
  getInternalMap().delete(name);
}

/**
 * Clears all crash keys currently stored.
 */
export function clearAllCrashKeys(): void {
  getInternalMap().clear();
}
