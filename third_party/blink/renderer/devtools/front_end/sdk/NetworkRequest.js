/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @implements {Common.ContentProvider}
 * @unrestricted
 */
SDK.NetworkRequest = class extends Common.Object {
  /**
   * @param {!Protocol.Network.RequestId} requestId
   * @param {string} url
   * @param {string} documentURL
   * @param {!Protocol.Page.FrameId} frameId
   * @param {!Protocol.Network.LoaderId} loaderId
   * @param {?Protocol.Network.Initiator} initiator
   */
  constructor(requestId, url, documentURL, frameId, loaderId, initiator) {
    super();

    this._requestId = requestId;
    this._backendRequestId = requestId;
    this.setUrl(url);
    this._documentURL = documentURL;
    this._frameId = frameId;
    this._loaderId = loaderId;
    /** @type {?Protocol.Network.Initiator} */
    this._initiator = initiator;
    /** @type {?SDK.NetworkRequest} */
    this._redirectSource = null;
    /** @type {?SDK.NetworkRequest} */
    this._redirectDestination = null;
    this._issueTime = -1;
    this._startTime = -1;
    this._endTime = -1;
    /** @type {!Protocol.Network.BlockedReason|undefined} */
    this._blockedReason = undefined;

    this.statusCode = 0;
    this.statusText = '';
    this.requestMethod = '';
    this.requestTime = 0;
    this.protocol = '';
    /** @type {!Protocol.Security.MixedContentType} */
    this.mixedContentType = Protocol.Security.MixedContentType.None;

    /** @type {?Protocol.Network.ResourcePriority} */
    this._initialPriority = null;
    /** @type {?Protocol.Network.ResourcePriority} */
    this._currentPriority = null;

    /** @type {?Protocol.Network.SignedExchangeInfo} */
    this._signedExchangeInfo = null;

    /** @type {!Common.ResourceType} */
    this._resourceType = Common.resourceTypes.Other;
    /** @type {?Promise<!SDK.NetworkRequest.ContentData>} */
    this._contentData = null;
    /** @type {!Array.<!SDK.NetworkRequest.WebSocketFrame>} */
    this._frames = [];
    /** @type {!Array.<!SDK.NetworkRequest.EventSourceMessage>} */
    this._eventSourceMessages = [];

    /** @type {!Object<string, (string|undefined)>} */
    this._responseHeaderValues = {};
    this._responseHeadersText = '';

    /** @type {!Array<!SDK.NetworkRequest.NameValue>} */
    this._requestHeaders = [];
    /** @type {!Object<string, (string|undefined)>} */
    this._requestHeaderValues = {};

    this._remoteAddress = '';

    /** @type {?Protocol.Network.RequestReferrerPolicy} */
    this._referrerPolicy = null;

    /** @type {!Protocol.Security.SecurityState} */
    this._securityState = Protocol.Security.SecurityState.Unknown;
    /** @type {?Protocol.Network.SecurityDetails} */
    this._securityDetails = null;

    /** @type {string} */
    this.connectionId = '0';
    /** @type {?Promise<?Array.<!SDK.NetworkRequest.NameValue>>} */
    this._formParametersPromise = null;
    // Assume no body initially
    /** @type {?Promise<?string>} */
    this._requestFormDataPromise = /** @type {?Promise<?string>} */ (Promise.resolve(null));
  }

  /**
   * @param {!SDK.NetworkRequest} other
   * @return {number}
   */
  indentityCompare(other) {
    const thisId = this.requestId();
    const thatId = other.requestId();
    if (thisId > thatId)
      return 1;
    if (thisId < thatId)
      return -1;
    return 0;
  }

  /**
   * @return {!Protocol.Network.RequestId}
   */
  requestId() {
    return this._requestId;
  }

  /**
   * @return {!Protocol.Network.RequestId}
   */
  backendRequestId() {
    return this._backendRequestId;
  }

  /**
   * @return {string}
   */
  url() {
    return this._url;
  }

  /**
   * @return {boolean}
   */
  isBlobRequest() {
    return this._url.startsWith('blob:');
  }

  /**
   * @param {string} x
   */
  setUrl(x) {
    if (this._url === x)
      return;

    this._url = x;
    this._parsedURL = new Common.ParsedURL(x);
    delete this._queryString;
    delete this._parsedQueryParameters;
    delete this._name;
    delete this._path;
  }

  /**
   * @return {string}
   */
  get documentURL() {
    return this._documentURL;
  }

  get parsedURL() {
    return this._parsedURL;
  }

  /**
   * @return {!Protocol.Page.FrameId}
   */
  get frameId() {
    return this._frameId;
  }

  /**
   * @return {!Protocol.Network.LoaderId}
   */
  get loaderId() {
    return this._loaderId;
  }

  /**
   * @param {string} ip
   * @param {number} port
   */
  setRemoteAddress(ip, port) {
    this._remoteAddress = ip + ':' + port;
    this.dispatchEventToListeners(SDK.NetworkRequest.Events.RemoteAddressChanged, this);
  }

  /**
   * @return {string}
   */
  remoteAddress() {
    return this._remoteAddress;
  }

  /**
   * @param {!Protocol.Network.RequestReferrerPolicy} referrerPolicy
   */
  setReferrerPolicy(referrerPolicy) {
    this._referrerPolicy = referrerPolicy;
  }

  /**
   * @return {?Protocol.Network.RequestReferrerPolicy}
   */
  referrerPolicy() {
    return this._referrerPolicy;
  }

  /**
   * @return {!Protocol.Security.SecurityState}
   */
  securityState() {
    return this._securityState;
  }

  /**
   * @param {!Protocol.Security.SecurityState} securityState
   */
  setSecurityState(securityState) {
    this._securityState = securityState;
  }

  /**
   * @return {?Protocol.Network.SecurityDetails}
   */
  securityDetails() {
    return this._securityDetails;
  }

  /**
   * @param {!Protocol.Network.SecurityDetails} securityDetails
   */
  setSecurityDetails(securityDetails) {
    this._securityDetails = securityDetails;
  }

  /**
   * @return {number}
   */
  get startTime() {
    return this._startTime || -1;
  }

  /**
   * @param {number} monotonicTime
   * @param {number} wallTime
   */
  setIssueTime(monotonicTime, wallTime) {
    this._issueTime = monotonicTime;
    this._wallIssueTime = wallTime;
    this._startTime = monotonicTime;
  }

  /**
   * @return {number}
   */
  issueTime() {
    return this._issueTime;
  }

  /**
   * @param {number} monotonicTime
   * @return {number}
   */
  pseudoWallTime(monotonicTime) {
    return this._wallIssueTime ? this._wallIssueTime - this._issueTime + monotonicTime : monotonicTime;
  }

  /**
   * @return {number}
   */
  get responseReceivedTime() {
    return this._responseReceivedTime || -1;
  }

  /**
   * @param {number} x
   */
  set responseReceivedTime(x) {
    this._responseReceivedTime = x;
  }

  /**
   * @return {number}
   */
  get endTime() {
    return this._endTime || -1;
  }

  /**
   * @param {number} x
   */
  set endTime(x) {
    if (this.timing && this.timing.requestTime) {
      // Check against accurate responseReceivedTime.
      this._endTime = Math.max(x, this.responseReceivedTime);
    } else {
      // Prefer endTime since it might be from the network stack.
      this._endTime = x;
      if (this._responseReceivedTime > x)
        this._responseReceivedTime = x;
    }
    this.dispatchEventToListeners(SDK.NetworkRequest.Events.TimingChanged, this);
  }

  /**
   * @return {number}
   */
  get duration() {
    if (this._endTime === -1 || this._startTime === -1)
      return -1;
    return this._endTime - this._startTime;
  }

  /**
   * @return {number}
   */
  get latency() {
    if (this._responseReceivedTime === -1 || this._startTime === -1)
      return -1;
    return this._responseReceivedTime - this._startTime;
  }

  /**
   * @return {number}
   */
  get resourceSize() {
    return this._resourceSize || 0;
  }

  /**
   * @param {number} x
   */
  set resourceSize(x) {
    this._resourceSize = x;
  }

  /**
   * @return {number}
   */
  get transferSize() {
    return this._transferSize || 0;
  }

  /**
   * @param {number} x
   */
  increaseTransferSize(x) {
    this._transferSize = (this._transferSize || 0) + x;
  }

  /**
   * @param {number} x
   */
  setTransferSize(x) {
    this._transferSize = x;
  }

  /**
   * @return {boolean}
   */
  get finished() {
    return this._finished;
  }

  /**
   * @param {boolean} x
   */
  set finished(x) {
    if (this._finished === x)
      return;

    this._finished = x;

    if (x)
      this.dispatchEventToListeners(SDK.NetworkRequest.Events.FinishedLoading, this);
  }

  /**
   * @return {boolean}
   */
  get failed() {
    return this._failed;
  }

  /**
   * @param {boolean} x
   */
  set failed(x) {
    this._failed = x;
  }

  /**
   * @return {boolean}
   */
  get canceled() {
    return this._canceled;
  }

  /**
   * @param {boolean} x
   */
  set canceled(x) {
    this._canceled = x;
  }

  /**
   * @return {!Protocol.Network.BlockedReason|undefined}
   */
  blockedReason() {
    return this._blockedReason;
  }

  /**
   * @param {!Protocol.Network.BlockedReason} reason
   */
  setBlockedReason(reason) {
    this._blockedReason = reason;
  }

  /**
   * @return {boolean}
   */
  wasBlocked() {
    return !!this._blockedReason;
  }

  /**
   * @return {boolean}
   */
  cached() {
    return (!!this._fromMemoryCache || !!this._fromDiskCache) && !this._transferSize;
  }

  /**
   * @return {boolean}
   */
  cachedInMemory() {
    return !!this._fromMemoryCache && !this._transferSize;
  }

  setFromMemoryCache() {
    this._fromMemoryCache = true;
    delete this._timing;
  }

  setFromDiskCache() {
    this._fromDiskCache = true;
  }

  /**
   * @return {boolean}
   */
  get fetchedViaServiceWorker() {
    return this._fetchedViaServiceWorker;
  }

  /**
   * @param {boolean} x
   */
  set fetchedViaServiceWorker(x) {
    this._fetchedViaServiceWorker = x;
  }

  /**
   * @return {!Protocol.Network.ResourceTiming|undefined}
   */
  get timing() {
    return this._timing;
  }

  /**
   * @param {!Protocol.Network.ResourceTiming|undefined} timingInfo
   */
  set timing(timingInfo) {
    if (!timingInfo || this._fromMemoryCache)
      return;
    // Take startTime and responseReceivedTime from timing data for better accuracy.
    // Timing's requestTime is a baseline in seconds, rest of the numbers there are ticks in millis.
    this._startTime = timingInfo.requestTime;
    const headersReceivedTime = timingInfo.requestTime + timingInfo.receiveHeadersEnd / 1000.0;
    if ((this._responseReceivedTime || -1) < 0 || this._responseReceivedTime > headersReceivedTime)
      this._responseReceivedTime = headersReceivedTime;
    if (this._startTime > this._responseReceivedTime)
      this._responseReceivedTime = this._startTime;

    this._timing = timingInfo;
    this.dispatchEventToListeners(SDK.NetworkRequest.Events.TimingChanged, this);
  }

  /**
   * @return {string}
   */
  get mimeType() {
    return this._mimeType;
  }

  /**
   * @param {string} x
   */
  set mimeType(x) {
    this._mimeType = x;
  }

  /**
   * @return {string}
   */
  get displayName() {
    return this._parsedURL.displayName;
  }

  /**
   * @return {string}
   */
  name() {
    if (this._name)
      return this._name;
    this._parseNameAndPathFromURL();
    return this._name;
  }

  /**
   * @return {string}
   */
  path() {
    if (this._path)
      return this._path;
    this._parseNameAndPathFromURL();
    return this._path;
  }

  _parseNameAndPathFromURL() {
    if (this._parsedURL.isDataURL()) {
      this._name = this._parsedURL.dataURLDisplayName();
      this._path = '';
    } else if (this._parsedURL.isAboutBlank()) {
      this._name = this._parsedURL.url;
      this._path = '';
    } else {
      this._path = this._parsedURL.host + this._parsedURL.folderPathComponents;

      const networkManager = SDK.NetworkManager.forRequest(this);
      const inspectedURL = networkManager ? networkManager.target().inspectedURL().asParsedURL() : null;
      this._path = this._path.trimURL(inspectedURL ? inspectedURL.host : '');
      if (this._parsedURL.lastPathComponent || this._parsedURL.queryParams) {
        this._name =
            this._parsedURL.lastPathComponent + (this._parsedURL.queryParams ? '?' + this._parsedURL.queryParams : '');
      } else if (this._parsedURL.folderPathComponents) {
        this._name =
            this._parsedURL.folderPathComponents.substring(this._parsedURL.folderPathComponents.lastIndexOf('/') + 1) +
            '/';
        this._path = this._path.substring(0, this._path.lastIndexOf('/'));
      } else {
        this._name = this._parsedURL.host;
        this._path = '';
      }
    }
  }

  /**
   * @return {string}
   */
  get folder() {
    let path = this._parsedURL.path;
    const indexOfQuery = path.indexOf('?');
    if (indexOfQuery !== -1)
      path = path.substring(0, indexOfQuery);
    const lastSlashIndex = path.lastIndexOf('/');
    return lastSlashIndex !== -1 ? path.substring(0, lastSlashIndex) : '';
  }

  /**
   * @return {!Common.ResourceType}
   */
  resourceType() {
    return this._resourceType;
  }

  /**
   * @param {!Common.ResourceType} resourceType
   */
  setResourceType(resourceType) {
    this._resourceType = resourceType;
  }

  /**
   * @return {string}
   */
  get domain() {
    return this._parsedURL.host;
  }

  /**
   * @return {string}
   */
  get scheme() {
    return this._parsedURL.scheme;
  }

  /**
   * @return {?SDK.NetworkRequest}
   */
  redirectSource() {
    return this._redirectSource;
  }

  /**
   * @param {?SDK.NetworkRequest} originatingRequest
   */
  setRedirectSource(originatingRequest) {
    this._redirectSource = originatingRequest;
  }

  /**
   * @return {?SDK.NetworkRequest}
   */
  redirectDestination() {
    return this._redirectDestination;
  }

  /**
   * @param {?SDK.NetworkRequest} redirectDestination
   */
  setRedirectDestination(redirectDestination) {
    this._redirectDestination = redirectDestination;
  }

  /**
   * @return {!Array.<!SDK.NetworkRequest.NameValue>}
   */
  requestHeaders() {
    return this._requestHeaders;
  }

  /**
   * @param {!Array.<!SDK.NetworkRequest.NameValue>} headers
   */
  setRequestHeaders(headers) {
    this._requestHeaders = headers;
    delete this._requestCookies;

    this.dispatchEventToListeners(SDK.NetworkRequest.Events.RequestHeadersChanged);
  }

  /**
   * @return {string|undefined}
   */
  requestHeadersText() {
    return this._requestHeadersText;
  }

  /**
   * @param {string} text
   */
  setRequestHeadersText(text) {
    this._requestHeadersText = text;

    this.dispatchEventToListeners(SDK.NetworkRequest.Events.RequestHeadersChanged);
  }

  /**
   * @param {string} headerName
   * @return {string|undefined}
   */
  requestHeaderValue(headerName) {
    if (headerName in this._requestHeaderValues)
      return this._requestHeaderValues[headerName];
    this._requestHeaderValues[headerName] = this._computeHeaderValue(this.requestHeaders(), headerName);
    return this._requestHeaderValues[headerName];
  }

  /**
   * @return {!Array.<!SDK.Cookie>}
   */
  get requestCookies() {
    if (!this._requestCookies)
      this._requestCookies = SDK.CookieParser.parseCookie(this.requestHeaderValue('Cookie'));
    return this._requestCookies;
  }

  /**
   * @return {!Promise<?string>}
   */
  requestFormData() {
    if (!this._requestFormDataPromise)
      this._requestFormDataPromise = SDK.NetworkManager.requestPostData(this);
    return this._requestFormDataPromise;
  }

  /**
   * @param {boolean} hasData
   * @param {?string} data
   */
  setRequestFormData(hasData, data) {
    this._requestFormDataPromise = (hasData && data === null) ? null : Promise.resolve(data);
    this._formParametersPromise = null;
  }

  /**
   * @return {string}
   */
  _filteredProtocolName() {
    const protocol = this.protocol.toLowerCase();
    if (protocol === 'h2')
      return 'http/2.0';
    return protocol.replace(/^http\/2(\.0)?\+/, 'http/2.0+');
  }

  /**
   * @return {string}
   */
  requestHttpVersion() {
    const headersText = this.requestHeadersText();
    if (!headersText) {
      const version = this.requestHeaderValue('version') || this.requestHeaderValue(':version');
      if (version)
        return version;
      return this._filteredProtocolName();
    }
    const firstLine = headersText.split(/\r\n/)[0];
    const match = firstLine.match(/(HTTP\/\d+\.\d+)$/);
    return match ? match[1] : 'HTTP/0.9';
  }

  /**
   * @return {!Array.<!SDK.NetworkRequest.NameValue>}
   */
  get responseHeaders() {
    return this._responseHeaders || [];
  }

  /**
   * @param {!Array.<!SDK.NetworkRequest.NameValue>} x
   */
  set responseHeaders(x) {
    this._responseHeaders = x;
    delete this._sortedResponseHeaders;
    delete this._serverTimings;
    delete this._responseCookies;
    this._responseHeaderValues = {};

    this.dispatchEventToListeners(SDK.NetworkRequest.Events.ResponseHeadersChanged);
  }

  /**
   * @return {string}
   */
  get responseHeadersText() {
    return this._responseHeadersText;
  }

  /**
   * @param {string} x
   */
  set responseHeadersText(x) {
    this._responseHeadersText = x;

    this.dispatchEventToListeners(SDK.NetworkRequest.Events.ResponseHeadersChanged);
  }

  /**
   * @return {!Array.<!SDK.NetworkRequest.NameValue>}
   */
  get sortedResponseHeaders() {
    if (this._sortedResponseHeaders !== undefined)
      return this._sortedResponseHeaders;

    this._sortedResponseHeaders = this.responseHeaders.slice();
    this._sortedResponseHeaders.sort(function(a, b) {
      return a.name.toLowerCase().compareTo(b.name.toLowerCase());
    });
    return this._sortedResponseHeaders;
  }

  /**
   * @param {string} headerName
   * @return {string|undefined}
   */
  responseHeaderValue(headerName) {
    if (headerName in this._responseHeaderValues)
      return this._responseHeaderValues[headerName];
    this._responseHeaderValues[headerName] = this._computeHeaderValue(this.responseHeaders, headerName);
    return this._responseHeaderValues[headerName];
  }

  /**
   * @return {!Array.<!SDK.Cookie>}
   */
  get responseCookies() {
    if (!this._responseCookies)
      this._responseCookies = SDK.CookieParser.parseSetCookie(this.responseHeaderValue('Set-Cookie'));
    return this._responseCookies;
  }

  /**
   * @return {string|undefined}
   */
  responseLastModified() {
    return this.responseHeaderValue('last-modified');
  }

  /**
   * @return {?Array.<!SDK.ServerTiming>}
   */
  get serverTimings() {
    if (typeof this._serverTimings === 'undefined')
      this._serverTimings = SDK.ServerTiming.parseHeaders(this.responseHeaders);
    return this._serverTimings;
  }

  /**
   * @return {?string}
   */
  queryString() {
    if (this._queryString !== undefined)
      return this._queryString;

    let queryString = null;
    const url = this.url();
    const questionMarkPosition = url.indexOf('?');
    if (questionMarkPosition !== -1) {
      queryString = url.substring(questionMarkPosition + 1);
      const hashSignPosition = queryString.indexOf('#');
      if (hashSignPosition !== -1)
        queryString = queryString.substring(0, hashSignPosition);
    }
    this._queryString = queryString;
    return this._queryString;
  }

  /**
   * @return {?Array.<!SDK.NetworkRequest.NameValue>}
   */
  get queryParameters() {
    if (this._parsedQueryParameters)
      return this._parsedQueryParameters;
    const queryString = this.queryString();
    if (!queryString)
      return null;
    this._parsedQueryParameters = this._parseParameters(queryString);
    return this._parsedQueryParameters;
  }

  /**
   * @return {!Promise<?Array<!SDK.NetworkRequest.NameValue>>}
   */
  async _parseFormParameters() {
    const requestContentType = this.requestContentType();

    if (!requestContentType)
      return null;

    // Handling application/x-www-form-urlencoded request bodies.
    if (requestContentType.match(/^application\/x-www-form-urlencoded\s*(;.*)?$/i)) {
      const formData = await this.requestFormData();
      if (!formData)
        return null;

      return this._parseParameters(formData);
    }

    // Handling multipart/form-data request bodies.
    const multipartDetails = requestContentType.match(/^multipart\/form-data\s*;\s*boundary\s*=\s*(\S+)\s*$/);

    if (!multipartDetails)
      return null;

    const boundary = multipartDetails[1];
    if (!boundary)
      return null;

    const formData = await this.requestFormData();
    if (!formData)
      return null;

    return this._parseMultipartFormDataParameters(formData, boundary);
  }

  /**
   * @return {!Promise<?Array<!SDK.NetworkRequest.NameValue>>}
   */
  formParameters() {
    if (!this._formParametersPromise)
      this._formParametersPromise = this._parseFormParameters();
    return this._formParametersPromise;
  }

  /**
   * @return {string}
   */
  responseHttpVersion() {
    const headersText = this._responseHeadersText;
    if (!headersText) {
      const version = this.responseHeaderValue('version') || this.responseHeaderValue(':version');
      if (version)
        return version;
      return this._filteredProtocolName();
    }
    const firstLine = headersText.split(/\r\n/)[0];
    const match = firstLine.match(/^(HTTP\/\d+\.\d+)/);
    return match ? match[1] : 'HTTP/0.9';
  }

  /**
   * @param {string} queryString
   * @return {!Array.<!SDK.NetworkRequest.NameValue>}
   */
  _parseParameters(queryString) {
    function parseNameValue(pair) {
      const position = pair.indexOf('=');
      if (position === -1)
        return {name: pair, value: ''};
      else
        return {name: pair.substring(0, position), value: pair.substring(position + 1)};
    }
    return queryString.split('&').map(parseNameValue);
  }

  /**
   * Parses multipart/form-data; boundary=boundaryString request bodies -
   * --boundaryString
   * Content-Disposition: form-data; name="field-name"; filename="r.gif"
   * Content-Type: application/octet-stream
   *
   * optionalValue
   * --boundaryString
   * Content-Disposition: form-data; name="field-name-2"
   *
   * optionalValue2
   * --boundaryString--
   *
   * @param {string} data
   * @param {string} boundary
   * @return {!Array.<!SDK.NetworkRequest.NameValue>}
   */
  _parseMultipartFormDataParameters(data, boundary) {
    const sanitizedBoundary = boundary.escapeForRegExp();
    const keyValuePattern = new RegExp(
        // Header with an optional file name.
        '^\\r\\ncontent-disposition\\s*:\\s*form-data\\s*;\\s*name="([^"]*)"(?:\\s*;\\s*filename="([^"]*)")?' +
            // Optional secondary header with the content type.
            '(?:\\r\\ncontent-type\\s*:\\s*([^\\r\\n]*))?' +
            // Padding.
            '\\r\\n\\r\\n' +
            // Value
            '(.*)' +
            // Padding.
            '\\r\\n$',
        'is');
    const fields = data.split(new RegExp(`--${sanitizedBoundary}(?:--\s*$)?`, 'g'));
    return fields.reduce(parseMultipartField, []);

    /**
     * @param {!Array.<!SDK.NetworkRequest.NameValue>} result
     * @param {string} field
     * @return {!Array.<!SDK.NetworkRequest.NameValue>}
     */
    function parseMultipartField(result, field) {
      const [match, name, filename, contentType, value] = field.match(keyValuePattern) || [];

      if (!match)
        return result;

      const processedValue = (filename || contentType) ? ls`(binary)` : value;
      result.push({name, value: processedValue});

      return result;
    }
  }

  /**
   * @param {!Array.<!SDK.NetworkRequest.NameValue>} headers
   * @param {string} headerName
   * @return {string|undefined}
   */
  _computeHeaderValue(headers, headerName) {
    headerName = headerName.toLowerCase();

    const values = [];
    for (let i = 0; i < headers.length; ++i) {
      if (headers[i].name.toLowerCase() === headerName)
        values.push(headers[i].value);
    }
    if (!values.length)
      return undefined;
    // Set-Cookie values should be separated by '\n', not comma, otherwise cookies could not be parsed.
    if (headerName === 'set-cookie')
      return values.join('\n');
    return values.join(', ');
  }

  /**
   * @return {!Promise<!SDK.NetworkRequest.ContentData>}
   */
  contentData() {
    if (this._contentData)
      return this._contentData;
    if (this._contentDataProvider)
      this._contentData = this._contentDataProvider();
    else
      this._contentData = SDK.NetworkManager.requestContentData(this);
    return this._contentData;
  }

  /**
   * @param {function():!Promise<!SDK.NetworkRequest.ContentData>} dataProvider
   */
  setContentDataProvider(dataProvider) {
    console.assert(!this._contentData, 'contentData can only be set once.');
    this._contentDataProvider = dataProvider;
  }

  /**
   * @override
   * @return {string}
   */
  contentURL() {
    return this._url;
  }

  /**
   * @override
   * @return {!Common.ResourceType}
   */
  contentType() {
    return this._resourceType;
  }

  /**
   * @override
   * @return {!Promise<boolean>}
   */
  async contentEncoded() {
    return (await this.contentData()).encoded;
  }

  /**
   * @override
   * @return {!Promise<?string>}
   */
  async requestContent() {
    return (await this.contentData()).content;
  }

  /**
   * @override
   * @param {string} query
   * @param {boolean} caseSensitive
   * @param {boolean} isRegex
   * @return {!Promise<!Array<!Common.ContentProvider.SearchMatch>>}
   */
  async searchInContent(query, caseSensitive, isRegex) {
    if (!this._contentDataProvider)
      return SDK.NetworkManager.searchInRequest(this, query, caseSensitive, isRegex);

    const contentData = await this.contentData();
    let content = contentData.content;
    if (!content)
      return [];
    if (contentData.encoded)
      content = window.atob(content);
    return Common.ContentProvider.performSearchInContent(content, query, caseSensitive, isRegex);
  }

  /**
   * @return {boolean}
   */
  isHttpFamily() {
    return !!this.url().match(/^https?:/i);
  }

  /**
   * @return {string|undefined}
   */
  requestContentType() {
    return this.requestHeaderValue('Content-Type');
  }

  /**
   * @return {boolean}
   */
  hasErrorStatusCode() {
    return this.statusCode >= 400;
  }

  /**
   * @param {!Protocol.Network.ResourcePriority} priority
   */
  setInitialPriority(priority) {
    this._initialPriority = priority;
  }

  /**
   * @return {?Protocol.Network.ResourcePriority}
   */
  initialPriority() {
    return this._initialPriority;
  }

  /**
   * @param {!Protocol.Network.ResourcePriority} priority
   */
  setPriority(priority) {
    this._currentPriority = priority;
  }

  /**
   * @return {?Protocol.Network.ResourcePriority}
   */
  priority() {
    return this._currentPriority || this._initialPriority || null;
  }

  /**
   * @param {!Protocol.Network.SignedExchangeInfo} info
   */
  setSignedExchangeInfo(info) {
    this._signedExchangeInfo = info;
  }

  /**
   * @return {?Protocol.Network.SignedExchangeInfo}
   */
  signedExchangeInfo() {
    return this._signedExchangeInfo;
  }

  /**
   * @param {!Element} image
   */
  async populateImageSource(image) {
    const {content, encoded} = await this.contentData();
    let imageSrc = Common.ContentProvider.contentAsDataURL(content, this._mimeType, encoded);
    if (imageSrc === null && !this._failed) {
      const cacheControl = this.responseHeaderValue('cache-control') || '';
      if (!cacheControl.includes('no-cache'))
        imageSrc = this._url;
    }
    if (imageSrc !== null)
      image.src = imageSrc;
  }

  /**
   * @return {?Protocol.Network.Initiator}
   */
  initiator() {
    return this._initiator;
  }

  /**
   * @return {!Array.<!SDK.NetworkRequest.WebSocketFrame>}
   */
  frames() {
    return this._frames;
  }

  /**
   * @param {string} errorMessage
   * @param {number} time
   */
  addFrameError(errorMessage, time) {
    this._addFrame({
      type: SDK.NetworkRequest.WebSocketFrameType.Error,
      text: errorMessage,
      time: this.pseudoWallTime(time),
      opCode: -1,
      mask: false
    });
  }

  /**
   * @param {!Protocol.Network.WebSocketFrame} response
   * @param {number} time
   * @param {boolean} sent
   */
  addFrame(response, time, sent) {
    const type = sent ? SDK.NetworkRequest.WebSocketFrameType.Send : SDK.NetworkRequest.WebSocketFrameType.Receive;
    this._addFrame({
      type: type,
      text: response.payloadData,
      time: this.pseudoWallTime(time),
      opCode: response.opcode,
      mask: response.mask
    });
  }

  /**
   * @param {!SDK.NetworkRequest.WebSocketFrame} frame
   */
  _addFrame(frame) {
    this._frames.push(frame);
    this.dispatchEventToListeners(SDK.NetworkRequest.Events.WebsocketFrameAdded, frame);
  }

  /**
   * @return {!Array.<!SDK.NetworkRequest.EventSourceMessage>}
   */
  eventSourceMessages() {
    return this._eventSourceMessages;
  }

  /**
   * @param {number} time
   * @param {string} eventName
   * @param {string} eventId
   * @param {string} data
   */
  addEventSourceMessage(time, eventName, eventId, data) {
    const message = {time: this.pseudoWallTime(time), eventName: eventName, eventId: eventId, data: data};
    this._eventSourceMessages.push(message);
    this.dispatchEventToListeners(SDK.NetworkRequest.Events.EventSourceMessageAdded, message);
  }

  /**
   * @param {number} redirectCount
   */
  markAsRedirect(redirectCount) {
    this._requestId = `${this._backendRequestId}:redirected.${redirectCount}`;
  }

  /**
   * @param {string} requestId
   */
  setRequestIdForTest(requestId) {
    this._backendRequestId = requestId;
    this._requestId = requestId;
  }
};

