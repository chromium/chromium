// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sets navigator.globalPrivacyControl to true in order to match
 * the user's Universal Opt-Out preference.
 */

// Define globalPrivacyControl on Navigator.prototype according to the
// Global Privacy Control preference.
if (typeof Navigator !== 'undefined' && Navigator.prototype) {
  try {
    Object.defineProperty(Navigator.prototype, 'globalPrivacyControl', {
      get: () => true,
      enumerable: true,
      configurable: true,
    });
  } catch {
    // Ignore error if property cannot be defined.
  }
}

// Define on WorkerNavigator.prototype if in a worker context.
const workerNavigator = (globalThis as any).WorkerNavigator;
if (typeof workerNavigator !== 'undefined' && workerNavigator.prototype) {
  try {
    Object.defineProperty(workerNavigator.prototype, 'globalPrivacyControl', {
      get: () => true,
      enumerable: true,
      configurable: true,
    });
  } catch {
    // Ignore error if property cannot be defined.
  }
}

// Fallback: define directly on navigator if Navigator.prototype was
// unavailable.
if (typeof navigator !== 'undefined' &&
    !('globalPrivacyControl' in navigator)) {
  try {
    Object.defineProperty(navigator, 'globalPrivacyControl', {
      get: () => true,
      enumerable: true,
      configurable: true,
    });
  } catch {
    // Ignore error if property cannot be defined.
  }
}
