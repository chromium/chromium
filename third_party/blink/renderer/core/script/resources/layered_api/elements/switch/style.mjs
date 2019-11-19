// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Style constant values.
const COLOR_ON = '#0077FF';
const TRACK_RADIUS = '13px';
const TRACK_BORDER_WIDTH = '2px';
const THUMB_HEIGHT = '22px';
const THUMB_WIDTH = '22px';
const THUMB_MARGIN_START = '2px';
const THUMB_MARGIN_END = '2px';

const MATERIAL_THUMB_SIZE = '20px';
const RIPPLE_COLOR = 'rgba(100,100,100,0.3)';
const RIPPLE_MAX_SIZE = '48px';

// Returns a function returning a CSSStyleSheet().
// TODO(tkent): Share this stylesheet factory feature with elements/toast/.
export function styleSheetFactory() {
  let styleSheet;
  return () => {
    if (!styleSheet) {
      styleSheet = new CSSStyleSheet();
      styleSheet.replaceSync(`
:host {
  block-size: 26px;
  border: none;
  box-sizing: border-box;
  display: inline-block;
  inline-size: 54px;
  user-select: none;
  vertical-align: middle;
}

#container {
  align-items: center;
  block-size: 100%;
  display: inline-flex;
  inline-size: 100%;
}

#thumb {
  background: white;
  block-size: ${THUMB_HEIGHT};
  border-radius: calc(${THUMB_HEIGHT} / 2);
  border: 1px solid black;
  box-sizing: border-box;
  display: inline-block;
  margin-inline-start: calc(-100% + ${THUMB_MARGIN_START});
  inline-size: ${THUMB_WIDTH};
}

/* :host::part(thumb-transitioning) doesn't work. crbug.com/980506 */
#thumb[part~="thumb-transitioning"] {
  transition: all linear 0.1s;
}

:host([on]) #thumb {
  border: 1px solid ${COLOR_ON};
  margin-inline-start: calc(0px - ${THUMB_WIDTH} - ${THUMB_MARGIN_END});
}

#track {
  block-size: 100%;
  border-radius: ${TRACK_RADIUS};
  border: ${TRACK_BORDER_WIDTH} solid #dddddd;
  box-shadow: 0 0 0 1px #f8f8f8;
  box-sizing: border-box;
  display: inline-block;
  inline-size: 100%;
  overflow: hidden;
  padding: 0px;
}

#trackFill {
  background: ${COLOR_ON};
  block-size: 100%;
  border-radius: calc(${TRACK_RADIUS}) - ${TRACK_BORDER_WIDTH});
  box-shadow: none;
  box-sizing: border-box;
  display: inline-block;
  inline-size: 0%;
  vertical-align: top;
}

#trackFill[part~="track-fill-transitioning"] {
  transition: all linear 0.1s;
}

:host([on]) #track {
  border: ${TRACK_BORDER_WIDTH} solid ${COLOR_ON};
}

:host(:focus) {
  outline-offset: 4px;
}

:host(:focus) #track {
  box-shadow: 0 0 0 2px #f8f8f8;
}

:host([on]:focus) #track {
  box-shadow: 0 0 0 2px #dddddd;
}

:host(:focus) #thumb {
  border: 2px solid black;
}

:host([on]:focus) #thumb {
  border: 2px solid ${COLOR_ON};
}

:host(:not(:focus-visible):focus) {
  outline: none;
}

:host(:not(:disabled):hover) #thumb {
  inline-size: 26px;
}

:host([on]:not(:disabled):hover) #thumb {
  margin-inline-start: calc(0px - 26px - ${THUMB_MARGIN_END});
}

:host(:active) #track {
  background: #dddddd;
}

:host([on]:active) #track {
  border: 2px solid #77bbff;
  box-shadow: 0 0 0 2px #f8f8f8;
}

:host([on]:active) #trackFill {
  background: #77bbff;
}

:host(:disabled) {
  opacity: 0.38;
}

/*
 * display:inline-block in the :host ruleset overrides 'hidden' handling
 * by the user agent.
 */
:host([hidden]) {
  display: none;
}

`);
    }
    return styleSheet;
  };
}

