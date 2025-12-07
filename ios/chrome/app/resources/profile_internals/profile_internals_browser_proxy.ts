// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface ProfileAttributes {
  gaiaId: string;
  userName: string;
  hasAuthenticationError: boolean;
  attachedGaiaIds: string[];
  lastActiveTime: string;
}

export interface ProfileState {
  profileName: string;
  isPersonalProfile: boolean;
  isCurrentProfile: boolean;
  isLoaded: boolean;
  isNewProfile: boolean;
  isFullyInitialized: boolean;
  attributes: ProfileAttributes;
}

/**
 * @fileoverview A helper object used by the profile internals debug page
 * to interact with the browser.
 */
export interface ProfileInternalsBrowserProxy {
  getProfilesList(): Promise<ProfileState[]>;
}

export class ProfileInternalsBrowserProxyImpl implements
    ProfileInternalsBrowserProxy {
  getProfilesList(): Promise<ProfileState[]> {
    return sendWithPromise('getProfilesList');
  }

  static getInstance(): ProfileInternalsBrowserProxy {
    return instance || (instance = new ProfileInternalsBrowserProxyImpl());
  }

  static setInstance(obj: ProfileInternalsBrowserProxy) {
    instance = obj;
  }
}

let instance: ProfileInternalsBrowserProxy|null = null;
