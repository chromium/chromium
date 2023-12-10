// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './trace_report.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ClientTraceReport} from './trace_report.mojom-webui.js';
import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {getTemplate} from './trace_report_list.html.js';

export enum NotificationTypeEnum {
  UPDATE = 'Update',
  ERROR = 'Error',
  ANNOUNCEMENT = 'Announcement'
}

export class Notification {
  readonly type: NotificationTypeEnum;
  readonly label: string;
  readonly icon: string;
  readonly style: string;

  constructor(type: NotificationTypeEnum, label: string) {
    this.type = type;
    this.label = label;
  }
}

export interface TraceReportListElement {
  $: {
    toast: CrToastElement,
  };
}

export class TraceReportListElement extends PolymerElement {
  static get is() {
    return 'trace-report-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      traces: Array,
      isLoading: Boolean,
      notification: Notification,
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();
  private traces: ClientTraceReport[] = [];
  private isLoading: boolean = false;
  private notification: Readonly<Notification>;

  override connectedCallback(): void {
    super.connectedCallback();
    this.initializeList();
  }

  private async initializeList(): Promise<void> {
    this.isLoading = true;
    const {reports} = await this.traceReportProxy_.handler.getAllTraceReports();
    if (reports) {
      this.traces = reports;
    } else {
      this.traces = [];
      this.notification = new Notification(
          NotificationTypeEnum.ERROR,
          'Error: Could not retrieve any trace reports.');
      this.$.toast.show();
    }
    this.isLoading = false;
  }

  private showToastHandler_(e: CustomEvent<Notification>): void {
    assert(e.detail);
    this.notification = e.detail;
    this.$.toast.show();
  }

  private getNotificationIcon_(type: NotificationTypeEnum): string {
    switch (type) {
      case NotificationTypeEnum.ANNOUNCEMENT:
        return 'cr:info-outline';
      case NotificationTypeEnum.ERROR:
        return 'cr:error-outline';
      case NotificationTypeEnum.UPDATE:
        return 'cr:sync';
      default:
        return '';
    }
  }

  private getNotificationStyling_(type: NotificationTypeEnum): string {
    switch (type) {
      case NotificationTypeEnum.ANNOUNCEMENT:
        return 'announcement';
      case NotificationTypeEnum.ERROR:
        return 'error';
      case NotificationTypeEnum.UPDATE:
        return 'update';
      default:
        return '';
    }
  }

  private hasTraces_(traces: ClientTraceReport[]): boolean {
    return traces.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report-list': TraceReportListElement;
  }
}

customElements.define(TraceReportListElement.is, TraceReportListElement);
