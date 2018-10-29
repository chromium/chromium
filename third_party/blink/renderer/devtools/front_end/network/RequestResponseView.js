/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

Network.RequestResponseView = class extends UI.VBox {
  /**
   * @param {!SDK.NetworkRequest} request
   */
  constructor(request) {
    super();
    this.element.classList.add('request-view');
    /** @protected */
    this.request = request;
    /** @type {?Promise<!UI.Widget>} */
    this._contentViewPromise = null;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {!SDK.NetworkRequest.ContentData} contentData
   * @return {boolean}
   */
  static _hasTextContent(request, contentData) {
    const mimeType = request.mimeType;
    let resourceType = Common.ResourceType.fromMimeType(mimeType);
    if (resourceType === Common.resourceTypes.Other)
      resourceType = request.contentType();
    if (resourceType === Common.resourceTypes.Image)
      return mimeType.startsWith('image/svg');
    if (resourceType.isTextType())
      return true;
    if (contentData.error)
      return false;
    if (resourceType === Common.resourceTypes.Other)
      return !!contentData.content && !contentData.encoded;
    return false;
  }

  /**
   * @protected
   * @param {!SDK.NetworkRequest} request
   * @return {!Promise<?UI.SearchableView>}
   */
  static async sourceViewForRequest(request) {
    let sourceView = request[Network.RequestResponseView._sourceViewSymbol];
    if (sourceView !== undefined)
      return sourceView;

    const contentData = await request.contentData();
    if (!Network.RequestResponseView._hasTextContent(request, contentData)) {
      request[Network.RequestResponseView._sourceViewSymbol] = null;
      return null;
    }

    const highlighterType = request.resourceType().canonicalMimeType() || request.mimeType;
    sourceView = SourceFrame.ResourceSourceFrame.createSearchableView(request, highlighterType);
    request[Network.RequestResponseView._sourceViewSymbol] = sourceView;
    return sourceView;
  }

  /**
   * @override
   * @final
   */
  wasShown() {
    this._doShowPreview();
  }

  /**
   * @return {!Promise<!UI.Widget>}
   */
  _doShowPreview() {
    if (!this._contentViewPromise)
      this._contentViewPromise = this.showPreview();
    return this._contentViewPromise;
  }

  /**
   * @protected
   * @return {!Promise<!UI.Widget>}
   */
  async showPreview() {
    const responseView = await this.createPreview();
    responseView.show(this.element);
    return responseView;
  }

  /**
   * @protected
   * @return {!Promise<!UI.Widget>}
   */
  async createPreview() {
    const contentData = await this.request.contentData();
    const sourceView = await Network.RequestResponseView.sourceViewForRequest(this.request);
    if ((!contentData.content || !sourceView) && !contentData.error)
      return new UI.EmptyWidget(Common.UIString('This request has no response data available.'));
    if (contentData.content && sourceView)
      return sourceView;
    return new UI.EmptyWidget(Common.UIString('Failed to load response data'));
  }

  /**
   * @param {number} line
   */
  async revealLine(line) {
    const view = await this._doShowPreview();
    if (view instanceof SourceFrame.ResourceSourceFrame.SearchableContainer)
      view.revealPosition(line);
  }
};

Network.RequestResponseView._sourceViewSymbol = Symbol('RequestResponseSourceView');
