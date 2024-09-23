// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tags that should not be visited.
const NON_TEXT_NODE_NAMES = new Set([
  'A',
  'APP',
  'APPLET',
  'AREA',
  'AUDIO',
  'BUTTON',
  'CANVAS',
  'CHROME_ANNOTATION',
  'EMBED',
  'FORM',
  'FRAME',
  'FRAMESET',
  'HEAD',
  'IFRAME',
  'IMG',
  'INPUT',
  'KEYGEN',
  'LABEL',
  'MAP',
  'NOSCRIPT',
  'OBJECT',
  'OPTGROUP',
  'OPTION',
  'PROGRESS',
  'SCRIPT',
  'SELECT',
  'STYLE',
  'TEXTAREA',
  'VIDEO',
]);

// Tags that should not be decorated.
const NO_DECORATION_NODE_NAMES = new Set([
  'A', 'LABEL'
]);

// Milliseconds delay between tap bubbling to top and checking for more DOM
// mutations before triggering annotation event.
const MS_DELAY_BEFORE_TRIGGER = 300;

export {
  MS_DELAY_BEFORE_TRIGGER, NON_TEXT_NODE_NAMES, NO_DECORATION_NODE_NAMES,
}