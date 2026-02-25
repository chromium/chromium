// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the web actuation tools.
 */

/**
 * Returns the element at the given coordinates, accounting for pixel type.
 *
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param pixelType The type of pixels (0=UNSPECIFIED, 1=DIPS, 2=PHYSICAL).
 * @return An object containing the target element and the transformed client
 *     coordinates.
 */
export function getElementFromPoint(x: number, y: number, pixelType: number):
    {element: Element|null, clientX: number, clientY: number} {
  // See components/optimization_guide/proto/features/common_quality_data.proto
  // for the definition of PixelType.
  const PixelType = {
    UNSPECIFIED: 0,
    DIPS: 1,
    PHYSICAL: 2,
  };

  // UNSPECIFIED and DIPS are assumed to be viewport coordinates (no change).
  let clientX = x;
  let clientY = y;

  if (pixelType === PixelType.PHYSICAL) {
    // PHYSICAL (Hardware Pixels).
    const dpr = window.devicePixelRatio;
    // Adjust for device pixel ratio.
    clientX /= dpr;
    clientY /= dpr;
  }

  return {
    element: document.elementFromPoint(clientX, clientY),
    clientX: clientX,
    clientY: clientY,
  };
}
