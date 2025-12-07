// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
// <if expr="is_ios">
// TODO(crbug.com/41173939): Remove this once injected by web. -->
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {WebuiUrlInfo} from './chrome_urls.mojom-webui.js';

export const INTERNAL_DEBUG_PAGES_HASH: string = 'internal-debug-pages';

export class ChromeUrlsAppElement extends CrLitElement {
  static get is() {
    return 'chrome-urls-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      debugPagesButtonDisabled_: {type: Boolean},
      internalUrlInfos_: {type: Array},
      webuiUrlInfos_: {type: Array},
      commandUrls_: {type: Array},
      internalUisEnabled_: {type: Boolean},
    };
  }

  protected accessor debugPagesButtonDisabled_: boolean = false;
  protected accessor webuiUrlInfos_: WebuiUrlInfo[] = [];
  protected accessor internalUrlInfos_: WebuiUrlInfo[] = [];
  protected accessor commandUrls_: Url[] = [];
  protected accessor internalUisEnabled_: boolean = false;
  protected tracker_: EventTracker = new EventTracker();

  // <if expr="is_ios">
  protected loadUrlsTimeout_: number|null = null;
  // </if>

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('internalUrlInfos_') &&
        this.internalUrlInfos_.length > 0) {
      this.onHashChanged_(CrRouter.getInstance().getHash());
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    this.onHashChanged_(CrRouter.getInstance().getHash());
    this.tracker_.add(
        CrRouter.getInstance(), 'cr-router-hash-changed',
        (e: Event) => this.onHashChanged_((e as CustomEvent<string>).detail));

    // Wait 10ms on iOS, because otherwise the message may get dropped on the
    // ground. See crbug.com/40894738. Short timeout here, because the usual
    // issue is setting up Mojo.
    // <if expr="is_ios">
    this.loadUrlsTimeout_ = setTimeout(() => this.onLoadUrlsTimeout_(), 10);
    // </if>
    // <if expr="not is_ios">
    this.fetchUrls_();
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.tracker_.removeAll();
    // <if expr="is_ios">
    if (this.loadUrlsTimeout_) {
      clearTimeout(this.loadUrlsTimeout_);
      this.loadUrlsTimeout_ = null;
    }
    // </if>
  }

  // <if expr="is_ios">
  private onLoadUrlsTimeout_() {
    // Set a longer timeout for retries, because the backend reply needs to come
    // back to clear the timeout in fetchUrls_().
    assert(this.loadUrlsTimeout_);
    clearTimeout(this.loadUrlsTimeout_);
    this.loadUrlsTimeout_ = setTimeout(() => this.onLoadUrlsTimeout_(), 100);
    this.fetchUrls_();
  }
  // </if>

  private fetchUrls_() {
    BrowserProxyImpl.getInstance().handler.getUrls().then(({urlsData}) => {
      // Since we use GURL on the C++ side, we need to remove the trailing
      // '/' here for nicer display.
      function getPrettyUrl(url: Url): Url {
        return {url: url.url.replace(/\/$/, '')};
      }
      urlsData.webuiUrls.forEach(info => {
        info.url = getPrettyUrl(info.url);
      });
      this.webuiUrlInfos_ = urlsData.webuiUrls.filter(info => !info.internal);
      this.internalUrlInfos_ = urlsData.webuiUrls.filter(info => info.internal);
      this.commandUrls_ = urlsData.commandUrls.map(url => getPrettyUrl(url));
      this.internalUisEnabled_ = urlsData.internalDebuggingUisEnabled;
      // <if expr="is_ios">
      if (this.loadUrlsTimeout_) {
        clearTimeout(this.loadUrlsTimeout_);
      }
      // </if>
    });
  }

  private onHashChanged_(hash: string) {
    if (hash !== INTERNAL_DEBUG_PAGES_HASH ||
        this.internalUrlInfos_.length === 0) {
      return;
    }

    const header =
        this.shadowRoot.querySelector<HTMLElement>('#internal-debugging-pages');
    assert(header);
    header.scrollIntoView(true);
  }

  protected getDebugPagesEnabledText_(): string {
    return this.internalUisEnabled_ ? 'enabled' : 'disabled';
  }

  protected getDebugPagesToggleButtonLabel_(): string {
    return this.internalUisEnabled_ ? 'Disable internal debugging pages' :
                                      'Enable internal debugging pages';
  }

  protected async onToggleDebugPagesClick_() {
    this.debugPagesButtonDisabled_ = true;
    const enabled = !this.internalUisEnabled_;
    await BrowserProxyImpl.getInstance().handler.setDebugPagesEnabled(enabled);
    this.internalUisEnabled_ = enabled;
    this.debugPagesButtonDisabled_ = false;
    const params = new URLSearchParams(window.location.search);
    const host = params.get('host');
    // If a host was provided, redirects to it when debug pages are enabled.
    if (enabled && host) {
      const hostUrl = new URL(host);
      if (this.internalUrlInfos_.some(
              info => info.url.url === hostUrl.origin)) {
        OpenWindowProxyImpl.getInstance().openUrl(host);
      }
    }
  }

  protected isInternalUiEnabled_(info: WebuiUrlInfo): boolean {
    return info.enabled && this.internalUisEnabled_;
  }

  protected isChromeUrlsUrl_(info: WebuiUrlInfo): boolean {
    return info.url.url === 'chrome://chrome-urls';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-urls-app': ChromeUrlsAppElement;
  }
}

customElements.define(ChromeUrlsAppElement.is, ChromeUrlsAppElement);
