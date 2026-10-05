// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for performing type actions on elements.
 */

import type {ActionTarget, Coordinate} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getElementFromPoint, isCoordinateTarget, isNodeIdTarget} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// LINT.IfChange(TypeToolResultCode)
enum TypeToolResultCode {
  // The function call was successful.
  OK = 0,
  // The coordinates provided to target the element were not in the viewport.
  COORDINATES_OUT_OF_BOUNDS = 1,
  // The DOM node id provided to target the element was not in the viewport.
  INVALID_DOM_NODE_ID = 2,
  // The target provided exists but is not an Element.
  TYPE_TARGET_NOT_ELEMENT = 3,
  // The target element is not focusable.
  TYPE_TARGET_NOT_FOCUSABLE = 4,
  // The function had invalid arguments passed in.
  INVALID_ARGUMENTS = 5,
  // The target element is disabled.
  ELEMENT_DISABLED = 6,
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h:TypeToolResultCode)

/**
 * Writes `text` into `element` based on its type (`value` for
 * `<input>`/`<textarea>`, `innerText` for `contentEditable`).
 *
 * Target elements may sanitize the value, e.g. `<input type="number">` discards
 * non-numeric text. This is not reported as an error, matching Desktop, which
 * lets Blink decline such characters silently.
 */
function commitValue(element: HTMLElement, text: string): void {
  if (element instanceof HTMLInputElement ||
      element instanceof HTMLTextAreaElement) {
    element.value = text;
    return;
  }
  element.innerText = text;
}

/**
 * Dispatches a sequence of events (`keydown`, `keypress`, `beforeinput`,
 * `input`, `keyup`, `change`) on `element` and writes `newValue` into it once
 * `beforeinput` has been successfully dispatched.
 *
 * Site-provided event listeners may `preventDefault()` on any of the following
 * cancelable events, suppressing the value update and the subsequent events:
 * - `keydown`: https://w3c.github.io/uievents/#event-type-keydown
 * - `keypress`: https://w3c.github.io/uievents/#event-type-keypress
 * - `beforeinput`: https://w3c.github.io/input-events/#event-type-beforeinput
 *
 * Writing only after `beforeinput` is accepted mirrors Blink's ordering in
 * `Editor::HandleEditingKeyboardEvent`:
 * https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/editing/editor_key_bindings.cc;l=119-125;drc=8abea14deda089834ba142a35e8342014812df55
 *
 * The outstanding events are non-cancelable and sent unconditionally:
 * - `input`: https://w3c.github.io/uievents/#event-type-input
 * - `keyup`: https://w3c.github.io/uievents/#event-type-keyup
 * - `change`: https://html.spec.whatwg.org/multipage/indices.html#event-change
 *
 * @param element The target element on which to dispatch events.
 * @param keyEventInit Initialization options for the keyboard events.
 * @param newValue The value to write into `element` once `beforeinput` is
 *     accepted, or null to dispatch the events without mutating `element`.
 */
function simulateEventsAndCommitValue(
    element: HTMLElement, keyEventInit: KeyboardEventInit,
    newValue: string|null): void {
  let inputDispatched = false;
  if (element.dispatchEvent(new KeyboardEvent('keydown', keyEventInit)) &&
      element.dispatchEvent(new KeyboardEvent('keypress', keyEventInit)) &&
      element.dispatchEvent(
          new InputEvent('beforeinput', {bubbles: true, cancelable: true}))) {
    if (newValue !== null) {
      commitValue(element, newValue);
    }
    element.dispatchEvent(new InputEvent('input', {bubbles: true}));
    inputDispatched = true;
  }
  element.dispatchEvent(new KeyboardEvent('keyup', keyEventInit));
  if (inputDispatched) {
    element.dispatchEvent(new Event('change', {bubbles: true}));
  }
}

/**
 * Returns whether `element` has a direct `Text` child node with non-whitespace
 * characters (ignoring formatting whitespace between tags).
 * @param element The HTML element whose child nodes are inspected.
 * @return True if `element` contains a direct non-whitespace `Text` node.
 */