/** @enum {symbol} */
SDK.NetworkRequest.Events = {
  FinishedLoading: Symbol('FinishedLoading'),
  TimingChanged: Symbol('TimingChanged'),
  RemoteAddressChanged: Symbol('RemoteAddressChanged'),
  RequestHeadersChanged: Symbol('RequestHeadersChanged'),
  ResponseHeadersChanged: Symbol('ResponseHeadersChanged'),
  WebsocketFrameAdded: Symbol('WebsocketFrameAdded'),
  EventSourceMessageAdded: Symbol('EventSourceMessageAdded')
};

/** @enum {string} */
SDK.NetworkRequest.InitiatorType = {
  Other: 'other',
  Parser: 'parser',
  Redirect: 'redirect',
  Script: 'script',
  Preload: 'preload',
  SignedExchange: 'signedExchange'
};

/** @typedef {!{name: string, value: string}} */
SDK.NetworkRequest.NameValue;

/** @enum {string} */
SDK.NetworkRequest.WebSocketFrameType = {
  Send: 'send',
  Receive: 'receive',
  Error: 'error'
};

/** @typedef {!{type: SDK.NetworkRequest.WebSocketFrameType, time: number, text: string, opCode: number, mask: boolean}} */
SDK.NetworkRequest.WebSocketFrame;

/** @typedef {!{time: number, eventName: string, eventId: string, data: string}} */
SDK.NetworkRequest.EventSourceMessage;

/** @typedef {!{error: ?string, content: ?string, encoded: boolean}} */
SDK.NetworkRequest.ContentData;
