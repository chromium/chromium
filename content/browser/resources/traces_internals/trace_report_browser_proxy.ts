/* Copyright 2023 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {PageCallbackRouter, PageHandlerRemote, TraceReportHandlerFactory} from './trace_report.mojom-webui.js';

/** Holds Mojo interfaces for communication with the browser process. */
export class TraceReportBrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = TraceReportHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): TraceReportBrowserProxy {
    return instance || (instance = new TraceReportBrowserProxy());
  }

  static setInstance(obj: TraceReportBrowserProxy): void {
    instance = obj;
  }
}

let instance: TraceReportBrowserProxy|null = null;
