// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TraceReportAppElement} from './app.js';

export function getHtml(this: TraceReportAppElement) {
  // clang-format off
  return html`
  <cr-tabs .tabNames="${this.tabNames_}" .selected="${this.selected_}"
      @selected-changed="${this.onSelectedChanged_}">
  </cr-tabs>

  <cr-page-selector .selected="${this.selected_}">
    <trace-report-list></trace-report-list>
    <tracing-scenarios-config></tracing-scenarios-config>
  </cr-page-selector>`;
  // clang-format on
}
