/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @implements {Common.ContentProvider}
 * @unrestricted
 */
SDK.Resource = class {
  /**
   * @param {!SDK.ResourceTreeModel} resourceTreeModel
   * @param {?SDK.NetworkRequest} request
   * @param {string} url
   * @param {string} documentURL
   * @param {!Protocol.Page.FrameId} frameId
   * @param {!Protocol.Network.LoaderId} loaderId
   * @param {!Common.ResourceType} type
   * @param {string} mimeType
   * @param {?Date} lastModified
   * @param {?number} contentSize
   */
  constructor(
      resourceTreeModel, request, url, documentURL, frameId, loaderId, type, mimeType, lastModified, contentSize) {
    this._resourceTreeModel = resourceTreeModel;
    this._request = request;
    this.url = url;
    this._documentURL = documentURL;
    this._frameId = frameId;
    this._loaderId = loaderId;
    this._type = type || Common.resourceTypes.Other;
    this._mimeType = mimeType;

    this._lastModified = lastModified && lastModified.isValid() ? lastModified : null;
    this._contentSize = contentSize;

    /** @type {?string} */ this._content;
    /** @type {boolean} */ this._contentEncoded;
    this._pendingContentCallbacks = [];
    if (this._request && !this._request.finished)
      this._request.addEventListener(SDK.NetworkRequest.Events.FinishedLoading, this._requestFinished, this);
  }

  /**
   * @return {?Date}
   */
  lastModified() {
    if (this._lastModified || !this._request)
      return this._lastModified;
    const lastModifiedHeader = this._request.responseLastModified();
    const date = lastModifiedHeader ? new Date(lastModifiedHeader) : null;
    this._lastModified = date && date.isValid() ? date : null;
    return this._lastModified;
  }

  /**
   * @return {?number}
   */
  contentSize() {
    if (typeof this._contentSize === 'number' || !this._request)
      return this._contentSize;
    return this._request.resourceSize;
  }

  /**
   * @return {?SDK.NetworkRequest}
   */
  get request() {
    return this._request;
  }

  /**
   * @return {string}
   */
  get url() {
    return this._url;
  }

  /**
   * @param {string} x
   */
  set url(x) {
    this._url = x;
    this._parsedURL = new Common.ParsedURL(x);
  }

  get parsedURL() {
    return this._parsedURL;
  }

  /**
   * @return {string}
   */
  get documentURL() {
    return this._documentURL;
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
   * @return {string}
   */
  get displayName() {
    return this._parsedURL.displayName;
  }

  /**
   * @return {!Common.ResourceType}
   */
  resourceType() {
    return this._request ? this._request.resourceType() : this._type;
  }

  /**
   * @return {string}
   */
  get mimeType() {
    return this._request ? this._request.mimeType : this._mimeType;
  }

  /**
   * @return {?string}
   */
  get content() {
    return this._content;
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
    if (this.resourceType() === Common.resourceTypes.Document && this.mimeType.indexOf('javascript') !== -1)
      return Common.resourceTypes.Script;
    return this.resourceType();
  }

  /**
   * @override
   * @return {!Promise<boolean>}
   */
  async contentEncoded() {
    await this.requestContent();
    return this._contentEncoded;
  }

  /**
   * @override
   * @return {!Promise<?string>}
   */
  requestContent() {
    if (typeof this._content !== 'undefined')
      return Promise.resolve(this._content);

    let callback;
    const promise = new Promise(fulfill => callback = fulfill);
    this._pendingContentCallbacks.push(callback);
    if (!this._request || this._request.finished)
      this._innerRequestContent();
    return promise;
  }

  /**
   * @return {string}
   */
  canonicalMimeType() {
    return this.contentType().canonicalMimeType() || this.mimeType;
  }

  /**
   * @override
   * @param {string} query
   * @param {boolean} caseSensitive
   * @param {boolean} isRegex
   * @return {!Promise<!Array<!Common.ContentProvider.SearchMatch>>}
   */
  async searchInContent(query, caseSensitive, isRegex) {
    if (!this.frameId)
      return [];
    if (this.request)
      return this.request.searchInContent(query, caseSensitive, isRegex);
    const result = await this._resourceTreeModel.target().pageAgent().searchInResource(
        this.frameId, this.url, query, caseSensitive, isRegex);
    return result || [];
  }

  /**
   * @param {!Element} image
   */
  async populateImageSource(image) {
    const content = await this.requestContent();
    const encoded = this._contentEncoded;
    image.src = Common.ContentProvider.contentAsDataURL(content, this._mimeType, encoded) || this._url;
  }

  _requestFinished() {
    this._request.removeEventListener(SDK.NetworkRequest.Events.FinishedLoading, this._requestFinished, this);
    if (this._pendingContentCallbacks.length)
      this._innerRequestContent();
  }

  async _innerRequestContent() {
    if (this._contentRequested)
      return;
    this._contentRequested = true;

    if (this.request) {
      const contentData = await this.request.contentData();
      this._content = contentData.content;
      this._contentEncoded = contentData.encoded;
    } else {
      const response = await this._resourceTreeModel.target().pageAgent().invoke_getResourceContent(
          {frameId: this.frameId, url: this.url});
      this._content = response[Protocol.Error] ? null : response.content;
      this._contentEncoded = response.base64Encoded;
    }

    if (this._content === null)
      this._contentEncoded = false;

    for (const callback of this._pendingContentCallbacks.splice(0))
      callback(this._content);
    delete this._contentRequested;
  }

  /**
   * @return {boolean}
   */
  hasTextContent() {
    if (this._type.isTextType())
      return true;
    if (this._type === Common.resourceTypes.Other)
      return !!this._content && !this._contentEncoded;
    return false;
  }

  /**
   * @return {!SDK.ResourceTreeFrame}
   */
  frame() {
    return this._resourceTreeModel.frameForId(this._frameId);
  }
};
