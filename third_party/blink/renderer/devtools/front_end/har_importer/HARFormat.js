// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

HARImporter.HARBase = class {
  /**
   * @param {*} data
   */
  constructor(data) {
    if (!data || typeof data !== 'object')
      throw 'First parameter is expected to be an object';
  }

  /**
   * @param {*} data
   * @return {!Date}
   */
  static _safeDate(data) {
    const date = new Date(data);
    if (!Number.isNaN(date.getTime()))
      return date;
    throw 'Invalid date format';
  }

  /**
   * @param {*} data
   * @return {number}
   */
  static _safeNumber(data) {
    const result = Number(data);
    if (!Number.isNaN(result))
      return result;
    throw 'Casting to number results in NaN';
  }

  /**
   * @param {*} data
   * @return {number|undefined}
   */
  static _optionalNumber(data) {
    return data !== undefined ? HARImporter.HARBase._safeNumber(data) : undefined;
  }

  /**
   * @param {*} data
   * @return {string|undefined}
   */
  static _optionalString(data) {
    return data !== undefined ? String(data) : undefined;
  }

  /**
   * @param {string} name
   * @return {string|undefined}
   */
  customAsString(name) {
    // Har specification says starting with '_' is a custom property, but closure uses '_' as a private property.
    const value = /** @type {!Object} */ (this)['_' + name];
    return value !== undefined ? String(value) : undefined;
  }

  /**
   * @param {string} name
   * @return {number|undefined}
   */
  customAsNumber(name) {
    // Har specification says starting with '_' is a custom property, but closure uses '_' as a private property.
    let value = /** @type {!Object} */ (this)['_' + name];
    if (value === undefined)
      return;
    value = Number(value);
    if (Number.isNaN(value))
      return;
    return value;
  }
};

// Using any of these classes may throw.
HARImporter.HARRoot = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.log = new HARImporter.HARLog(data['log']);
  }
};

HARImporter.HARLog = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.version = String(data['version']);
    this.creator = new HARImporter.HARCreator(data['creator']);
    this.browser = data['browser'] ? new HARImporter.HARCreator(data['browser']) : undefined;
    this.pages = Array.isArray(data['pages']) ? data['pages'].map(page => new HARImporter.HARPage(page)) : [];
    if (!Array.isArray(data['entries']))
      throw 'log.entries is expected to be an array';
    this.entries = data['entries'].map(entry => new HARImporter.HAREntry(entry));
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARCreator = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.name = String(data['name']);
    this.version = String(data['version']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARPage = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.startedDateTime = HARImporter.HARBase._safeDate(data['startedDateTime']);
    this.id = String(data['id']);
    this.title = String(data['title']);
    this.pageTimings = new HARImporter.HARPageTimings(data['pageTimings']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARPageTimings = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.onContentLoad = HARImporter.HARBase._optionalNumber(data['onContentLoad']);
    this.onLoad = HARImporter.HARBase._optionalNumber(data['onLoad']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HAREntry = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.pageref = HARImporter.HARBase._optionalString(data['pageref']);
    this.startedDateTime = HARImporter.HARBase._safeDate(data['startedDateTime']);
    this.time = HARImporter.HARBase._safeNumber(data['time']);
    this.request = new HARImporter.HARRequest(data['request']);
    this.response = new HARImporter.HARResponse(data['response']);
    this.cache = {};  // Not yet implemented.
    this.timings = new HARImporter.HARTimings(data['timings']);
    this.serverIPAddress = HARImporter.HARBase._optionalString(data['serverIPAddress']);
    this.connection = HARImporter.HARBase._optionalString(data['connection']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);

    // Chrome specific.
    this._fromCache = HARImporter.HARBase._optionalString(data['_fromCache']);
    if (data['_initiator'])
      this._initiator = new HARImporter.HARInitiator(data['_initiator']);
    this._priority = HARImporter.HARBase._optionalString(data['_priority']);
  }
};

HARImporter.HARRequest = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.method = String(data['method']);
    this.url = String(data['url']);
    this.httpVersion = String(data['httpVersion']);
    this.cookies =
        Array.isArray(data['cookies']) ? data['cookies'].map(cookie => new HARImporter.HARCookie(cookie)) : [];
    this.headers =
        Array.isArray(data['headers']) ? data['headers'].map(header => new HARImporter.HARHeader(header)) : [];
    this.queryString =
        Array.isArray(data['queryString']) ? data['queryString'].map(qs => new HARImporter.HARQueryString(qs)) : [];
    this.postData = data['postData'] ? new HARImporter.HARPostData(data['postData']) : undefined;
    this.headersSize = HARImporter.HARBase._safeNumber(data['headersSize']);
    this.bodySize = HARImporter.HARBase._safeNumber(data['bodySize']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARResponse = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.status = HARImporter.HARBase._safeNumber(data['status']);
    this.statusText = String(data['statusText']);
    this.httpVersion = String(data['httpVersion']);
    this.cookies =
        Array.isArray(data['cookies']) ? data['cookies'].map(cookie => new HARImporter.HARCookie(cookie)) : [];
    this.headers =
        Array.isArray(data['headers']) ? data['headers'].map(header => new HARImporter.HARHeader(header)) : [];
    this.content = new HARImporter.HARContent(data['content']);
    this.redirectURL = String(data['redirectURL']);
    this.headersSize = HARImporter.HARBase._safeNumber(data['headersSize']);
    this.bodySize = HARImporter.HARBase._safeNumber(data['bodySize']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);

    // Chrome specific.
    this._transferSize = HARImporter.HARBase._optionalNumber(data['_transferSize']);
    this._error = HARImporter.HARBase._optionalString(data['_error']);
  }
};

