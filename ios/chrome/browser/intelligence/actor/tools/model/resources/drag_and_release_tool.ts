// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A script to drag and release elements on a web page.
 *
 * Based on the DragAndReleaseTool built on Desktop:
 * https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/drag_and_release_tool.cc;l=50;drc=36e01346a61fe3cda0df8b3672cceb4223ca7d36
 */

import type {ActionTarget, Coordinate} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getElementFromPoint, isCoordinateTarget, isDisabled, isNodeIdTarget} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {areElementsConnected, safeIsDraggable, safeNodeType, safeParentElement} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/safe_dom_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// Distance interval (in pixels) for intermediate drag movement steps, matching
// Desktop's behavior:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/drag_and_release_tool.cc;l=38;drc=36e01346a61fe3cda0df8b3672cceb4223ca7d36
const DRAG_INTERVAL_PIXELS = 20;

// LINT.IfChange(DragAndReleaseToolResultCode)
export enum DragAndReleaseToolResultCode {
  // The function call was successful.
  OK = 0,
  // The coordinates provided for the source target were not in the viewport.
  FROM_COORDINATES_OUT_OF_BOUNDS = 1,
  // The coordinates provided for the destination target were not in the
  // viewport.
  TO_COORDINATES_OUT_OF_BOUNDS = 2,
  // The DOM node ID provided for the source target was not found or not an
  // Element.
  FROM_INVALID_DOM_NODE_ID = 3,
  // The DOM node ID provided for the destination target was not found or not an
  // Element.
  TO_INVALID_DOM_NODE_ID = 4,
  // The drag event was suppressed.
  DRAG_SUPPRESSED = 5,
  // The source element is disabled.
  FROM_ELEMENT_DISABLED = 6,
  // The destination element is disabled.
  TO_ELEMENT_DISABLED = 7,
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool_js_unittest.mm:DragAndReleaseToolResultCode)

/**
 * An element and its coordinates resolved from an ActionTarget.
 */
interface ResolvedTarget {
  /** The resolved DOM element. */
  element: Element;
  /** Client x-coordinate in viewport space. */
  clientX: number;
  /** Client y-coordinate in viewport space. */
  clientY: number;
}

type TargetResolutionResult = {
  success: true,
  target: ResolvedTarget,
}|{success: false, resultCode: number, message: string};

/**
 * Resolves a target specified by a DOM node ID.
 * @param nodeId The DOM node ID.
 * @param isSource Whether this is the source target (true) or destination
 *     target (false).
 * @return The resolution result.
 */
function resolveNodeTarget(
    nodeId: number, isSource: boolean): TargetResolutionResult {
  const node = getNodeById(nodeId, window);
  if (!node) {
    return {
      success: false,
      resultCode: isSource ?
          DragAndReleaseToolResultCode.FROM_INVALID_DOM_NODE_ID :
          DragAndReleaseToolResultCode.TO_INVALID_DOM_NODE_ID,
      message: `No element found with id ${nodeId}.`,
    };
  }

  let element: Element|null = null;
  const nodeType = safeNodeType(node);
  if (nodeType === Node.ELEMENT_NODE) {
    element = node as Element;
  } else if (nodeType === Node.TEXT_NODE) {
    element = safeParentElement(node);
  }

  if (!element) {
    return {
      success: false,
      resultCode: isSource ?
          DragAndReleaseToolResultCode.FROM_INVALID_DOM_NODE_ID :
          DragAndReleaseToolResultCode.TO_INVALID_DOM_NODE_ID,
      message: `Node with id ${nodeId} is not an Element.`,
    };
  }

  const rect = element.getBoundingClientRect();
  const clientX = rect.left + rect.width / 2;
  const clientY = rect.top + rect.height / 2;
  if (clientX < 0 || clientX > window.innerWidth || clientY < 0 ||
      clientY > window.innerHeight) {
    return {
      success: false,
      resultCode: isSource ?
          DragAndReleaseToolResultCode.FROM_COORDINATES_OUT_OF_BOUNDS :
          DragAndReleaseToolResultCode.TO_COORDINATES_OUT_OF_BOUNDS,
      message: `Element with id ${nodeId} is outside of the viewport.`,
    };
  }

  return {
    success: true,
    target: {
      element,
      clientX,
      clientY,
    },
  };
}

/**
 * Resolves a target specified by coordinates.
 * @param coordinate The target coordinate.
 * @param isSource Whether this is the source target (true) or destination
 *     target (false).
 * @return The resolution result.
 */
