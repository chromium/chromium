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
 * @unrestricted
 */
SourceFrame.ImageView = class extends UI.SimpleView {
  /**
   * @param {string} mimeType
   * @param {!Common.ContentProvider} contentProvider
   */
  constructor(mimeType, contentProvider) {
    super(Common.UIString('Image'));
    this.registerRequiredCSS('source_frame/imageView.css');
    this.element.classList.add('image-view');
    this._url = contentProvider.contentURL();
    this._parsedURL = new Common.ParsedURL(this._url);
    this._mimeType = mimeType;
    this._contentProvider = contentProvider;
    this._uiSourceCode = contentProvider instanceof Workspace.UISourceCode ?
        /** @type {!Workspace.UISourceCode} */ (contentProvider) :
        null;
    if (this._uiSourceCode) {
      this._uiSourceCode.addEventListener(
          Workspace.UISourceCode.Events.WorkingCopyCommitted, this._workingCopyCommitted, this);
      new UI.DropTarget(
          this.element, [UI.DropTarget.Type.ImageFile, UI.DropTarget.Type.URI], Common.UIString('Drop image file here'),
          this._handleDrop.bind(this));
    }
    this._sizeLabel = new UI.ToolbarText();
    this._dimensionsLabel = new UI.ToolbarText();
    this._mimeTypeLabel = new UI.ToolbarText(mimeType);
    this._container = this.element.createChild('div', 'image');
    this._imagePreviewElement = this._container.createChild('img', 'resource-image-view');
    this._imagePreviewElement.addEventListener('contextmenu', this._contextMenu.bind(this), true);
  }

  /**
   * @override
   * @return {!Array<!UI.ToolbarItem>}
   */
  syncToolbarItems() {
    return [
      this._sizeLabel, new UI.ToolbarSeparator(), this._dimensionsLabel, new UI.ToolbarSeparator(), this._mimeTypeLabel
    ];
  }

  /**
   * @override
   */
  wasShown() {
    this._updateContentIfNeeded();
  }

  /**
   * @override
   */
  disposeView() {
    if (this._uiSourceCode) {
      this._uiSourceCode.removeEventListener(
          Workspace.UISourceCode.Events.WorkingCopyCommitted, this._workingCopyCommitted, this);
    }
  }

  _workingCopyCommitted() {
    this._updateContentIfNeeded();
  }

  async _updateContentIfNeeded() {
    const content = await this._contentProvider.requestContent();
    if (this._cachedContent === content)
      return;

    const contentEncoded = await this._contentProvider.contentEncoded();
    this._cachedContent = content;
    let imageSrc = Common.ContentProvider.contentAsDataURL(content, this._mimeType, contentEncoded);
    if (content === null)
      imageSrc = this._url;
    const loadPromise = new Promise(x => this._imagePreviewElement.onload = x);
    this._imagePreviewElement.src = imageSrc;
    const size = content && !contentEncoded ? content.length : this._base64ToSize(content);
    this._sizeLabel.setText(Number.bytesToString(size));
    await loadPromise;
    this._dimensionsLabel.setText(
        Common.UIString('%d × %d', this._imagePreviewElement.naturalWidth, this._imagePreviewElement.naturalHeight));
  }

  /**
   * @param {?string} content
   * @return {number}
   */
  _base64ToSize(content) {
    if (!content)
      return 0;
    let size = content.length * 3 / 4;
    if (content[content.length - 1] === '=')
      size--;
    if (content.length > 1 && content[content.length - 2] === '=')
      size--;
    return size;
  }

  _contextMenu(event) {
    const contextMenu = new UI.ContextMenu(event);
    if (!this._parsedURL.isDataURL())
      contextMenu.clipboardSection().appendItem(Common.UIString('Copy image URL'), this._copyImageURL.bind(this));
    if (this._imagePreviewElement.src) {
      contextMenu.clipboardSection().appendItem(
          Common.UIString('Copy image as data URI'), this._copyImageAsDataURL.bind(this));
    }

    contextMenu.clipboardSection().appendItem(Common.UIString('Open image in new tab'), this._openInNewTab.bind(this));
    contextMenu.clipboardSection().appendItem(Common.UIString('Save\u2026'), this._saveImage.bind(this));
    contextMenu.show();
  }

  _copyImageAsDataURL() {
    InspectorFrontendHost.copyText(this._imagePreviewElement.src);
  }

  _copyImageURL() {
    InspectorFrontendHost.copyText(this._url);
  }

  _saveImage() {
    const link = createElement('a');
    link.download = this._parsedURL.displayName;
    link.href = this._url;
    link.click();
  }

  _openInNewTab() {
    InspectorFrontendHost.openInNewTab(this._url);
  }

  /**
   * @param {!DataTransfer} dataTransfer
   */
  async _handleDrop(dataTransfer) {
    const items = dataTransfer.items;
    if (!items.length || items[0].kind !== 'file')
      return;

    const entry = items[0].webkitGetAsEntry();
    const encoded = !entry.name.endsWith('.svg');
    entry.file(file => {
      const reader = new FileReader();
      reader.onloadend = () => {
        let result;
        try {
          result = /** @type {?string} */ (reader.result);
        } catch (e) {
          result = null;
          console.error('Can\'t read file: ' + e);
        }
        if (typeof result !== 'string')
          return;
        this._uiSourceCode.setContent(encoded ? btoa(result) : result, encoded);
      };
      if (encoded)
        reader.readAsBinaryString(file);
      else
        reader.readAsText(file);
    });
  }
};