export function materialStyleSheetFactory() {
  let styleSheet;
  return () => {
    if (!styleSheet) {
      styleSheet = new CSSStyleSheet();
      styleSheet.replaceSync(`
:host {
  block-size: 20px;
  inline-size: 36px;
}

#track,
:host(:active) #track {
  background: rgba(0,0,0,0.4);
  block-size: 14px;
  border: none;
  box-shadow: none;
}

:host([on]) #track,
:host([on]:active) #track,
:host([on]:focus) #track {
  border: none;
  box-shadow: none;
}

#trackFill,
:host([on]:active) #trackFill {
  background: rgba(63,81,181,0.5);
}

#thumb,
:host(:focus) #thumb {
  block-size: ${MATERIAL_THUMB_SIZE};
  border-radius: calc(${MATERIAL_THUMB_SIZE} / 2);
  border: none;
  box-shadow: 0 1px 5px 0 rgba(0,0,0,0.6);
  inline-size: ${MATERIAL_THUMB_SIZE};
  margin-inline-start: -100%;
}

:host([on]) #thumb,
:host([on]:focus) #thumb,
:host([on]:not(:disabled):hover) #thumb {
  background: rgb(63,81,181);
  border: none;
  margin-inline-start: calc(0px - 20px);
}

:host(:not(:disabled):hover) #thumb {
  inline-size: ${MATERIAL_THUMB_SIZE};
}

/*
 * Ripple effect
 *
 * Translucent circle is painted on the thumb if the element is :active or
 * :focus-visible.  It has
 *   - Size transition when it appears
 *   - Opacity transition when it disappears
 * part(thumb)::before represents the former, and part(thumb)::after represents
 * the latter.
 */
#thumb::before {
  background: ${RIPPLE_COLOR};
  block-size: 0px;
  border-radius: 0px;
  content: "";
  display: inline-block;
  inline-size: 0px;
  left: calc(${MATERIAL_THUMB_SIZE} / 2);
  position: relative;
  top: calc(${MATERIAL_THUMB_SIZE} / 2);
  transition: none;
}

:host(:active) #thumb::before,
:host(:focus-visible) #thumb::before {
  block-size: ${RIPPLE_MAX_SIZE};
  border-radius: calc(${RIPPLE_MAX_SIZE} / 2);
  inline-size: ${RIPPLE_MAX_SIZE};
  left: calc((${MATERIAL_THUMB_SIZE} - ${RIPPLE_MAX_SIZE}) / 2);
  top: calc((${MATERIAL_THUMB_SIZE} - ${RIPPLE_MAX_SIZE}) / 2);
  transition: all linear 0.1s;
}

#thumb::after {
  background: ${RIPPLE_COLOR};
  block-size: ${RIPPLE_MAX_SIZE};
  border-radius: calc(${RIPPLE_MAX_SIZE} / 2);
  content: "";
  display: inline-block;
  inline-size: ${RIPPLE_MAX_SIZE};
  left: calc((${MATERIAL_THUMB_SIZE} - ${RIPPLE_MAX_SIZE}) / 2);
  opacity: 0;
  position: relative;
  /* Why 18px? */
  top: calc((${MATERIAL_THUMB_SIZE} - ${RIPPLE_MAX_SIZE}) / 2 - 18px);
  transition: opacity linear 0.3s;
}

:host(:active) #thumb::after,
:host(:focus-visible) #thumb::after {
  block-size: 0px;
  content: "";
  inline-size: 0px;
  opacity: 1;
  transition: none;
}

`);
    }
    return styleSheet;
  };
}

/**
 * Add '$part-transitioning' part to the element if the element already has
 * a part name.
 *
 * TODO(tkent): We should apply custom state.
 *
 * @param {!Element} element
 */
export function markTransition(element) {
  // Should check hasAttribute() to avoid creating a DOMTokenList instance.
  if (!element.hasAttribute('part') || element.part.length < 1) {
    return;
  }
  element.part.add(element.part[0] + '-transitioning');
}

/**
 * Remove '$part-transitioning' part from the element if the element already has
 * a part name.
 *
 * TODO(tkent): We should apply custom state.
 *
 * @param {!Element} element
 */
export function unmarkTransition(element) {
  // Should check hasAttribute() to avoid creating a DOMTokenList instance.
  if (!element.hasAttribute('part') || element.part.length < 1) {
    return;
  }
  element.part.remove(element.part[0] + '-transitioning');
}