function resolveCoordinateTarget(
    coordinate: Coordinate, isSource: boolean): TargetResolutionResult {
  const {element, clientX, clientY} = getElementFromPoint(coordinate);
  if (!element) {
    return {
      success: false,
      resultCode: isSource ?
          DragAndReleaseToolResultCode.FROM_COORDINATES_OUT_OF_BOUNDS :
          DragAndReleaseToolResultCode.TO_COORDINATES_OUT_OF_BOUNDS,
      message:
          `No element found at coordinates (${coordinate.x}, ${coordinate.y}).`,
    };
  }
  return {
    success: true,
    target: {element, clientX, clientY},
  };
}

/**
 * Resolves an ActionTarget specification to a DOM element and client
 * coordinates.
 * @param target The target specification with coordinates or DOM node ID.
 * @param isSource Whether this is the source target (true) or destination
 *     target (false).
 * @return The resolution result.
 */
function resolveTarget(
    target: ActionTarget, isSource: boolean): TargetResolutionResult {
  if (isNodeIdTarget(target)) {
    return resolveNodeTarget(target.contentNodeId, isSource);
  }

  if (isCoordinateTarget(target)) {
    return resolveCoordinateTarget(target.coordinate, isSource);
  }

  return {
    success: false,
    resultCode: isSource ?
        DragAndReleaseToolResultCode.FROM_COORDINATES_OUT_OF_BOUNDS :
        DragAndReleaseToolResultCode.TO_COORDINATES_OUT_OF_BOUNDS,
    message: 'Invalid target specification.',
  };
}

/**
 * Creates a Touch object with the given coordinates.
 */
function createTouch(target: Element, clientX: number, clientY: number): Touch {
  return new Touch({identifier: 0, target, clientX, clientY});
}

/**
 * Dispatches a TouchEvent on the given element.
 */
function dispatchTouchEvent(
    element: Element, type: string, touches: Touch[], targetTouches: Touch[],
    changedTouches: Touch[]): void {
  element.dispatchEvent(new TouchEvent(type, {
    bubbles: true,
    cancelable: true,
    view: window,
    touches,
    targetTouches,
    changedTouches,
  }));
}

/**
 * Creates a DataTransfer instance for drag events.
 */
function createDataTransfer(): DataTransfer {
  const dt = new DataTransfer();
  // 'all' to permit any drop target operation (move/copy/link).
  dt.effectAllowed = 'all';
  // 'copy' to align with Desktop's default when effectAllowed: 'all'.
  // See
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/clipboard/data_transfer.cc;l=221;drc=9992be78bde6193f5d95a53609c03a0039deb414
  dt.dropEffect = 'copy';
  return dt;
}

/**
 * Creates a DragEvent with the given coordinates and payload.
 * @param type The drag event type (e.g. 'dragstart' or 'drop').
 * @param clientX Viewport X coordinate.
 * @param clientY Viewport Y coordinate.
 * @param dataTransfer The DataTransfer object for the drag operation.
 * @param buttons Bitmask of pressed mouse buttons (default: 1 for primary).
 * @return The configured DragEvent instance.
 */
function createDragEvent(
    type: string, clientX: number, clientY: number, dataTransfer: DataTransfer,
    buttons = 1): DragEvent {
  return new DragEvent(type, {
    bubbles: true,
    cancelable: true,
    view: window,
    dataTransfer,
    clientX,
    clientY,
    buttons,
  });
}

/**
 * Dispatches the drag initiation phase on the source element:
 * - If HTML5 draggable: mousemove -> mousedown -> dragstart.
 * - Otherwise: touchstart.
 *
 * @param fromElement The source DOM element.
 * @param toElement The destination DOM element.
 * @param fromX The starting viewport X coordinate.
 * @param fromY The starting viewport Y coordinate.
 * @param draggable Whether the element is an HTML5 draggable target.
 * @param dataTransfer The drag data transfer object if HTML5.
 * @return True if both elements remain connected to the document.
 */
function dispatchDragStart(
    fromElement: Element, toElement: Element, fromX: number, fromY: number,
    draggable: boolean, dataTransfer: DataTransfer|null): boolean {
  if (draggable && dataTransfer) {
    fromElement.dispatchEvent(new MouseEvent('mousemove', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: fromX,
      clientY: fromY,
    }));

    fromElement.dispatchEvent(new MouseEvent('mousedown', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: fromX,
      clientY: fromY,
      buttons: 1,
    }));

    fromElement.dispatchEvent(
        createDragEvent('dragstart', fromX, fromY, dataTransfer));
  } else {
    const startTouch = createTouch(fromElement, fromX, fromY);
    dispatchTouchEvent(
        fromElement, 'touchstart', [startTouch], [startTouch], [startTouch]);
  }

  return areElementsConnected(fromElement, toElement);
}

