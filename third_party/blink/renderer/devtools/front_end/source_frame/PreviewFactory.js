// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

SourceFrame.PreviewFactory = class {
  /**
   * @param {!Common.ContentProvider} provider
   * @param {string} mimeType
   * @returns {!Promise<?UI.Widget>}
   */
  static async createPreview(provider, mimeType) {
    let resourceType = Common.ResourceType.fromMimeType(mimeType);
    if (resourceType === Common.resourceTypes.Other)
      resourceType = provider.contentType();

    switch (resourceType) {
      case Common.resourceTypes.Image:
        return new SourceFrame.ImageView(mimeType, provider);
      case Common.resourceTypes.Font:
        return new SourceFrame.FontView(mimeType, provider);
    }

    let content = await provider.requestContent();
    if (!content)
      return new UI.EmptyWidget(Common.UIString('Nothing to preview'));

    if (await provider.contentEncoded())
      content = window.atob(content);

    const parsedXML = SourceFrame.XMLView.parseXML(content, mimeType);
    if (parsedXML)
      return SourceFrame.XMLView.createSearchableView(parsedXML);

    const jsonView = await SourceFrame.JSONView.createView(content);
    if (jsonView)
      return jsonView;

    if (resourceType.isTextType()) {
      const highlighterType =
          provider.contentType().canonicalMimeType() || mimeType.replace(/;.*/, '');  // remove charset
      return SourceFrame.ResourceSourceFrame.createSearchableView(
          provider, highlighterType, true /* autoPrettyPrint */);
    }

    return null;
  }
};
