// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchAndReportErrors} from '//ios/web/public/js_messaging/resources/error_reporting.js';
import {CrWebError} from '//ios/web/public/js_messaging/resources/gcrweb_error.js';
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

  constructor() {
    const crweb = new CrWebApi();
    crweb.addFunction('getFrameId', this.getFrameId.bind(this));
    crweb.addFunction('registerFrame', this.registerFrame.bind(this));
    this.registerApi('crweb', crweb);
  }

  /*
   * Register a Javascript API into the CrWeb object. In case
   * of any collision, do not override a pre-registered API.
   */
  registerApi(apiIdentifier: string, api: CrWebApi): void {
    if (this.registeredApis[apiIdentifier] !== undefined) {
      throw new CrWebError(`API ${apiIdentifier} already registered.`);
    }
    this.registeredApis[apiIdentifier] = api;
  }

  // TODO(crbug.com/399666983): Throw an exception when deprecating legacy
  // version.
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

  // TODO(crbug.com/399666983): Remove legacy API handling
  /**
   * Interface to convert actual calls from the native side into
   * new CrWeb calls or call legacy code for that function or property.
   * @param apiName can be undefined.
   */
  callFunctionInGcrWeb(
      apiName: string, funcOrPropName: string, args: unknown[]): unknown {
    try {
      const registeredApi = gCrWeb.getRegisteredApi(apiName);
      if (registeredApi) {
        if (registeredApi.hasFunction(funcOrPropName)) {
          const func = registeredApi.getFunction(funcOrPropName);
          return func(...args);
        }
        return registeredApi.getProperty(funcOrPropName);
      }
      if (apiName === '') {
        return gCrWebLegacy[funcOrPropName](...args);
      }
      return gCrWebLegacy[apiName][funcOrPropName](...args);
    } catch (error) {
      if (error instanceof CrWebError) {
        sendWebKitMessage(
            'WindowErrorResultHandler',
            {'message': error.message, 'is_crweb': true});
      }
    }
    return undefined;
  }
}

export class CrWebApi {
  private readonly functions: {[id: string]: Function} = {};
  private readonly properties: {[id: string]: unknown} = {};

  addFunction(funcName: string, func: Function): void {
    this.functions[funcName] = function(...args: unknown[]) {
      return catchAndReportErrors.apply(
        null, [/*crweb=*/ true, funcName, func, args]);
    };
  }

  addProperty(propertyName: string, property: unknown): void {
    this.properties[propertyName] = property;
  }

  getFunction(funcName: string): Function {
    if (!this.hasFunction(funcName)) {
      throw new CrWebError(`Function ${funcName} is not available.`);
    }
    return this.functions[funcName] as Function;
  }

  getProperty(propertyName: string): unknown {
    if (!this.hasProperty(propertyName)) {
      throw new CrWebError(`Property ${propertyName} is not available.`);
    }
    return this.properties[propertyName];
  }

  hasFunction(funcName: string): boolean {
    return funcName in this.functions;
  }

  hasProperty(propertyName: string): boolean {
    return propertyName in this.properties;
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