/**
 * Dispatches intermediate trajectory events along the drag path:
 * - If HTML5 draggable: mousemove (currentElement), drag (fromElement),
 *   dragleave/dragenter/dragover (currentElement).
 * - Otherwise: touchmove (strictly target-locked to fromElement).
 *
 * @param fromElement The source DOM element.
 * @param toElement The destination DOM element.
 * @param fromX The starting viewport X coordinate.
 * @param fromY The starting viewport Y coordinate.
 * @param toX The ending viewport X coordinate.
 * @param toY The ending viewport Y coordinate.
 * @param draggable Whether the element is an HTML5 draggable target.
 * @param dataTransfer The drag data transfer object if HTML5.
 * @return True if both elements remain connected to the document.
 */
function dispatchIntermediateDrag(
    fromElement: Element, toElement: Element, fromX: number, fromY: number,
    toX: number, toY: number, draggable: boolean,
    dataTransfer: DataTransfer|null): boolean {
  const deltaX = toX - fromX;
  const deltaY = toY - fromY;
  const distance = Math.hypot(deltaX, deltaY);
  const stepCount = Math.max(1, Math.ceil(distance / DRAG_INTERVAL_PIXELS));

  let previousTargetElement: Element = fromElement;

  for (let i = 1; i <= stepCount; i++) {
    if (!areElementsConnected(fromElement, toElement)) {
      return false;
    }

    const progress = i / stepCount;
    const currentX = fromX + deltaX * progress;
    const currentY = fromY + deltaY * progress;

    if (draggable && dataTransfer) {
      const currentElement = document.elementFromPoint(currentX, currentY) ||
          (i === stepCount ? toElement : fromElement);

      currentElement.dispatchEvent(new MouseEvent('mousemove', {
        bubbles: true,
        cancelable: true,
        view: window,
        clientX: currentX,
        clientY: currentY,
        buttons: 1,
      }));

      fromElement.dispatchEvent(
          createDragEvent('drag', currentX, currentY, dataTransfer));

      if (currentElement !== previousTargetElement) {
        previousTargetElement.dispatchEvent(
            createDragEvent('dragleave', currentX, currentY, dataTransfer));
        currentElement.dispatchEvent(
            createDragEvent('dragenter', currentX, currentY, dataTransfer));
        previousTargetElement = currentElement;
      } else if (i === 1) {
        currentElement.dispatchEvent(
            createDragEvent('dragenter', currentX, currentY, dataTransfer));
      }

      currentElement.dispatchEvent(
          createDragEvent('dragover', currentX, currentY, dataTransfer));
    } else {
      const moveTouch = createTouch(fromElement, currentX, currentY);
      dispatchTouchEvent(
          fromElement, 'touchmove', [moveTouch], [moveTouch], [moveTouch]);
    }
  }

  return areElementsConnected(fromElement, toElement);
}

/**
 * Dispatches terminal drop and release events:
 * - If HTML5 draggable: drop (toElement) -> mouseup (toElement) -> dragend
 * (fromElement).
 * - Otherwise: touchend (fromElement).
 *
 * @param fromElement The source DOM element.
 * @param toElement The destination DOM element.
 * @param toX The ending viewport X coordinate.
 * @param toY The ending viewport Y coordinate.
 * @param draggable Whether the element is an HTML5 draggable target.
 * @param dataTransfer The drag data transfer object if HTML5.
 */
function dispatchDragRelease(
    fromElement: Element, toElement: Element, toX: number, toY: number,
    draggable: boolean, dataTransfer: DataTransfer|null): void {
  if (draggable && dataTransfer) {
    toElement.dispatchEvent(createDragEvent('drop', toX, toY, dataTransfer));

    toElement.dispatchEvent(new MouseEvent('mouseup', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: toX,
      clientY: toY,
    }));

    fromElement.dispatchEvent(
        createDragEvent('dragend', toX, toY, dataTransfer, /*buttons=*/ 0));
  } else {
    const endTouch = createTouch(fromElement, toX, toY);
    dispatchTouchEvent(fromElement, 'touchend', [], [], [endTouch]);
  }
}

/**
 * Updates range slider value and fires input/change events if element is a
 * range input.
 * fromElement: input -> change
 *
 * @param fromElement The element being dragged.
 * @param toX The target viewport X coordinate.
 */
