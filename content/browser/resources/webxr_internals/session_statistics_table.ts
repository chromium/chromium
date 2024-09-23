// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './session_statistics_table.html.js';
import type {XrFrameStatistics, XrLogMessage} from './xr_session.mojom-webui.js';


const COLUMN_NAMES = ['Logs'];

export class SessionStatisticsTable extends CustomElement {
  textLines: string[];
  totalDuration: bigint;
  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();

    this.totalDuration = 0n;
    this.textLines = [COLUMN_NAMES.join(', ')];
    const table =
        this.getRequiredElement<HTMLTableElement>('#session-statistics-table');

    const headerRow = table.insertRow();
    COLUMN_NAMES.forEach((columnName) => {
      const headerCell = document.createElement('th');
      headerCell.textContent = columnName;
      headerRow.appendChild(headerCell);
    });


    // Add event listener to the button
    const button = this.getRequiredElement<HTMLTableElement>('#copy-button');
    button.addEventListener('click', () => {
      this.copyToClipboard();
    });
  }

  addXrSessionStatisticsRow(xrSessionStatistics: XrFrameStatistics) {
    const durationInMilliseconds =
        xrSessionStatistics.duration.microseconds / 1000n;
    this.totalDuration += durationInMilliseconds;
    const durationInSeconds = durationInMilliseconds / 1000n;

    const fps = xrSessionStatistics.numFrames / durationInSeconds;
    const droppedFrames = xrSessionStatistics.droppedFrames / durationInSeconds;
    const frameDataTime = this.getDisplayMillisecondsFromMicroSeconds(
        xrSessionStatistics.frameDataTime.microseconds);
    const animationFrameTime = this.getDisplayMillisecondsFromMicroSeconds(
        xrSessionStatistics.pageAnimationFrameTime.microseconds);
    const submitFrameTime = this.getDisplayMillisecondsFromMicroSeconds(
        xrSessionStatistics.submitFrameTime.microseconds);
    const cellValues = [
      `${this.totalDuration}`,
      `${fps}`,
      `${droppedFrames}`,
      `${frameDataTime}`,
      `${animationFrameTime}`,
      `${submitFrameTime}`,
    ];

    this.textLines.push(cellValues.join(', '));

    const cellValuesString = `Duration:${this.totalDuration}ms, Frame Rate:${
        fps}, Dropped Frames:${droppedFrames}, Frame Data Time:${
        frameDataTime}ms/frame, Animation Frame Time:${
        animationFrameTime}ms/frame, Submit Frame Time:${
        submitFrameTime}ms/frame`;
    this.addRow([cellValuesString]);
  }

  addRow(cellValues: string[]) {
    const table =
        this.getRequiredElement<HTMLTableElement>('#session-statistics-table');
    const newRow = table.insertRow();

    cellValues.forEach((value) => {
      const cell = newRow.insertCell();
      cell.textContent = value;
    });
  }

  // Method to copy textLines to clipboard
  async copyToClipboard(): Promise<void> {
    const textToCopy = this.textLines.join('\n');
    await navigator.clipboard.writeText(textToCopy);
  }

  addConsoleMessageRow(xrLogMessage: XrLogMessage) {
    const message = xrLogMessage.message;
    this.addRow([message]);
  }

  // Method to convert microseconds to milliseconds and round to 2 decimal
  // places and return  it as a string
  getDisplayMillisecondsFromMicroSeconds(time: bigint): string {
    const timeInMilliseconds = Number(time) / 1000;
    return timeInMilliseconds.toFixed(2);
  }
}

// Declare the custom element
declare global {
  interface HTMLElementTagNameMap {
    'session-statistics-table': SessionStatisticsTable;
  }
}

customElements.define('session-statistics-table', SessionStatisticsTable);
