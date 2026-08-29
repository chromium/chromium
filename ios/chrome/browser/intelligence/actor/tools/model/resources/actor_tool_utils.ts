// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for actuation.
 */

/**
 * Coordinate matching optimization_guide::proto::Coordinate.
 */
export interface Coordinate {
  x: number;
  y: number;
  pixelType: number;
}

/**
 * Action target specified via coordinates.
 */
export interface CoordinateTarget {
  coordinate: Coordinate;
}

/**
 * Action target specified via a DOM content node ID.
 */
export interface NodeIdTarget {
  contentNodeId: number;
}

/**
 * ActionTarget representing either a CoordinateTarget or a NodeIdTarget.
 *
 * Based on ActionTarget from actions_data.proto:
 * https://source.chromium.org/chromium/chromium/src/+/main:components/optimization_guide/proto/features/actions_data.proto;l=25;drc=d40c7dbce9b721e32f2befa0d8cd8a0811384995
 */
export type ActionTarget = CoordinateTarget|NodeIdTarget;

/**
 * Returns whether `val` is a valid, finite number.
 */
function isValidNumber(val: number|undefined|null): val is number {
  return typeof val === 'number' && Number.isFinite(val);
}

/**
 * Type guard to check if an ActionTarget is a NodeIdTarget.
 */
export function isNodeIdTarget(target: unknown): target is NodeIdTarget {
  return typeof target === 'object' && target !== null &&
      isValidNumber((target as NodeIdTarget).contentNodeId);
}

/**
 * Type guard to check if an ActionTarget is a CoordinateTarget.
 */
export function isCoordinateTarget(target: unknown):
    target is CoordinateTarget {
  if (typeof target !== 'object' || target === null) {
    return false;
  }
  const coord = (target as CoordinateTarget).coordinate;
  return typeof coord === 'object' && coord !== null &&
      isValidNumber(coord.x) && isValidNumber(coord.y) &&
      isValidNumber(coord.pixelType);
}

/**
 * Returns the element at the given coordinates, accounting for pixel type.
 *
 * @param coordinate The coordinate specifying position and pixel type.
 * @return An object containing the target element and the transformed client
 *     coordinates.
 */
export function getElementFromPoint(coordinate: Coordinate):
    {element: Element|null, clientX: number, clientY: number} {
  // See components/optimization_guide/proto/features/common_quality_data.proto
  // for the definition of PixelType.
  const PixelType = {
    UNSPECIFIED: 0,
    DIPS: 1,
    PHYSICAL: 2,
  };

  // UNSPECIFIED and DIPS are assumed to be viewport coordinates (no change).
  let clientX = coordinate.x;
  let clientY = coordinate.y;

  if (coordinate.pixelType === PixelType.PHYSICAL) {
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
