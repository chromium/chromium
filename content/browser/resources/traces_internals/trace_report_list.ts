// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './trace_report.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ClientTraceReport} from './trace_report.mojom-webui.js';
import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {getCss} from './trace_report_list.css.js';
import {getHtml} from './trace_report_list.html.js';
// clang-format on

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

export class TraceReportListElement extends CrLitElement {
  static get is() {
    return 'trace-report-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      traces: {
        type: Array,
      },
      isLoading: {
        type: Boolean,
      },
      notification: {
        type: Notification,
      },
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();
  protected traces: ClientTraceReport[] = [];
  protected isLoading: boolean = false;
  protected notification: Readonly<Notification>;

  override connectedCallback(): void {
    super.connectedCallback();
    this.initializeList(true);
  }

  protected async initializeList(hasLoading: boolean = false): Promise<void> {
    this.isLoading = hasLoading;
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

  protected showToastHandler_(e: CustomEvent<Notification>): void {
    assert(e.detail);
    this.notification = e.detail;
    this.$.toast.show();
  }

  protected getNotificationIcon_(): string {
    if (this.notification === undefined) {
      return '';
    }
    switch (this.notification.type) {
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

  protected getNotificationStyling_(): string {
    if (this.notification === undefined) {
      return '';
    }
    switch (this.notification.type) {
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

  protected hasTraces_(): boolean {
    return this.traces.length > 0;
  }

  protected async onDeleteAllTracesClick_(): Promise<void> {
    const {success} = await this.traceReportProxy_.handler.deleteAllTraces();
    if (!success) {
      this.dispatchToast_('Failed to delete to delete all traces.');
    }
    this.initializeList();
  }

  private dispatchToast_(message: string): void {
    this.dispatchEvent(new CustomEvent('show-toast', {
      bubbles: true,
      composed: true,
      detail: new Notification(NotificationTypeEnum.ERROR, message),
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report-list': TraceReportListElement;
  }
}

customElements.define(TraceReportListElement.is, TraceReportListElement);