function updateRangeSliderValueIfNeeded(
    fromElement: Element, toX: number): void {
  if (!(fromElement instanceof HTMLInputElement &&
        fromElement.type === 'range')) {
    return;
  }
  const parsedMin = parseFloat(fromElement.min);
  const parsedMax = parseFloat(fromElement.max);
  const min = !isNaN(parsedMin) ? parsedMin : 0;
  const max =
      !isNaN(parsedMax) && parsedMax >= min ? parsedMax : Math.max(100, min);
  const isAnyStep = fromElement.step?.toLowerCase() === 'any';
  const parsedStep = parseFloat(fromElement.step);
  const step =
      (!isAnyStep && !isNaN(parsedStep) && parsedStep > 0) ? parsedStep : 1;
  const rect = fromElement.getBoundingClientRect();
  const isRtl = window.getComputedStyle(fromElement).direction === 'rtl';

  let fraction = 0;
  if (rect.width > 0) {
    fraction = Math.max(0, Math.min(1, (toX - rect.left) / rect.width));
    if (isRtl) {
      fraction = 1 - fraction;
    }
  }

  let newValue = min + fraction * (max - min);
  if (!isAnyStep) {
    newValue = min + Math.round((newValue - min) / step) * step;
  }
  newValue = Math.max(min, Math.min(max, newValue));

  fromElement.value = String(newValue);
  fromElement.dispatchEvent(new Event('input', {bubbles: true}));
  fromElement.dispatchEvent(new Event('change', {bubbles: true}));
}

/**
 * Coordinates simulated drag and release events across start, intermediate,
 * and release phases:
 * 1. dispatchDragStart
 * 2. dispatchIntermediateDrag
 * 3. dispatchDragRelease
 * 4. updateRangeSliderValueIfNeeded
 *
 * @param fromTarget The resolved source target.
 * @param toTarget The resolved destination target.
 * @return The result of the drag and release operation.
 */
function dispatchDragAndReleaseEvents(
    fromTarget: ResolvedTarget,
    toTarget: ResolvedTarget): {resultCode: number, message: string} {
  const {element: fromElement, clientX: fromX, clientY: fromY} = fromTarget;
  const {element: toElement, clientX: toX, clientY: toY} = toTarget;

  const draggable = safeIsDraggable(fromElement);
  const dataTransfer = draggable ? createDataTransfer() : null;

  const suppressionResult = {
    resultCode: DragAndReleaseToolResultCode.DRAG_SUPPRESSED,
    message: 'Drag suppressed: element was detached from the document.',
  };

  // TODO(crbug.com/557176809): Follow up after testing to determine whether an
  // artificial delay between drag events (e.g. matching Desktop's
  // kInitialMoveDelay and kMoveDelay) is necessary for site compatibility.
  if (!dispatchDragStart(
          fromElement, toElement, fromX, fromY, draggable, dataTransfer)) {
    return suppressionResult;
  }

  if (!dispatchIntermediateDrag(
          fromElement, toElement, fromX, fromY, toX, toY, draggable,
          dataTransfer)) {
    return suppressionResult;
  }

  dispatchDragRelease(
      fromElement, toElement, toX, toY, draggable, dataTransfer);

  updateRangeSliderValueIfNeeded(fromElement, toX);

  return {
    resultCode: DragAndReleaseToolResultCode.OK,
    message: 'Dispatched drag and release events.',
  };
}


/**
 * Performs a drag and release action from a source target to a destination
 * target.
 * @param fromTarget The source target specification (coordinates or DOM node
 *     ID).
 * @param toTarget The destination target specification (coordinates or DOM
 *     node ID).
 * @return Result code and status message.
 */
function dragAndRelease(fromTarget: ActionTarget, toTarget: ActionTarget):
    {resultCode: number, message: string} {
  const source = resolveTarget(fromTarget, /*isSource=*/ true);
  if (!source.success) {
    return {resultCode: source.resultCode, message: source.message};
  }

  const destination = resolveTarget(toTarget, /*isSource=*/ false);
  if (!destination.success) {
    return {resultCode: destination.resultCode, message: destination.message};
  }

  if (isDisabled(source.target.element)) {
    return {
      resultCode: DragAndReleaseToolResultCode.FROM_ELEMENT_DISABLED,
      message: 'Source element is disabled.',
    };
  }

  if (isDisabled(destination.target.element)) {
    return {
      resultCode: DragAndReleaseToolResultCode.TO_ELEMENT_DISABLED,
      message: 'Destination element is disabled.',
    };
  }

  return dispatchDragAndReleaseEvents(source.target, destination.target);
}

const dragAndReleaseToolApi = new CrWebApi('drag_and_release_tool');
dragAndReleaseToolApi.addFunction('dragAndRelease', dragAndRelease);
gCrWeb.registerApi(dragAndReleaseToolApi);
