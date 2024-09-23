// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './trace_report.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ClientTraceReport} from './trace_report.mojom-webui.js';
import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {getCss} from './trace_report_list.css.js';
import {getHtml} from './trace_report_list.html.js';
// clang-format on

export enum NotificationType {
  UPDATE = 'Update',
  ERROR = 'Error',
  ANNOUNCEMENT = 'Announcement'
}

export class Notification {
  readonly type: NotificationType;
  readonly label: string;

  constructor(type: NotificationType, label: string) {
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
      traces_: {type: Array},
      isLoading_: {type: Boolean},
      notification: {type: Notification},
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();
  protected traces_: ClientTraceReport[] = [];
  protected isLoading_: boolean = false;
  protected notification_?: Readonly<Notification>;

  override connectedCallback(): void {
    super.connectedCallback();
    this.initializeList(true);
  }

  protected async initializeList(hasLoading: boolean = false): Promise<void> {
    this.isLoading_ = hasLoading;
    const {reports} = await this.traceReportProxy_.handler.getAllTraceReports();
    if (reports) {
      this.traces_ = reports;
    } else {
      this.traces_ = [];
      this.notification_ = new Notification(
          NotificationType.ERROR,
          'Error: Could not retrieve any trace reports.');
      this.$.toast.show();
    }
    this.isLoading_ = false;
  }

  protected hasTraces_(): boolean {
    return this.traces_.length > 0;
  }

  protected showToastHandler_(e: CustomEvent<Notification>): void {
    assert(e.detail);
    this.notification_ = e.detail;
    this.$.toast.show();
  }

  protected getNotificationIcon_(): string {
    switch (this.getNotificationType_()) {
      case NotificationType.ANNOUNCEMENT:
        return 'cr:info-outline';
      case NotificationType.ERROR:
        return 'cr:error-outline';
      case NotificationType.UPDATE:
        return 'cr:sync';
      default:
        return 'cr:warning';
    }
  }

  protected getNotificationStyling_(): string {
    switch (this.getNotificationType_()) {
      case NotificationType.ANNOUNCEMENT:
        return 'announcement';
      case NotificationType.ERROR:
        return 'error';
      case NotificationType.UPDATE:
        return 'update';
      default:
        return '';
    }
  }

  protected getNotificationLabel_(): string {
    return this.notification_?.label || '';
  }

  protected getNotificationType_(): string {
    return this.notification_?.type || '';
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
      detail: new Notification(NotificationType.ERROR, message),
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report-list': TraceReportListElement;
  }
}

customElements.define(TraceReportListElement.is, TraceReportListElement);
