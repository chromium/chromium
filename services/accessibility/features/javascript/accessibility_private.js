// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Massages some mojo interfaces into the chrome.accessibilityPrivate extension
// API surface used by a11y component extensions.
// TODO(b/290971224): Compile and type-check this.
// TODO(b/266767235): Convert to typescript.
class AtpAccessibilityPrivate {
  /**
   * @enum {string}
   */
  AssistiveTechnologyType = {
    CHROME_VOX: 'chromeVox',
    SELECT_TO_SPEAK: 'selectToSpeak',
    SWITCH_ACCESS: 'switchAccess',
    AUTO_CLICK: 'autoClick',
    MAGNIFIER: 'magnifier',
    DICTATION: 'dictation',
  };

  /**
   * @enum {string}
   */
  FocusType = {
    GLOW: 'glow',
    SOLID: 'solid',
    DASHED: 'dashed',
  };

  /**
   * @enum {string}
   */
  FocusRingStackingOrder = {
    ABOVE_ACCESSIBILITY_BUBBLES: 'aboveAccessibilityBubbles',
    BELOW_ACCESSIBILITY_BUBBLES: 'belowAccessibilityBubbles',
  };

  constructor() {
    // This is a singleton.
    console.assert(!chrome.accessibilityPrivate);
    const UserInterfaceApi = ax.mojom.UserInterface;
    this.userInterfaceRemote_ = UserInterfaceApi.getRemote();
  }

  /**
   * Sets the given accessibility focus rings for this extension.
   * @param {!Array<!chrome.accessibilityPrivate.FocusRingInfo>} focusRings
   *     Array of focus rings to draw.
   * @param {chrome.accessibilityPrivate.FocusRingInfo} atType The assistive
   *     technology type of the feature using focus rings.
   */
  setFocusRings(focusRingInfos, atType) {
    let mojomFocusRings = [];
    let mojomAtType = ax.mojom.AssistiveTechnologyType.kUnknown;
    switch (atType) {
      case this.AssistiveTechnologyType.CHROME_VOX:
        mojomAtType = ax.mojom.AssistiveTechnologyType.kChromeVox;
        break;
      case this.AssistiveTechnologyType.SELECT_TO_SPEAK:
        mojomAtType = ax.mojom.AssistiveTechnologyType.kSelectToSpeak;
        break;
      case this.AssistiveTechnologyType.SWITCH_ACCESS:
        mojomAtType = ax.mojom.AssistiveTechnologyType.kSwitchAccess;
        break;
      case this.AssistiveTechnologyType.AUTO_CLICK:
        mojomAtType = ax.mojom.AssistiveTechnologyType.kAutoClick;
        break;
      case this.AssistiveTechnologyType.MAGNIFIER:
        mojomAtType = ax.mojom.AssistiveTechnologyType.kMagnifier;
        break;
      case this.AssistiveTechnologyType.DICTATION:
        mojomAtType = ax.mojom.AssistiveTechnologyType.kDictation;
        break;
      default:
        console.error('Unknown assistive technology type', atType);
        return;
    }
    for (let focusRingInfo of focusRingInfos) {
      let mojomFocusRing = new ax.mojom.FocusRingInfo();
      if (focusRingInfo.rects && focusRingInfo.rects.length) {
        mojomFocusRing.rects = [];
        for (let rect of focusRingInfo.rects) {
          let mojomRect = new gfx.mojom.Rect();
          mojomRect.x = rect.left;
          mojomRect.y = rect.top;
          mojomRect.width = rect.width;
          mojomRect.height = rect.height;
          mojomFocusRing.rects.push(mojomRect);
        }
      }
      if (focusRingInfo.type !== undefined) {
        switch (focusRingInfo.type) {
          case this.FocusType.GLOW:
            mojomFocusRing.type = ax.mojom.FocusType.kGlow;
            break;
          case this.FocusType.SOLID:
            mojomFocusRing.type = ax.mojom.FocusType.kSolid;
            break;
          case this.FocusType.DASHED:
            mojomFocusRing.type = ax.mojom.FocusType.kDashed;
            break;
          default:
            console.error('Unknown focus ring type', focusRingInfo.type);
        }
      }
      if (focusRingInfo.color !== undefined) {
        mojomFocusRing.color = new skia.mojom.SkColor();
        mojomFocusRing.color.value =
            AtpAccessibilityPrivate.parseHexColor_(focusRingInfo.color);
      }
      if (focusRingInfo.secondaryColor !== undefined) {
        mojomFocusRing.secondaryColor = new skia.mojom.SkColor();
        mojomFocusRing.secondaryColor.value =
            AtpAccessibilityPrivate.parseHexColor_(
                focusRingInfo.secondaryColor);
      }
      if (focusRingInfo.backgroundColor !== undefined) {
        mojomFocusRing.backgroundColor = new skia.mojom.SkColor();
        mojomFocusRing.backgroundColor.value =
            AtpAccessibilityPrivate.parseHexColor_(
                focusRingInfo.backgroundColor);
      }
      if (focusRingInfo.stackingOrder !== undefined) {
        switch (focusRingInfo.stackingOrder) {
          case this.FocusRingStackingOrder.ABOVE_ACCESSIBILITY_BUBBLES:
            mojomFocusRing.stackingOrder =
                ax.mojom.FocusRingStackingOrder.kAboveAccessibilityBubbles;
            break;
          case this.FocusRingStackingOrder.BELOW_ACCESSIBILITY_BUBBLES:
            mojomFocusRing.stackingOrder =
                ax.mojom.FocusRingStackingOrder.kBelowAccessibilityBubbles;
            break;
          default:
            console.error(
                'Unknown focus ring stacking order',
                focusRingInfo.stackingOrder);
        }
      }
      if (focusRingInfo.id !== undefined) {
        mojomFocusRing.id = focusRingInfo.id;
      }
      mojomFocusRings.push(mojomFocusRing)
    }
    this.userInterfaceRemote_.setFocusRings(mojomFocusRings, mojomAtType);
  }

  static parseHexColor_(colorString) {
    if (colorString[0] === '#') {
      colorString = colorString.substr(1);
    }
    return parseInt(colorString, 16);
  }
}

// Shim the accessibilityPrivate api onto the Chrome object to mimic
// chrome.accessibilityPrivate in extensions.
chrome.accessibilityPrivate = new AtpAccessibilityPrivate();
