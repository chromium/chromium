/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   JS code for the button design pattern
 */

'use strict';

var ICON_MUTE_URL = '#icon-mute';
var ICON_SOUND_URL = '#icon-sound';

function init() {
  var actionButton = document.getElementById('action');
  actionButton.addEventListener('click', activateActionButton);
  actionButton.addEventListener('keydown', actionButtonKeydownHandler);
  actionButton.addEventListener('keyup', actionButtonKeyupHandler);

  var toggleButton = document.getElementById('toggle');
  toggleButton.addEventListener('click', toggleButtonClickHandler);
  toggleButton.addEventListener('keydown', toggleButtonKeydownHandler);
  toggleButton.addEventListener('keyup', toggleButtonKeyupHandler);
}

/**
 * Activates the action button with the enter key.
 *
 * @param {KeyboardEvent} event
 */
function actionButtonKeydownHandler(event) {
  // The action button is activated by space on the keyup event, but the
  // default action for space is already triggered on keydown. It needs to be
  // prevented to stop scrolling the page before activating the button.
  if (event.keyCode === 32) {
    event.preventDefault();
  }
  // If enter is pressed, activate the button
  else if (event.keyCode === 13) {
    event.preventDefault();
    activateActionButton();
  }
}

/**
 * Activates the action button with the space key.
 *
 * @param {KeyboardEvent} event
 */
function actionButtonKeyupHandler(event) {
  if (event.keyCode === 32) {
    event.preventDefault();
    activateActionButton();
  }
}

function activateActionButton() {
  window.print();
}

/**
 * Toggles the toggle button’s state if it’s actually a button element or has
 * the `role` attribute set to `button`.
 *
 * @param {MouseEvent} event
 */
function toggleButtonClickHandler(event) {
  if (
    event.currentTarget.tagName === 'button' ||
    event.currentTarget.getAttribute('role') === 'button'
  ) {
    toggleButtonState(event.currentTarget);
  }
}

/**
 * Toggles the toggle button’s state with the enter key.
 *
 * @param {KeyboardEvent} event
 */
function toggleButtonKeydownHandler(event) {
  if (event.keyCode === 32) {
    event.preventDefault();
  } else if (event.keyCode === 13) {
    event.preventDefault();
    toggleButtonState(event.currentTarget);
  }
}

/**
 * Toggles the toggle button’s state with space key.
 *
 * @param {KeyboardEvent} event
 */
function toggleButtonKeyupHandler(event) {
  if (event.keyCode === 32) {
    event.preventDefault();
    toggleButtonState(event.currentTarget);
  }
}

/**
 * Toggles the toggle button’s state between *pressed* and *not pressed*.
 *
 * @param {HTMLElement} button
 */
function toggleButtonState(button) {
  var isAriaPressed = button.getAttribute('aria-pressed') === 'true';

  button.setAttribute('aria-pressed', isAriaPressed ? 'false' : 'true');

  var icon = button.querySelector('use');
  icon.setAttribute(
    'xlink:href',
    isAriaPressed ? ICON_SOUND_URL : ICON_MUTE_URL
  );
}

window.onload = init;