function hasDirectNonWhitespaceText(element: HTMLElement): boolean {
  for (const node of Array.from(element.childNodes)) {
    if (node.nodeType === Node.TEXT_NODE &&
        (node.textContent ?? '').trim().length > 0) {
      return true;
    }
  }
  return false;
}

/**
 * Resolves `element` to a leaf editable `HTMLElement` that can be mutated
 * without clobbering child elements, or `null` if ambiguous or not editable.
 */
function resolveLeafEditableElement(element: HTMLElement): HTMLElement|null {
  if (element instanceof HTMLInputElement ||
      element instanceof HTMLTextAreaElement) {
    return element;
  }
  if (!element.isContentEditable) {
    return null;
  }

  // Unwrap single-child `contentEditable` wrappers so assigning `innerText` on
  // the leaf does not destroy wrapper elements (e.g. `<p><span>...</span></p>`)
  // or a wrapped `<input>`.
  let current = element;
  while (!(current instanceof HTMLInputElement) &&
         !(current instanceof HTMLTextAreaElement)) {
    // Ignore placeholder `<br>` elements inside `contentEditable` blocks.
    const nonBrChildren =
        Array.from(current.children)
            .filter(child => !(child instanceof HTMLBRElement));
    if (nonBrChildren.length === 0) {
      break;
    }
    // Fail if unwrapping is ambiguous (multiple children, or sibling text
    // alongside a child element that would be clobbered by `innerText`).
    if (nonBrChildren.length > 1 || hasDirectNonWhitespaceText(current) ||
        !(nonBrChildren[0] instanceof HTMLElement)) {
      return null;
    }
    current = nonBrChildren[0];
  }

  // A child inside a `contentEditable` host may explicitly opt out via
  // `contenteditable="false"`.
  const isInputOrTextArea = current instanceof HTMLInputElement ||
      current instanceof HTMLTextAreaElement;
  if (!isInputOrTextArea && !current.isContentEditable) {
    return null;
  }
  return current;
}

/**
 * Updates the text in element and sends events to simulate manual typing.
 * @param element The target element.
 * @param text The text to type.
 * @param mode The type mode (1=DELETE_EXISTING, 2=PREPEND, 3=APPEND) from
 *             optimization_guide::Action::TypeAction::TypeMode in
 *             components/optimization_guide/proto/features/actions_data.proto.
 * @param followByEnter Whether to press enter after typing.
 * @return an object containing the result of the type attempt.
 */
function updateElementAndDispatchTypeEvents(
    element: HTMLElement, text: string, mode: number,
    followByEnter: boolean): {resultCode: number, message: string} {
  const leafElement = resolveLeafEditableElement(element);
  if (!leafElement) {
    return {
      resultCode: TypeToolResultCode.TYPE_TARGET_NOT_FOCUSABLE,
      message: 'Target element does not resolve to an input, textarea, or ' +
          'leaf contentEditable.',
    };
  }

  // Check if the target or resolved leaf element is disabled, following
  // Desktop's example. See:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/type_tool.cc;l=664;drc=228f9f11d9a39d57c02e2f4cd101ff978a812717
  if ((element as any).disabled || (leafElement as any).disabled) {
    return {
      resultCode: TypeToolResultCode.ELEMENT_DISABLED,
      message: 'Target element is disabled.',
    };
  }

  const isInputOrTextArea = leafElement instanceof HTMLInputElement ||
      leafElement instanceof HTMLTextAreaElement;
  const currentText =
      isInputOrTextArea ? leafElement.value : leafElement.innerText;

  let newText = '';
  switch (mode) {
    case 1:  // DELETE_EXISTING
      newText = text;
      break;
    case 2:  // PREPEND
      newText = text + currentText;
      break;
    case 3:  // APPEND
      newText = currentText + text;
      break;
    default: {
      return {
        resultCode: TypeToolResultCode.INVALID_ARGUMENTS,
        message: 'Invalid type mode. Must be in [1,3].',
      };
    }
  }

  // Focus the element, following Desktop's example which clicks on the input
  // element:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/type_tool.cc;l=486;drc=b36c618599df1c588a7485ace1f89e82a8cf2ee5.
  //
  // Per the HTML spec, only the root editing host is focusable by default, not
  // editable descendants like `<p>` or `<span>`:
  // https://html.spec.whatwg.org/multipage/interaction.html#editing-host
  // https://html.spec.whatwg.org/multipage/interaction.html#the-tabindex-attribute
  // https://html.spec.whatwg.org/multipage/interaction.html#dom-focus
  //
  // Focusing `editingHost` first activates the editing host; calling
  // `leafElement.focus()` afterward shifts focus if `leafElement` is itself
  // focusable (e.g., a nested `<input>`) and is a no-op otherwise.
  let editingHost = leafElement;
  let parent = editingHost.parentElement;
  while (parent instanceof HTMLElement && parent.isContentEditable) {
    editingHost = parent;
    parent = editingHost.parentElement;
  }
  editingHost.focus();
  if (leafElement !== editingHost) {
    leafElement.focus();
  }

  simulateEventsAndCommitValue(
      leafElement, {bubbles: true, cancelable: true}, newText);

  if (followByEnter) {
    simulateEventsAndCommitValue(
        leafElement, {
          bubbles: true,
          cancelable: true,
          key: 'Enter',
          code: 'Enter',
          keyCode: 13,
          which: 13,
        },
        /*newValue=*/ null);
  }

  return {
    resultCode: TypeToolResultCode.OK,
    message: 'Dispatched type events.',
  };
}

