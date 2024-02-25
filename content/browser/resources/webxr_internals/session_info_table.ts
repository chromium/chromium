// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './session_info_table.html.js';
import * as TimeUtil from './time_util.js';
import type {SessionRejectedRecord, SessionRequestedRecord, SessionStartedRecord, SessionStoppedRecord} from './webxr_internals.mojom-webui.js';
import * as XRRuntimeUtil from './xr_runtime_util.js';
import * as XRSessionUtil from './xr_session_util.js';

const COLUMN_NAMES = [
  'Trace ID',
  'Session State',
  'Attributes',
];

export class SessionInfoTableElement extends CustomElement {
  private traceIdToTableCell: {[key: string]: HTMLTableCellElement} = {};

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();

    const table =
        this.getRequiredElement<HTMLTableElement>('#session-info-table');

    const headerRow = table.insertRow();
    COLUMN_NAMES.forEach((columnName) => {
      const headerCell = document.createElement('th');
      headerCell.textContent = columnName;
      headerRow.appendChild(headerCell);
    });
  }

  addSessionRequestedRow(sessionRequestedRecord: SessionRequestedRecord) {
    const {options, requestedTime} = sessionRequestedRecord;
    const {traceId, mode, requiredFeatures, optionalFeatures, depthOptions} =
        options;

    const attributes = [
      `Mode: ${XRSessionUtil.sessionModeToString(mode)}`,
      `Required Features: ${
          requiredFeatures.map(XRSessionUtil.sessionFeatureToString)
              .join(', ')}`,
      `Optional Features: ${
          optionalFeatures.map(XRSessionUtil.sessionFeatureToString)
              .join(', ')}`,
      `Requested Time: ${TimeUtil.formatMojoTime(requestedTime)}`,
    ];

    const depthUsagePreferences = depthOptions?.usagePreferences || [];
    if (depthUsagePreferences.length) {
      attributes.push(`Depth Usage Preferences: ${
          depthUsagePreferences.map(XRSessionUtil.depthUsageToString)
              .join(', ')}`);
    }

    const depthDataFormatPreferences =
        depthOptions?.dataFormatPreferences || [];
    if (depthDataFormatPreferences.length) {
      attributes.push(`Depth Data Format Preferences: ${
          depthDataFormatPreferences.map(XRSessionUtil.depthFormatToString)
              .join(', ')}`);
    }

    this.addSessionRow(traceId.toString(), 'Requested', attributes);
  }

  addSessionRejectedRow(sessionRejectedRecord: SessionRejectedRecord) {
    const {
      traceId,
      failureReason,
      failureReasonDescription,
      rejectedFeatures,
      rejectedTime,
    } = sessionRejectedRecord;

    const rejectedFeaturesDescription = rejectedFeatures.length ?
        ` rejectedFeatures=${
            rejectedFeatures.map(XRSessionUtil.sessionFeatureToString)
                .join(', ')}` :
        '';
    const attributes = [
      `Failure Reason: ${
          XRSessionUtil.requestSessionErrorToString(failureReason)}`,
      `Failure Reason Description: ${failureReasonDescription} ${
          rejectedFeaturesDescription}`,
      `Rejected Time: ${TimeUtil.formatMojoTime(rejectedTime)}`,
    ];

    this.addSessionRow(traceId.toString(), 'Rejected', attributes);
  }

  addSessionStartedRow(sessionStartedRecord: SessionStartedRecord) {
    const {traceId, deviceId, startedTime} = sessionStartedRecord;
    const attributes = [
      `Device Id: ${XRRuntimeUtil.deviceIdToString(deviceId)}`,
      `Started Time: ${TimeUtil.formatMojoTime(startedTime)}`,
    ];

    this.addSessionRow(traceId.toString(), 'Started', attributes);
  }

  addSessionStoppedRow(sessionStoppedRecord: SessionStoppedRecord) {
    const {traceId, stoppedTime} = sessionStoppedRecord;
    const attributes = [
      `Stopped Time: ${TimeUtil.formatMojoTime(stoppedTime)}`,
    ];

    this.addSessionRow(traceId.toString(), 'Stopped', attributes);
  }

  private createTableCell(textContent: string = ''): HTMLTableCellElement {
    const cell = document.createElement('td');
    cell.textContent = textContent;
    return cell;
  }

  private createAttributesList(attributes: string[]): HTMLUListElement {
    const ul = document.createElement('ul');
    ul.style.padding = '0';

    attributes.forEach((attribute) => {
      const li = document.createElement('li');
      li.textContent = attribute;
      ul.appendChild(li);
    });

    return ul;
  }

  private updateTraceIdCell(traceId: string, newRow: HTMLTableRowElement) {
    let traceIdCell = this.traceIdToTableCell[traceId];

    if (traceIdCell === undefined) {
      traceIdCell = this.createTableCell(traceId);
      newRow.appendChild(traceIdCell);
      traceIdCell.rowSpan = 1;
      this.traceIdToTableCell[traceId] = traceIdCell;
    } else {
      traceIdCell.rowSpan++;
    }
  }

  private addSessionRow(
      traceId: string, sessionType: string, attributes: string[]) {
    const table =
        this.getRequiredElement<HTMLTableElement>('#session-info-table');
    const newRow = table.insertRow();

    this.updateTraceIdCell(traceId, newRow);

    const sessionTypeCell = this.createTableCell(sessionType);
    newRow.appendChild(sessionTypeCell);

    const attributesCell = this.createTableCell();
    const ul = this.createAttributesList(attributes);
    attributesCell.appendChild(ul);
    newRow.appendChild(attributesCell);
  }
}

// Declare the custom element
declare global {
  interface HTMLElementTagNameMap {
    'session-info-table': SessionInfoTableElement;
  }
}

customElements.define('session-info-table', SessionInfoTableElement);
