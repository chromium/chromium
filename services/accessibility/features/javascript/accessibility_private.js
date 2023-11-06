// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * One AutoclickObserver is created the first time
 * chrome.accessibilityPrivate.onScrollableBoundsForPointRequested is
 * called. This allows the browser to send events to
 * accessibility service JS.
 * @implements {ax.mojom.AutoclickInterface}
 */
class AutoclickObserver {
  constructor(pendingReceiver, callback) {
    this.receiver_ = new ax.mojom.AutoclickReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
    this.callback_ = callback;
  }

  /** @override */
  requestScrollableBoundsForPoint(point) {
    if (this.callback_) {
      this.callback_(point);
    }
  }
}

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

    // Create AccessibilityPrivate's ChromeEvents.

    /** @public {ChromeEvent} */
    this.onScrollableBoundsForPointRequested = new ChromeEvent(() => {
      // Construct remote and Autoclick receiver on-demand.
      // This way other users of AccessibilityPrivate that do not need
      // Autoclick do not need to try to access ax.mojom.Autoclick*,
      // meaning we do not need to import the generated JS mojom bindings
      // when they are not used.
      if (!this.autoclickRemote_) {
        this.autoclickRemote_ = ax.mojom.AutoclickClient.getRemote();
        this.autoclickRemote_.bindAutoclick().then(bindAutoclickResult => {
          if (!bindAutoclickResult.autoclickReceiver) {
            console.error('autoclickReceiver was unexpectedly missing');
            return;
          }
          const autoclickObserver = new AutoclickObserver(
              bindAutoclickResult.autoclickReceiver, (point) => {
                this.onScrollableBoundsForPointRequested.callListeners(point);
              });
        });
      }
    });

    // Private members.

    this.userInterfaceRemote_ = null;
    this.autoclickRemote_ = null;
  }

  /**
   * Load user interface remote on-demand. Not every consumer of
   * AccessibilityPrivate will need this; this way we don't need
   * to load the generated JS bindings for UserInterface for
   * every consumer.
   * @return {!ax.mojom.UserInterfaceRemote}
   */
  getUserInterfaceRemote_() {
    if (!this.userInterfaceRemote_) {
      this.userInterfaceRemote_ = ax.mojom.UserInterface.getRemote();
    }
    return this.userInterfaceRemote_;
  }

  /**
   * Called by Autoclick JS when onScrollableBoundsForPointRequested has found a
   * scrolling container. `rect` will be the bounds of the nearest scrollable
   * ancestor of the node at the point requested using
   * onScrollableBoundsForPointRequested.
   * @param {chrome.accessibilityPrivate.ScreenRect} rect
   */
  handleScrollableBoundsForPointFound(rect) {
    this.autoclickRemote_.handleScrollableBoundsForPointFound(
        AtpAccessibilityPrivate.convertRectToMojom_(rect));
  }

  /**
   * Darkens or undarkens the screen.
   * @param {boolean} darken
   */
  darkenScreen(darken) {
    this.getUserInterfaceRemote_().darkenScreen(darken);
  }

  /**
   * Opens a specified ChromeOS settings subpage. For example, to open a page
   * with the url 'chrome://settings/manageAccessibility/tts', pass in the
   * substring 'manageAccessibility/tts'.
   * @param {string} subpage
   */
  openSettingsSubpage(subpage) {
    this.getUserInterfaceRemote_().openSettingsSubpage(subpage);
  }

  /**
   * Shows a confirmation dialog.
   * @param {string} title The title of the confirmation dialog.
   * @param {string} description The description to show within the confirmation
   * dialog.
   * @param {string} cancelName The human-readable name of the cancel button.
   * @param {function(boolean): void} callback
   */
  showConfirmationDialog(title, description, cancelName, callback) {
    this.getUserInterfaceRemote_().showConfirmationDialog(title, description,
        cancelName).then(response => callback(response.confirmed));
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
        mojomFocusRing.rects =
            AtpAccessibilityPrivate.convertRectsToMojom_(focusRingInfo.rects);
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
        mojomFocusRing.color =
            AtpAccessibilityPrivate.convertColorToMojom_(focusRingInfo.color);
      }
      if (focusRingInfo.secondaryColor !== undefined) {
        mojomFocusRing.secondaryColor =
            AtpAccessibilityPrivate.convertColorToMojom_(
                focusRingInfo.secondaryColor);
      }
      if (focusRingInfo.backgroundColor !== undefined) {
        mojomFocusRing.backgroundColor =
            AtpAccessibilityPrivate.convertColorToMojom_(
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
    this.getUserInterfaceRemote_().setFocusRings(mojomFocusRings, mojomAtType);
  }

  /**
   * Sets the given focus highlights.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @param {string} color
   */
  setHighlights(rects, color) {
    const mojomColor = AtpAccessibilityPrivate.convertColorToMojom_(color);
    const mojomRects = AtpAccessibilityPrivate.convertRectsToMojom_(rects);
    this.getUserInterfaceRemote_().setHighlights(mojomRects, mojomColor);
  }

  /**
   * Shows or hides the virtual keyboard.
   * @param {boolean} is_visible
   */
  setVirtualKeyboardVisible(is_visible) {
    this.getUserInterfaceRemote_().setVirtualKeyboardVisible(is_visible);
  }

  /**
   * Convert array of accessibilityPrivate.ScreenRect to gfx.mojom.Rects.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @return {!Array<!gfx.mojom.Rect>}
   * @private
   */
  static convertRectsToMojom_(rects) {
    let mojomRects = [];
    for (const rect of rects) {
      mojomRects.push(AtpAccessibilityPrivate.convertRectToMojom_(rect));
    }
    return mojomRects;
  }

  /**
   * Convert an accessibilityPrivate.ScreenRect to gfx.mojom.Rect.
   * @param {chrome.accessibilityPrivate.ScreenRect} rect
   * @return {gfx.mojom.Rect}
   * @private
   */
  static convertRectToMojom_(rect) {
    let mojomRect = new gfx.mojom.Rect();
    mojomRect.x = rect.left;
    mojomRect.y = rect.top;
    mojomRect.width = rect.width;
    mojomRect.height = rect.height;
    return mojomRect;
  }

  /**
   * Converts a hex string to SkColor object.
   * @param {string} colorString
   * @return {skia.mojom.SkColor}
   * @private
   */
  static convertColorToMojom_(colorString) {
    let result = new skia.mojom.SkColor();
    if (colorString[0] === '#') {
      colorString = colorString.substr(1);
    }
    if (colorString.length === 6) {
      colorString = 'FF' + colorString;
    }
    result.value = parseInt(colorString, 16);
    return result;
  }
}

// Shim the accessibilityPrivate api onto the Chrome object to mimic
// chrome.accessibilityPrivate in extensions.
chrome.accessibilityPrivate = new AtpAccessibilityPrivate();
