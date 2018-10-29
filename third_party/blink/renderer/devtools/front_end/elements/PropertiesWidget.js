/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
Elements.PropertiesWidget = class extends UI.ThrottledWidget {
  constructor() {
    super(true /* isWebComponent */);
    this.registerRequiredCSS('elements/propertiesWidget.css');

    SDK.targetManager.addModelListener(SDK.DOMModel, SDK.DOMModel.Events.AttrModified, this._onNodeChange, this);
    SDK.targetManager.addModelListener(SDK.DOMModel, SDK.DOMModel.Events.AttrRemoved, this._onNodeChange, this);
    SDK.targetManager.addModelListener(
        SDK.DOMModel, SDK.DOMModel.Events.CharacterDataModified, this._onNodeChange, this);
    SDK.targetManager.addModelListener(
        SDK.DOMModel, SDK.DOMModel.Events.ChildNodeCountUpdated, this._onNodeChange, this);
    UI.context.addFlavorChangeListener(SDK.DOMNode, this._setNode, this);
    this._node = UI.context.flavor(SDK.DOMNode);
    this.update();
  }

  /**
   * @param {!Common.Event} event
   */
  _setNode(event) {
    this._node = /** @type {?SDK.DOMNode} */ (event.data);
    this.update();
  }

  /**
   * @override
   * @protected
   * @return {!Promise<undefined>}
   */
  async doUpdate() {
    if (this._lastRequestedNode) {
      this._lastRequestedNode.domModel().runtimeModel().releaseObjectGroup(Elements.PropertiesWidget._objectGroupName);
      delete this._lastRequestedNode;
    }

    if (!this._node) {
      this.contentElement.removeChildren();
      this.sections = [];
      return;
    }

    this._lastRequestedNode = this._node;
    const object = await this._node.resolveToObject(Elements.PropertiesWidget._objectGroupName);
    if (!object)
      return;

    const result = await object.callFunction(protoList);
    object.release();

    if (!result.object || result.wasThrown)
      return;

    const propertiesResult = await result.object.getOwnProperties(false /* generatePreview */);
    result.object.release();

    if (!propertiesResult || !propertiesResult.properties)
      return;

    const properties = propertiesResult.properties;
    const expanded = [];
    const sections = this.sections || [];
    for (let i = 0; i < sections.length; ++i)
      expanded.push(sections[i].expanded);

    this.contentElement.removeChildren();
    this.sections = [];

    // Get array of property user-friendly names.
    for (let i = 0; i < properties.length; ++i) {
      if (!parseInt(properties[i].name, 10))
        continue;
      const property = properties[i].value;
      let title = property.description;
      title = title.replace(/Prototype$/, '');
      const section = new ObjectUI.ObjectPropertiesSection(property, title);
      section.element.classList.add('properties-widget-section');
      this.sections.push(section);
      this.contentElement.appendChild(section.element);
      if (expanded[this.sections.length - 1])
        section.expand();
      section.addEventListener(UI.TreeOutline.Events.ElementExpanded, this._propertyExpanded, this);
    }

    /**
     * @suppressReceiverCheck
     * @this {*}
     */
    function protoList() {
      let proto = this;
      const result = {__proto__: null};
      let counter = 1;
      while (proto) {
        result[counter++] = proto;
        proto = proto.__proto__;
      }
      return result;
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _propertyExpanded(event) {
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.DOMPropertiesExpanded);
    for (const section of this.sections)
      section.removeEventListener(UI.TreeOutline.Events.ElementExpanded, this._propertyExpanded, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _onNodeChange(event) {
    if (!this._node)
      return;
    const data = event.data;
    const node = /** @type {!SDK.DOMNode} */ (data instanceof SDK.DOMNode ? data : data.node);
    if (this._node !== node)
      return;
    this.update();
  }
};

Elements.PropertiesWidget._objectGroupName = 'properties-sidebar-pane';
