// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import './trace_report_list.js';
import './tracing_scenarios_config.js';

import {CrRouter} from '//resources/js/cr_router.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

interface Tab {
  name: string;
  path: string;
}

export class TraceReportAppElement extends CrLitElement {
  static get is() {
    return 'trace-report-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      // The index of the currently selected page.
      selected_: {type: Number},
      tabNames_: {type: Array},
    };
  }

  private tabs: Tab[] = [
    {
      name: 'Reports',
      path: '',
    },
    {
      name: 'Scenarios',
      path: 'scenarios',
    },
  ];
  protected selected_: number = 0;
  protected tabNames_: string[] = this.tabs.map(tab => tab.name);

  override firstUpdated() {
    const router = CrRouter.getInstance();
    this.pathChanged_(router.getPath());
    router.addEventListener(
        'cr-router-path-changed',
        (e: Event) => this.pathChanged_((e as CustomEvent<string>).detail));
  }

  /** Updates the location hash on selection change. */
  protected onSelectedChanged_(e: CustomEvent<{value: number}>) {
    if (e === undefined || e.detail.value === this.selected_) {
      return;
    }
    this.selected_ = e.detail.value;
    const newTab = this.tabs[this.selected_];

    if (newTab === undefined) {
      return;
    }

    const tabPath = newTab.path;
    CrRouter.getInstance().setPath(`/${tabPath}`);
  }

  /**
   * Returns the index of the currently selected tab corresponding to the
   * path or zero if no match.
   */
  protected selectedFromPath_(path: string): number {
    const index = this.tabs.findIndex(tab => path === tab.path);
    return Math.max(index, 0);
  }

  /** Updates the selection property on path change. */
  private pathChanged_(newValue: string, _oldValue?: string) {
    this.selected_ = this.selectedFromPath_(newValue.substr(1));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report-app': TraceReportAppElement;
  }
}

customElements.define(TraceReportAppElement.is, TraceReportAppElement);
