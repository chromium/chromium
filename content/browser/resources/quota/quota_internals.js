// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {QuotaInternalsHandler, QuotaInternalsHandlerRemote} from './quota_internals.mojom-webui.js';

function RenderData(response) {
  const rowTemplate = /** @type {!HTMLTemplateElement} */ ($('listener-row'));
  const tableBody = /** @type {!HTMLTableElement} */ ($('listeners-tbody'));
  const listenerRowTemplate =
      /** @type {!HTMLTemplateElement} */ (rowTemplate.cloneNode(true));
  const listenerRow = listenerRowTemplate.content;

  const availableSpaceBytes =
      parseFloat(Number(response.availableSpace) / (1024 ** 3)).toFixed(2);
  const totalSpaceBytes =
      parseFloat(Number(response.totalSpace) / (1024 ** 3)).toFixed(2);

  tableBody.innerHTML = trustedTypes.emptyHTML;

  listenerRow.querySelector('.total-space').textContent =
      `${totalSpaceBytes} GB`;
  listenerRow.querySelector('.available-space').textContent =
      `${availableSpaceBytes} GB`;

  tableBody.append(listenerRow);
}

document.addEventListener('DOMContentLoaded', () => {
  /**
   * @type {!QuotaInternalsHandlerRemote}
   */
  const pageHandler = QuotaInternalsHandler.getRemote();

  pageHandler.getDiskAvailability().then((response) => {
    RenderData(response);
  });
});