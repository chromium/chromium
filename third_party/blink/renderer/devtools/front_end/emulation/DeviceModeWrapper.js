// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
Emulation.DeviceModeWrapper = class extends UI.VBox {
  /**
   * @param {!Emulation.InspectedPagePlaceholder} inspectedPagePlaceholder
   */
  constructor(inspectedPagePlaceholder) {
    super();
    Emulation.DeviceModeView._wrapperInstance = this;
    this._inspectedPagePlaceholder = inspectedPagePlaceholder;
    /** @type {?Emulation.DeviceModeView} */
    this._deviceModeView = null;
    this._toggleDeviceModeAction = UI.actionRegistry.action('emulation.toggle-device-mode');
    const model = self.singleton(Emulation.DeviceModeModel);
    this._showDeviceModeSetting = model.enabledSetting();
    this._showDeviceModeSetting.setRequiresUserAction(!!Runtime.queryParam('hasOtherClients'));
    this._showDeviceModeSetting.addChangeListener(this._update.bind(this, false));
    SDK.targetManager.addModelListener(
        SDK.OverlayModel, SDK.OverlayModel.Events.ScreenshotRequested, this._screenshotRequestedFromOverlay, this);
    this._update(true);
  }

  _toggleDeviceMode() {
    this._showDeviceModeSetting.set(!this._showDeviceModeSetting.get());
  }

  /**
   * @param {boolean=} fullSize
   * @param {!Protocol.Page.Viewport=} clip
   * @return {boolean}
   */
  _captureScreenshot(fullSize, clip) {
    if (!this._deviceModeView)
      this._deviceModeView = new Emulation.DeviceModeView();
    this._deviceModeView.setNonEmulatedAvailableSize(this._inspectedPagePlaceholder.element);
    if (fullSize)
      this._deviceModeView.captureFullSizeScreenshot();
    else if (clip)
      this._deviceModeView.captureAreaScreenshot(clip);
    else
      this._deviceModeView.captureScreenshot();
    return true;
  }

  /**
   * @param {!Common.Event} event
   */
  _screenshotRequestedFromOverlay(event) {
    const clip = /** @type {!Protocol.Page.Viewport} */ (event.data);
    this._captureScreenshot(false, clip);
  }

  /**
   * @param {boolean} force
   */
  _update(force) {
    this._toggleDeviceModeAction.setToggled(this._showDeviceModeSetting.get());
    if (!force) {
      const showing = this._deviceModeView && this._deviceModeView.isShowing();
      if (this._showDeviceModeSetting.get() === showing)
        return;
    }

    if (this._showDeviceModeSetting.get()) {
      if (!this._deviceModeView)
        this._deviceModeView = new Emulation.DeviceModeView();
      this._deviceModeView.show(this.element);
      this._inspectedPagePlaceholder.clearMinimumSize();
      this._inspectedPagePlaceholder.show(this._deviceModeView.element);
    } else {
      if (this._deviceModeView)
        this._deviceModeView.detach();
      this._inspectedPagePlaceholder.restoreMinimumSize();
      this._inspectedPagePlaceholder.show(this.element);
    }
  }
};

/** @type {!Emulation.DeviceModeWrapper} */
Emulation.DeviceModeView._wrapperInstance;

/**
 * @implements {UI.ActionDelegate}
 * @unrestricted
 */
Emulation.DeviceModeWrapper.ActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    if (Emulation.DeviceModeView._wrapperInstance) {
      switch (actionId) {
        case 'emulation.capture-screenshot':
          return Emulation.DeviceModeView._wrapperInstance._captureScreenshot();

        case 'emulation.capture-node-screenshot': {
          const node = UI.context.flavor(SDK.DOMNode);
          if (!node)
            return true;
          async function captureClip() {
            const object = await node.resolveToObject();
            const result = await object.callFunction(function() {
              const rect = this.getBoundingClientRect();
              const docRect = this.ownerDocument.documentElement.getBoundingClientRect();
              return JSON.stringify({
                x: rect.left - docRect.left,
                y: rect.top - docRect.top,
                width: rect.width,
                height: rect.height,
                scale: 1
              });
            });
            const clip = /** @type {!Protocol.Page.Viewport} */ (JSON.parse(result.object.value));
            Emulation.DeviceModeView._wrapperInstance._captureScreenshot(false, clip);
          }
          captureClip();
          return true;
        }

        case 'emulation.capture-full-height-screenshot':
          return Emulation.DeviceModeView._wrapperInstance._captureScreenshot(true);

        case 'emulation.toggle-device-mode':
          Emulation.DeviceModeView._wrapperInstance._toggleDeviceMode();
          return true;
      }
    }
    return false;
  }
};
