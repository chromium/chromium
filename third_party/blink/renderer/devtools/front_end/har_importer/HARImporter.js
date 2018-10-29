// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

HARImporter.Importer = class {
  /**
   * @param {!HARImporter.HARLog} log
   * @return {!Array<!SDK.NetworkRequest>}
   */
  static requestsFromHARLog(log) {
    /** @type {!Map<string, !HARImporter.HARPage>} */
    const pages = new Map();
    for (const page of log.pages)
      pages.set(page.id, page);

    log.entries.sort((a, b) => a.startedDateTime - b.startedDateTime);

    /** @type {!Map<string, !SDK.NetworkLog.PageLoad>} */
    const pageLoads = new Map();
    /** @type {!Array<!SDK.NetworkRequest>} */
    const requests = [];
    for (const entry of log.entries) {
      let pageLoad = pageLoads.get(entry.pageref);
      const documentURL = pageLoad ? pageLoad.mainRequest.url() : entry.request.url;

      let initiator = null;
      if (entry._initiator) {
        initiator = {
          type: entry._initiator.type,
          url: entry._initiator.url,
          lineNumber: entry._initiator.lineNumber
        };
      }

      const request = new SDK.NetworkRequest(
          'har-' + requests.length, entry.request.url, documentURL, '', '', initiator);
      const page = pages.get(entry.pageref);
      if (!pageLoad && page) {
        pageLoad = HARImporter.Importer._buildPageLoad(page, request);
        pageLoads.set(entry.pageref, pageLoad);
      }
      HARImporter.Importer._fillRequestFromHAREntry(request, entry, pageLoad);
      if (pageLoad)
        pageLoad.bindRequest(request);
      requests.push(request);
    }
    return requests;
  }

  /**
   * @param {!HARImporter.HARPage} page
   * @param {!SDK.NetworkRequest} mainRequest
   * @return {!SDK.NetworkLog.PageLoad}
   */
  static _buildPageLoad(page, mainRequest) {
    const pageLoad = new SDK.NetworkLog.PageLoad(mainRequest);
    pageLoad.startTime = page.startedDateTime;
    pageLoad.contentLoadTime = page.pageTimings.onContentLoad * 1000;
    pageLoad.loadTime = page.pageTimings.onLoad * 1000;
    return pageLoad;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {!HARImporter.HAREntry} entry
   * @param {?SDK.NetworkLog.PageLoad} pageLoad
   */
  static _fillRequestFromHAREntry(request, entry, pageLoad) {
    // Request data.
    if (entry.request.postData)
      request.setRequestFormData(true, entry.request.postData.text);
    else
      request.setRequestFormData(false, null);
    request.connectionId = entry.connection || '';
    request.requestMethod = entry.request.method;
    request.setRequestHeaders(entry.request.headers);

    // Response data.
    if (entry.response.content.mimeType && entry.response.content.mimeType !== 'x-unknown')
      request.mimeType = entry.response.content.mimeType;
    request.responseHeaders = entry.response.headers;
    request.statusCode = entry.response.status;
    request.statusText = entry.response.statusText;
    let protocol = entry.response.httpVersion.toLowerCase();
    if (protocol === 'http/2.0')
      protocol = 'h2';
    request.protocol = protocol.replace(/^http\/2\.0?\+quic/, 'http/2+quic');

    // Timing data.
    const issueTime = entry.startedDateTime.getTime() / 1000;
    request.setIssueTime(issueTime, issueTime);

    // Content data.
    const contentSize = entry.response.content.size > 0 ? entry.response.content.size : 0;
    const headersSize = entry.response.headersSize > 0 ? entry.response.headersSize : 0;
    const bodySize = entry.response.bodySize > 0 ? entry.response.bodySize : 0;
    request.resourceSize = contentSize || (headersSize + bodySize);
    let transferSize = entry.response.customAsNumber('transferSize');
    if (transferSize === undefined)
      transferSize = entry.response.headersSize + entry.response.bodySize;
    request.setTransferSize(transferSize >= 0 ? transferSize : 0);

    const fromCache = entry.customAsString('fromCache');
    if (fromCache === 'memory')
      request.setFromMemoryCache();
    else if (fromCache === 'disk')
      request.setFromDiskCache();

    const contentData = {error: null, content: null, encoded: entry.response.content.encoding === 'base64'};
    if (entry.response.content.text !== undefined)
      contentData.content = entry.response.content.text;
    request.setContentDataProvider(async () => contentData);

    // Timing data.
    HARImporter.Importer._setupTiming(request, issueTime, entry.time, entry.timings);

    // Meta data.
    request.setRemoteAddress(entry.serverIPAddress || '', 80);  // Har does not support port numbers.
    let resourceType = (pageLoad && pageLoad.mainRequest === request) ?
        Common.resourceTypes.Document :
        Common.ResourceType.fromMimeType(entry.response.content.mimeType);
    if (!resourceType)
      resourceType = Common.ResourceType.fromURL(entry.request.url) || Common.resourceTypes.Other;
    request.setResourceType(resourceType);

    const priority = entry.customAsString('priority');
    if (Protocol.Network.ResourcePriority.hasOwnProperty(priority))
      request.setPriority(/** @type {!Protocol.Network.ResourcePriority} */ (priority));

    request.finished = true;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {number} issueTime
   * @param {number} entryTotalDuration
   * @param {!HARImporter.HARTimings} timings
   */
  static _setupTiming(request, issueTime, entryTotalDuration, timings) {
    /**
     * @param {number|undefined} timing
     * @return {number}
     */
    function accumulateTime(timing) {
      if (timing === undefined || timing < 0)
        return -1;
      lastEntry += timing;
      return lastEntry;
    }
    let lastEntry = timings.blocked >= 0 ? timings.blocked : 0;

    const proxy = timings.customAsNumber('blocked_proxy') || -1;
    const queueing = timings.customAsNumber('blocked_queueing') || -1;

    // SSL is part of connect for both HAR and Chrome's format so subtract it here.
    const ssl = timings.ssl >= 0 ? timings.ssl : 0;
    if (timings.connect > 0)
      timings.connect -= ssl;
    const timing = {
      proxyStart: proxy > 0 ? lastEntry - proxy : -1,
      proxyEnd: proxy > 0 ? lastEntry : -1,
      requestTime: issueTime + (queueing > 0 ? queueing : 0) / 1000,
      dnsStart: timings.dns >= 0 ? lastEntry : -1,
      dnsEnd: accumulateTime(timings.dns),

      // Add ssl to end time without modifying lastEntry (see comment above).
      connectStart: timings.connect >= 0 ? lastEntry : -1,
      connectEnd: accumulateTime(timings.connect) + ssl,

      // Now update lastEntry to add ssl timing back in (see comment above).
      sslStart: timings.ssl >= 0 ? lastEntry : -1,
      sslEnd: accumulateTime(timings.ssl),

      workerStart: -1,
      workerReady: -1,
      sendStart: timings.send >= 0 ? lastEntry : -1,
      sendEnd: accumulateTime(timings.send),
      pushStart: 0,
      pushEnd: 0,
      receiveHeadersEnd: accumulateTime(timings.wait)
    };
    accumulateTime(timings.receive);

    request.timing = timing;
    request.endTime = issueTime + Math.max(entryTotalDuration, lastEntry) / 1000;
  }
};
