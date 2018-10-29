// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Components.ImagePreview = class {
  /**
   * @param {!SDK.Target} target
   * @param {string} originalImageURL
   * @param {boolean} showDimensions
   * @param {!Object=} precomputedFeatures
   * @return {!Promise<?Element>}
   */
  static build(target, originalImageURL, showDimensions, precomputedFeatures) {
    const resourceTreeModel = target.model(SDK.ResourceTreeModel);
    if (!resourceTreeModel)
      return Promise.resolve(/** @type {?Element} */ (null));
    let resource = resourceTreeModel.resourceForURL(originalImageURL);
    let imageURL = originalImageURL;
    if (!isImageResource(resource) && precomputedFeatures && precomputedFeatures.currentSrc) {
      imageURL = precomputedFeatures.currentSrc;
      resource = resourceTreeModel.resourceForURL(imageURL);
    }
    if (!isImageResource(resource))
      return Promise.resolve(/** @type {?Element} */ (null));

    let fulfill;
    const promise = new Promise(x => fulfill = x);
    const imageElement = createElement('img');
    imageElement.addEventListener('load', buildContent, false);
    imageElement.addEventListener('error', () => fulfill(null), false);
    resource.populateImageSource(imageElement);
    return promise;

    /**
     * @param {?SDK.Resource} resource
     * @return {boolean}
     */
    function isImageResource(resource) {
      return !!resource && resource.resourceType() === Common.resourceTypes.Image;
    }

    function buildContent() {
      const container = createElement('table');
      UI.appendStyle(container, 'components/imagePreview.css');
      container.className = 'image-preview-container';
      const intrinsicWidth = imageElement.naturalWidth;
      const intrinsicHeight = imageElement.naturalHeight;
      const renderedWidth = precomputedFeatures ? precomputedFeatures.renderedWidth : intrinsicWidth;
      const renderedHeight = precomputedFeatures ? precomputedFeatures.renderedHeight : intrinsicHeight;
      let description;
      if (showDimensions) {
        description = ls`${renderedWidth} \xd7 ${renderedHeight} pixels`;
        if (renderedHeight !== intrinsicHeight || renderedWidth !== intrinsicWidth)
          description += ls` (intrinsic: ${intrinsicWidth} \xd7 ${intrinsicHeight} pixels)`;
      }

      container.createChild('tr').createChild('td', 'image-container').appendChild(imageElement);
      if (description)
        container.createChild('tr').createChild('td').createChild('span', 'description').textContent = description;
      if (imageURL !== originalImageURL) {
        container.createChild('tr').createChild('td').createChild('span', 'description').textContent =
            String.sprintf('currentSrc: %s', imageURL.trimMiddle(100));
      }
      fulfill(container);
    }
  }

  /**
   * @param {!SDK.DOMNode} node
   * @return {!Promise<!Object|undefined>}
   */
  static async loadDimensionsForNode(node) {
    if (!node.nodeName() || node.nodeName().toLowerCase() !== 'img')
      return;

    const object = await node.resolveToObject('');

    if (!object)
      return;

    const featuresObject = object.callFunctionJSON(features, undefined);
    object.release();
    return featuresObject;

    /**
     * @return {!{renderedWidth: number, renderedHeight: number, currentSrc: (string|undefined)}}
     * @suppressReceiverCheck
     * @this {!Element}
     */
    function features() {
      return {renderedWidth: this.width, renderedHeight: this.height, currentSrc: this.currentSrc};
    }
  }
};
