// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/ios/web_ui.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './profile_internals_app.css.js';
import {getHtml} from './profile_internals_app.html.js';
import type {ProfileInternalsBrowserProxy, ProfileState} from './profile_internals_browser_proxy.js';
import {ProfileInternalsBrowserProxyImpl} from './profile_internals_browser_proxy.js';

interface ProfileStateElement {
  profileState: ProfileState;
  properties: string[];
}

export class ProfileInternalsAppElement extends CrLitElement {
  static get is() {
    return 'profile-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Profiles list supplied by ProfileInternalsBrowserProxy.
       */
      profilesList_: {type: Array},
    };
  }

  private profileInternalsBrowserProxy_: ProfileInternalsBrowserProxy =
      ProfileInternalsBrowserProxyImpl.getInstance();

  protected accessor profilesList_: ProfileStateElement[] = [];

  override async connectedCallback() {
    super.connectedCallback();

    const profilesList =
        await this.profileInternalsBrowserProxy_.getProfilesList();
    this.profilesList_ = profilesList.map(
        profile => ({
          profileState: profile,
          properties: this.getPropertiesArray_(profile),
        }));
  }

  private getPropertiesArray_(profile: ProfileState): string[] {
    const properties = [];
    if (profile.isPersonalProfile) {
      properties.push('personal');
    }
    if (profile.isCurrentProfile) {
      properties.push('current');
    }
    if (profile.isLoaded) {
      properties.push('loaded');
    }
    if (profile.isNewProfile) {
      properties.push('new');
    }
    if (!profile.isFullyInitialized) {
      properties.push('uninitialized');
    }
    return properties;
  }
}

customElements.define(
    ProfileInternalsAppElement.is, ProfileInternalsAppElement);