/**
 * Simulates typing into an element at the specified coordinates.
 * @param coordinate The target coordinate.
 * @param text The text to type.
 * @param mode The type mode.
 * @param followByEnter Whether to press enter after typing.
 * @return An object containing the result of the type attempt.
 */
function typeByCoordinate(
    coordinate: Coordinate, text: string, mode: number,
    followByEnter: boolean): {resultCode: number, message: string} {
  const {element} = getElementFromPoint(coordinate);

  if (!element || !(element instanceof HTMLElement)) {
    return {
      resultCode: TypeToolResultCode.TYPE_TARGET_NOT_ELEMENT,
      message: 'No element found at the target coordinates.',
    };
  }

  return updateElementAndDispatchTypeEvents(element, text, mode, followByEnter);
}

/**
 * Simulates typing into an element with the given DOM node ID.
 * @param nodeId The DOM node ID of the target element.
 * @param text The text to type.
 * @param mode The type mode.
 * @param followByEnter Whether to press enter after typing.
 * @return an object containing the result of the type attempt.
 */
function typeByNodeId(
    nodeId: number, text: string, mode: number,
    followByEnter: boolean): {resultCode: number, message: string} {
  const node = getNodeById(nodeId, window);
  if (!node) {
    return {
      resultCode: TypeToolResultCode.INVALID_DOM_NODE_ID,
      message: `No element found with id ${nodeId}.`,
    };
  }

  let element: HTMLElement;
  if (node instanceof HTMLElement) {
    element = node;
  } else if (node.parentElement instanceof HTMLElement) {
    element = node.parentElement;
  } else {
    return {
      resultCode: TypeToolResultCode.TYPE_TARGET_NOT_ELEMENT,
      message: `Node with id ${nodeId} is not an HTMLElement.`,
    };
  }

  return updateElementAndDispatchTypeEvents(element, text, mode, followByEnter);
}

/**
 * Simulates typing into an element specified by an ActionTarget.
 * @param target The action target (coordinate or node ID).
 * @param text The text to type.
 * @param mode The type mode.
 * @param followByEnter Whether to press enter.
 * @return an object containing the result of the type attempt.
 */
function type(
    target: ActionTarget, text: string, mode: number, followByEnter: boolean): {
  resultCode: number,
  message: string,
} {
  if (isNodeIdTarget(target)) {
    return typeByNodeId(target.contentNodeId, text, mode, followByEnter);
  } else if (isCoordinateTarget(target)) {
    return typeByCoordinate(target.coordinate, text, mode, followByEnter);
  }

  return {
    resultCode: TypeToolResultCode.INVALID_ARGUMENTS,
    message: 'Invalid target.',
  };
}

const typeToolApi = new CrWebApi('type_tool');
typeToolApi.addFunction('type', type);
gCrWeb.registerApi(typeToolApi);
