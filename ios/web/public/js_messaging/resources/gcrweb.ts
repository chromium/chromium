// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {generateRandomId, sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview This file exports `gCrWeb` API to be used by other
 * files to augment its functionality. `gCrWeb` is intended
 * to be used as a bridge for native code to access JavaScript functionality.
 * The functions added to `gCrWeb` are not intended to be used within
 * other TypeScript files.
 */

class CrWeb {
  private readonly registeredApis: {[id: string]: CrWebApi} = {};
  private frameId: string = generateRandomId();

  /*
   * Register a Javascript API into the CrWeb object. In case
   * of any collision, do not override a pre-registered API.
   */
  registerApi(apiIdentifier: string, api: CrWebApi): void {
    if (this.registeredApis[apiIdentifier] !== undefined) {
      throw new Error(`API ${apiIdentifier} already registered.`);
    }
    this.registeredApis[apiIdentifier] = api;
  }

  getRegisteredApi(apiIdentifier: string): CrWebApi|undefined {
    return this.registeredApis[apiIdentifier];
  }

  /**
   * Returns the frameId associated with this frame. A new value will be created
   * for this frame the first time it is called. The frameId will persist as
   * long as this JavaScript context lives. For example, the frameId will be the
   * same when navigating 'back' to this frame.
   */
  getFrameId(): string {
    if (!this.frameId) {
      this.frameId = generateRandomId();
    }
    return this.frameId;
  }

  /**
   * Registers and frame by sending its frameId to the native application.
   */
  registerFrame() {
    if (!document.documentElement) {
      // Prevent registering frames if there is no document element created.
      // This is true when rendering non-web content, such as PDFs.
      return;
    }

    sendWebKitMessage(
      'FrameBecameAvailable', {'crwFrameId': this.getFrameId()});
  }
}

export class CrWebApi {
  private readonly contents: {[id: string]: unknown} = {};

  addFunction(name: string, func: Function): void {
    this.contents[name] = func;
  }

  addProperty(name: string, property: unknown): void {
    this.contents[name] = property;
  }

  getFunction(name: string): Function|null {
    if (typeof this.contents[name] === 'function') {
      return this.contents[name];
    }
    return null;
  }

  getProperty(name: string): unknown {
    return this.contents[name];
  }
}

type CrWebType = Window&(typeof globalThis)&{__gCrWeb: CrWeb};

// Initializes window's `__gCrWeb` property. Without this step,
// the window's `__gCrWeb` property cannot be found.
if (!(window as CrWebType).__gCrWeb) {
  (window as CrWebType).__gCrWeb = new CrWeb();
}

export const gCrWebLegacy: any = (window as CrWebType).__gCrWeb;
export const gCrWeb: CrWeb = (window as CrWebType).__gCrWeb;