HARImporter.HARCookie = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.name = String(data['name']);
    this.value = String(data['value']);
    this.path = HARImporter.HARBase._optionalString(data['path']);
    this.domain = HARImporter.HARBase._optionalString(data['domain']);
    this.expires = data['expires'] ? HARImporter.HARBase._safeDate(data['expires']) : undefined;
    this.httpOnly = data['httpOnly'] !== undefined ? !!data['httpOnly'] : undefined;
    this.secure = data['secure'] !== undefined ? !!data['secure'] : undefined;
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARHeader = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.name = String(data['name']);
    this.value = String(data['value']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARQueryString = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.name = String(data['name']);
    this.value = String(data['value']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARPostData = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.mimeType = String(data['mimeType']);
    this.params = Array.isArray(data['params']) ? data['params'].map(param => new HARImporter.HARParam(param)) : [];
    this.text = String(data['text']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARParam = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.name = String(data['name']);
    this.value = HARImporter.HARBase._optionalString(data['value']);
    this.fileName = HARImporter.HARBase._optionalString(data['fileName']);
    this.contentType = HARImporter.HARBase._optionalString(data['contentType']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARContent = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.size = HARImporter.HARBase._safeNumber(data['size']);
    this.compression = HARImporter.HARBase._optionalNumber(data['compression']);
    this.mimeType = String(data['mimeType']);
    this.text = HARImporter.HARBase._optionalString(data['text']);
    this.encoding = HARImporter.HARBase._optionalString(data['encoding']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);
  }
};

HARImporter.HARTimings = class extends HARImporter.HARBase {
  /**
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.blocked = HARImporter.HARBase._optionalNumber(data['blocked']);
    this.dns = HARImporter.HARBase._optionalNumber(data['dns']);
    this.connect = HARImporter.HARBase._optionalNumber(data['connect']);
    this.send = HARImporter.HARBase._safeNumber(data['send']);
    this.wait = HARImporter.HARBase._safeNumber(data['wait']);
    this.receive = HARImporter.HARBase._safeNumber(data['receive']);
    this.ssl = HARImporter.HARBase._optionalNumber(data['ssl']);
    this.comment = HARImporter.HARBase._optionalString(data['comment']);

    // Chrome specific.
    this._blocked_queueing = HARImporter.HARBase._optionalNumber(data['_blocked_queueing']);
    this._blocked_proxy = HARImporter.HARBase._optionalNumber(data['_blocked_proxy']);
  }
};

HARImporter.HARInitiator = class extends HARImporter.HARBase {
  /**
   * Based on Initiator defined in browser_protocol.pdl
   *
   * @param {*} data
   */
  constructor(data) {
    super(data);
    this.type = HARImporter.HARBase._optionalString(data['type']);
    this.url = HARImporter.HARBase._optionalString(data['url']);
    this.lineNumber = HARImporter.HARBase._optionalNumber(data['lineNumber']);
  }
};
