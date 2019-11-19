// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = {argumentsReceived: false, params: null};

// DefaultColor is used by ColorPicker when selectedColor is not provided.
var DefaultColor = '#000000';

// DefaultColorPalette is used by ColorSuggestionPicker when the list of values
// is empty.
var DefaultColorPalette = [
  '#000000', '#404040', '#808080', '#c0c0c0', '#ffffff', '#980000', '#ff0000',
  '#ff9900', '#ffff00', '#00ff00', '#00ffff', '#4a86e8', '#0000ff', '#9900ff',
  '#ff00ff'
];

/**
 * @param {Event} event
 */
function handleMessage(event) {
  initialize(JSON.parse(event.data));
  global.argumentsReceived = true;
}

if (window.dialogArguments) {
  initialize(dialogArguments);
} else {
  window.addEventListener('message', handleMessage, false);
  window.setTimeout(handleArgumentsTimeout, 1000);
}

/**
 * @param {!Object} args
 */
function initialize(args) {
  global.params = args;
  var main = $('main');
  main.innerHTML = '';
  var errorString;
  if (global.params.shouldShowColorSuggestionPicker) {
    if (global.params.isFormControlsRefreshEnabled) {
      document.body.classList.add('controls-refresh');
    }
    main.classList.add('color-suggestion-picker-main');
    errorString = validateColorSuggestionPickerArguments(args);
  } else {
    main.classList.add('color-picker-main');
    errorString = validateColorPickerArguments(args);
  }
  if (errorString) {
    main.textContent = 'Internal error: ' + errorString;
    resizeWindow(main.offsetWidth, main.offsetHeight);
  } else {
    if (global.params.shouldShowColorSuggestionPicker) {
      initializeColorSuggestionPicker();
    } else {
      initializeColorPicker();
    }
  }
}

function handleArgumentsTimeout() {
  if (global.argumentsReceived)
    return;
  // When an argument timeout happens, we do not have enough information to
  // determine whether to show the color picker or color suggestion picker.
  // In this case, we just choose to show the color suggestion picker.
  var args = {
    values: DefaultColorPalette,
    otherColorLabel: 'Other...',
    shouldShowColorSuggestionPicker: true,
    selectedColor: DefaultColor,
  };
  initialize(args);
}