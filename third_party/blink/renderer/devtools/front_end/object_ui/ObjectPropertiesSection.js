/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
ObjectUI.ObjectPropertiesSection = class extends UI.TreeOutlineInShadow {
  /**
   * @param {!SDK.RemoteObject} object
   * @param {?string|!Element=} title
   * @param {!Components.Linkifier=} linkifier
   * @param {?string=} emptyPlaceholder
   * @param {boolean=} ignoreHasOwnProperty
   * @param {!Array.<!SDK.RemoteObjectProperty>=} extraProperties
   * @param {boolean=} showOverflow
   */
  constructor(object, title, linkifier, emptyPlaceholder, ignoreHasOwnProperty, extraProperties, showOverflow) {
    super();
    this._object = object;
    this._editable = true;
    if (!showOverflow)
      this.hideOverflow();
    this.setFocusable(true);
    this.setShowSelectionOnKeyboardFocus(true);
    this._objectTreeElement = new ObjectUI.ObjectPropertiesSection.RootElement(
        object, linkifier, emptyPlaceholder, ignoreHasOwnProperty, extraProperties);
    this.appendChild(this._objectTreeElement);
    if (typeof title === 'string' || !title) {
      this.titleElement = this.element.createChild('span');
      this.titleElement.textContent = title || '';
    } else {
      this.titleElement = title;
      this.element.appendChild(title);
    }
    if (!this.titleElement.hasAttribute('tabIndex'))
      this.titleElement.tabIndex = -1;

    this.element._section = this;
    this.registerRequiredCSS('object_ui/objectValue.css');
    this.registerRequiredCSS('object_ui/objectPropertiesSection.css');
    this.rootElement().childrenListElement.classList.add('source-code', 'object-properties-section');
  }

  /**
   * @param {!SDK.RemoteObject} object
   * @param {!Components.Linkifier=} linkifier
   * @param {boolean=} skipProto
   * @return {!Element}
   */
  static defaultObjectPresentation(object, linkifier, skipProto) {
    const componentRoot = createElementWithClass('span', 'source-code');
    const shadowRoot = UI.createShadowRootWithCoreStyles(componentRoot, 'object_ui/objectValue.css');
    shadowRoot.appendChild(
        ObjectUI.ObjectPropertiesSection.createValueElement(object, false /* wasThrown */, true /* showPreview */));
    if (!object.hasChildren)
      return componentRoot;

    const objectPropertiesSection = new ObjectUI.ObjectPropertiesSection(object, componentRoot, linkifier);
    objectPropertiesSection.editable = false;
    if (skipProto)
      objectPropertiesSection.skipProto();

    return objectPropertiesSection.element;
  }

  /**
   * @param {!SDK.RemoteObjectProperty} propertyA
   * @param {!SDK.RemoteObjectProperty} propertyB
   * @return {number}
   */
  static CompareProperties(propertyA, propertyB) {
    const a = propertyA.name;
    const b = propertyB.name;
    if (a === '__proto__')
      return 1;
    if (b === '__proto__')
      return -1;
    if (!propertyA.enumerable && propertyB.enumerable)
      return 1;
    if (!propertyB.enumerable && propertyA.enumerable)
      return -1;
    if (a.startsWith('_') && !b.startsWith('_'))
      return 1;
    if (b.startsWith('_') && !a.startsWith('_'))
      return -1;
    if (propertyA.symbol && !propertyB.symbol)
      return 1;
    if (propertyB.symbol && !propertyA.symbol)
      return -1;
    return String.naturalOrderComparator(a, b);
  }

  /**
   * @param {?string} name
   * @return {!Element}
   */
  static createNameElement(name) {
    const nameElement = createElementWithClass('span', 'name');
    if (/^\s|\s$|^$|\n/.test(name))
      nameElement.createTextChildren('"', name.replace(/\n/g, '\u21B5'), '"');
    else
      nameElement.textContent = name;
    return nameElement;
  }

  /**
   * @param {?string=} description
   * @param {boolean=} includePreview
   * @param {string=} defaultName
   * @return {!Element} valueElement
   */
  static valueElementForFunctionDescription(description, includePreview, defaultName) {
    const valueElement = createElementWithClass('span', 'object-value-function');
    description = description || '';
    const text = description.replace(/^function [gs]et /, 'function ')
                     .replace(/^function [gs]et\(/, 'function\(')
                     .replace(/^[gs]et /, '');
    defaultName = defaultName || '';

    // This set of best-effort regular expressions captures common function descriptions.
    // Ideally, some parser would provide prefix, arguments, function body text separately.
    const asyncMatch = text.match(/^(async\s+function)/);
    const isGenerator = text.startsWith('function*');
    const isGeneratorShorthand = text.startsWith('*');
    const isBasic = !isGenerator && text.startsWith('function');
    const isClass = text.startsWith('class ') || text.startsWith('class{');
    const firstArrowIndex = text.indexOf('=>');
    const isArrow = !asyncMatch && !isGenerator && !isBasic && !isClass && firstArrowIndex > 0;

    let textAfterPrefix;
    if (isClass) {
      textAfterPrefix = text.substring('class'.length);
      const classNameMatch = /^[^{\s]+/.exec(textAfterPrefix.trim());
      let className = defaultName;
      if (classNameMatch)
        className = classNameMatch[0].trim() || defaultName;
      addElements('class', textAfterPrefix, className);
    } else if (asyncMatch) {
      textAfterPrefix = text.substring(asyncMatch[1].length);
      addElements('async \u0192', textAfterPrefix, nameAndArguments(textAfterPrefix));
    } else if (isGenerator) {
      textAfterPrefix = text.substring('function*'.length);
      addElements('\u0192*', textAfterPrefix, nameAndArguments(textAfterPrefix));
    } else if (isGeneratorShorthand) {
      textAfterPrefix = text.substring('*'.length);
      addElements('\u0192*', textAfterPrefix, nameAndArguments(textAfterPrefix));
    } else if (isBasic) {
      textAfterPrefix = text.substring('function'.length);
      addElements('\u0192', textAfterPrefix, nameAndArguments(textAfterPrefix));
    } else if (isArrow) {
      const maxArrowFunctionCharacterLength = 60;
      let abbreviation = text;
      if (defaultName)
        abbreviation = defaultName + '()';
      else if (text.length > maxArrowFunctionCharacterLength)
        abbreviation = text.substring(0, firstArrowIndex + 2) + ' {\u2026}';
      addElements('', text, abbreviation);
    } else {
      addElements('\u0192', text, nameAndArguments(text));
    }
    valueElement.title = description.trimEnd(500);
    return valueElement;

    /**
     * @param {string} contents
     * @return {string}
     */
    function nameAndArguments(contents) {
      const startOfArgumentsIndex = contents.indexOf('(');
      const endOfArgumentsMatch = contents.match(/\)\s*{/);
      if (startOfArgumentsIndex !== -1 && endOfArgumentsMatch && endOfArgumentsMatch.index > startOfArgumentsIndex) {
        const name = contents.substring(0, startOfArgumentsIndex).trim() || defaultName;
        const args = contents.substring(startOfArgumentsIndex, endOfArgumentsMatch.index + 1);
        return name + args;
      }
      return defaultName + '()';
    }

    /**
     * @param {string} prefix
     * @param {string} body
     * @param {string} abbreviation
     */
    function addElements(prefix, body, abbreviation) {
      const maxFunctionBodyLength = 200;
      if (prefix.length)
        valueElement.createChild('span', 'object-value-function-prefix').textContent = prefix + ' ';
      if (includePreview)
        valueElement.createTextChild(body.trim().trimEnd(maxFunctionBodyLength));
      else
        valueElement.createTextChild(abbreviation.replace(/\n/g, ' '));
    }
  }

  /**
   * @param {!SDK.RemoteObject} value
   * @param {boolean} wasThrown
   * @param {boolean} showPreview
   * @param {!Element=} parentElement
   * @param {!Components.Linkifier=} linkifier
   * @return {!Element}
   */
  static createValueElementWithCustomSupport(value, wasThrown, showPreview, parentElement, linkifier) {
    if (value.customPreview()) {
      const result = (new ObjectUI.CustomPreviewComponent(value)).element;
      result.classList.add('object-properties-section-custom-section');
      return result;
    }
    return ObjectUI.ObjectPropertiesSection.createValueElement(value, wasThrown, showPreview, parentElement, linkifier);
  }

  /**
   * @param {!SDK.RemoteObject} value
   * @param {boolean} wasThrown
   * @param {boolean} showPreview
   * @param {!Element=} parentElement
   * @param {!Components.Linkifier=} linkifier
   * @return {!Element}
   */
  static createValueElement(value, wasThrown, showPreview, parentElement, linkifier) {
    let valueElement;
    const type = value.type;
    const subtype = value.subtype;
    const description = value.description;
    if (type === 'object' && subtype === 'internal#location') {
      const rawLocation = value.debuggerModel().createRawLocationByScriptId(
          value.value.scriptId, value.value.lineNumber, value.value.columnNumber);
      if (rawLocation && linkifier)
        return linkifier.linkifyRawLocation(rawLocation, '');
      valueElement = createUnknownInternalLocationElement();
    } else if (type === 'string' && typeof description === 'string') {
      valueElement = createStringElement();
    } else if (type === 'function') {
      valueElement = ObjectUI.ObjectPropertiesSection.valueElementForFunctionDescription(description);
    } else if (type === 'object' && subtype === 'node' && description) {
      valueElement = createNodeElement();
    } else if (type === 'number' && description && description.indexOf('e') !== -1) {
      valueElement = createNumberWithExponentElement();
      if (parentElement)  // FIXME: do it in the caller.
        parentElement.classList.add('hbox');
    } else {
      valueElement = createElementWithClass('span', 'object-value-' + (subtype || type));
      valueElement.title = description || '';
      if (value.preview && showPreview) {
        const previewFormatter = new ObjectUI.RemoteObjectPreviewFormatter();
        previewFormatter.appendObjectPreview(valueElement, value.preview, false /* isEntry */);
      } else if (description.length > ObjectUI.ObjectPropertiesSection._maxRenderableStringLength) {
        valueElement.appendChild(UI.createExpandableText(description, 50));
      } else {
        valueElement.textContent = description;
      }
    }

    if (wasThrown) {
      const wrapperElement = createElementWithClass('span', 'error value');
      wrapperElement.createTextChild('[' + Common.UIString('Exception') + ': ');
      wrapperElement.appendChild(valueElement);
      wrapperElement.createTextChild(']');
      return wrapperElement;
    }
    valueElement.classList.add('value');
    return valueElement;

    /**
     * @return {!Element}
     */
    function createUnknownInternalLocationElement() {
      const valueElement = createElementWithClass('span');
      valueElement.textContent = '<' + Common.UIString('unknown') + '>';
      valueElement.title = description || '';
      return valueElement;
    }

    /**
     * @return {!Element}
     */
    function createStringElement() {
      const valueElement = createElementWithClass('span', 'object-value-string');
      const text = description.replace(/\n/g, '\u21B5');
      valueElement.createChild('span', 'object-value-string-quote').textContent = '"';
      if (description.length > ObjectUI.ObjectPropertiesSection._maxRenderableStringLength)
        valueElement.appendChild(UI.createExpandableText(text, 50));
      else
        valueElement.createTextChild(text);
      valueElement.createChild('span', 'object-value-string-quote').textContent = '"';
      valueElement.title = description || '';
      return valueElement;
    }

    /**
     * @return {!Element}
     */
    function createNodeElement() {
      const valueElement = createElementWithClass('span', 'object-value-node');
      ObjectUI.RemoteObjectPreviewFormatter.createSpansForNodeTitle(valueElement, /** @type {string} */ (description));
      valueElement.addEventListener('click', event => {
        Common.Revealer.reveal(value);
        event.consume(true);
      }, false);
      valueElement.addEventListener('mousemove', () => SDK.OverlayModel.highlightObjectAsDOMNode(value), false);
      valueElement.addEventListener('mouseleave', () => SDK.OverlayModel.hideDOMNodeHighlight(), false);
      return valueElement;
    }

    /**
     * @return {!Element}
     */
    function createNumberWithExponentElement() {
      const valueElement = createElementWithClass('span', 'object-value-number');
      const numberParts = description.split('e');
      valueElement.createChild('span', 'object-value-scientific-notation-mantissa').textContent = numberParts[0];
      valueElement.createChild('span', 'object-value-scientific-notation-exponent').textContent = 'e' + numberParts[1];
      valueElement.classList.add('object-value-scientific-notation-number');
      valueElement.title = description || '';
      return valueElement;
    }
  }

  /**
   * @param {!SDK.RemoteObject} func
   * @param {!Element} element
   * @param {boolean} linkify
   * @param {boolean=} includePreview
   */
  static formatObjectAsFunction(func, element, linkify, includePreview) {
    func.debuggerModel().functionDetailsPromise(func).then(didGetDetails);

    /**
     * @param {?SDK.DebuggerModel.FunctionDetails} response
     */
    function didGetDetails(response) {
      if (linkify && response && response.location) {
        element.classList.add('linkified');
        element.addEventListener('click', () => Common.Revealer.reveal(response.location) && false);
      }

      // The includePreview flag is false for formats such as console.dir().
      let defaultName = includePreview ? '' : 'anonymous';
      if (response && response.functionName)
        defaultName = response.functionName;
      const valueElement = ObjectUI.ObjectPropertiesSection.valueElementForFunctionDescription(
          func.description, includePreview, defaultName);
      element.appendChild(valueElement);
    }
  }

  skipProto() {
    this._skipProto = true;
  }

  expand() {
    this._objectTreeElement.expand();
  }

  /**
   * @param {boolean} value
   */
  setEditable(value) {
    this._editable = value;
  }

  /**
   * @return {!UI.TreeElement}
   */
  objectTreeElement() {
    return this._objectTreeElement;
  }

  enableContextMenu() {
    this.element.addEventListener('contextmenu', this._contextMenuEventFired.bind(this), false);
  }

  _contextMenuEventFired(event) {
    const contextMenu = new UI.ContextMenu(event);
    contextMenu.appendApplicableItems(this._object);
    if (this._object instanceof SDK.LocalJSONObject) {
      contextMenu.viewSection().appendItem(
          ls`Expand recursively`,
          this._objectTreeElement.expandRecursively.bind(this._objectTreeElement, Number.MAX_VALUE));
      contextMenu.viewSection().appendItem(
          ls`Collapse children`, this._objectTreeElement.collapseChildren.bind(this._objectTreeElement));
    }
    contextMenu.show();
  }

  titleLessMode() {
    this._objectTreeElement.listItemElement.classList.add('hidden');
    this._objectTreeElement.childrenListElement.classList.add('title-less-mode');
    this._objectTreeElement.expand();
  }
};

/** @const */
ObjectUI.ObjectPropertiesSection._arrayLoadThreshold = 100;
/** @const */
ObjectUI.ObjectPropertiesSection._maxRenderableStringLength = 10000;


/**
 * @unrestricted
 */
ObjectUI.ObjectPropertiesSection.RootElement = class extends UI.TreeElement {
  /**
   * @param {!SDK.RemoteObject} object
   * @param {!Components.Linkifier=} linkifier
   * @param {?string=} emptyPlaceholder
   * @param {boolean=} ignoreHasOwnProperty
   * @param {!Array.<!SDK.RemoteObjectProperty>=} extraProperties
   */
  constructor(object, linkifier, emptyPlaceholder, ignoreHasOwnProperty, extraProperties) {
    const contentElement = createElement('content');
    super(contentElement);

    this._object = object;
    this._extraProperties = extraProperties || [];
    this._ignoreHasOwnProperty = !!ignoreHasOwnProperty;
    this._emptyPlaceholder = emptyPlaceholder;

    this.setExpandable(true);
    this.selectable = true;
    this.toggleOnClick = true;
    this.listItemElement.classList.add('object-properties-section-root-element');
    this._linkifier = linkifier;
  }

  /**
   * @override
   */
  onexpand() {
    if (this.treeOutline)
      this.treeOutline.element.classList.add('expanded');
  }

  /**
   * @override
   */
  oncollapse() {
    if (this.treeOutline)
      this.treeOutline.element.classList.remove('expanded');
  }

  /**
   * @override
   * @param {!Event} e
   * @return {boolean}
   */
  ondblclick(e) {
    return true;
  }

  /**
   * @override
   */
  onpopulate() {
    ObjectUI.ObjectPropertyTreeElement._populate(
        this, this._object, !!this.treeOutline._skipProto, this._linkifier, this._emptyPlaceholder,
        this._ignoreHasOwnProperty, this._extraProperties);
  }
};

/**
 * @unrestricted
 */
ObjectUI.ObjectPropertyTreeElement = class extends UI.TreeElement {
  /**
   * @param {!SDK.RemoteObjectProperty} property
   * @param {!Components.Linkifier=} linkifier
   */
  constructor(property, linkifier) {
    // Pass an empty title, the title gets made later in onattach.
    super();

    this.property = property;
    this.toggleOnClick = true;
    /** @type {!Array.<!Object>} */
    this._highlightChanges = [];
    this._linkifier = linkifier;
    this.listItemElement.addEventListener('contextmenu', this._contextMenuFired.bind(this), false);
  }

  /**
   * @param {!UI.TreeElement} treeElement
   * @param {!SDK.RemoteObject} value
   * @param {boolean} skipProto
   * @param {!Components.Linkifier=} linkifier
   * @param {?string=} emptyPlaceholder
   * @param {boolean=} flattenProtoChain
   * @param {!Array.<!SDK.RemoteObjectProperty>=} extraProperties
   * @param {!SDK.RemoteObject=} targetValue
   * @return {!Promise}
   */
  static async _populate(
      treeElement, value, skipProto, linkifier, emptyPlaceholder, flattenProtoChain, extraProperties, targetValue) {
    if (value.arrayLength() > ObjectUI.ObjectPropertiesSection._arrayLoadThreshold) {
      treeElement.removeChildren();
      ObjectUI.ArrayGroupingTreeElement._populateArray(treeElement, value, 0, value.arrayLength() - 1, linkifier);
      return;
    }

    let allProperties;
    if (flattenProtoChain)
      allProperties = await value.getAllProperties(false /* accessorPropertiesOnly */, true /* generatePreview */);
    else
      allProperties = await SDK.RemoteObject.loadFromObjectPerProto(value, true /* generatePreview */);
    const properties = allProperties.properties;
    const internalProperties = allProperties.internalProperties;
    treeElement.removeChildren();
    if (!properties)
      return;

    extraProperties = extraProperties || [];
    for (let i = 0; i < extraProperties.length; ++i)
      properties.push(extraProperties[i]);

    ObjectUI.ObjectPropertyTreeElement.populateWithProperties(
        treeElement, properties, internalProperties, skipProto, targetValue || value, linkifier, emptyPlaceholder);
  }

  /**
   * @param {!UI.TreeElement} treeNode
   * @param {!Array.<!SDK.RemoteObjectProperty>} properties
   * @param {?Array.<!SDK.RemoteObjectProperty>} internalProperties
   * @param {boolean} skipProto
   * @param {?SDK.RemoteObject} value
   * @param {!Components.Linkifier=} linkifier
   * @param {?string=} emptyPlaceholder
   */
  static populateWithProperties(
      treeNode,
      properties,
      internalProperties,
      skipProto,
      value,
      linkifier,
      emptyPlaceholder) {
    properties.sort(ObjectUI.ObjectPropertiesSection.CompareProperties);

    const tailProperties = [];
    let protoProperty = null;
    for (let i = 0; i < properties.length; ++i) {
      const property = properties[i];
      property.parentObject = value;
      if (property.name === '__proto__' && !property.isAccessorProperty()) {
        protoProperty = property;
        continue;
      }

      if (property.isOwn && property.getter) {
        const getterProperty = new SDK.RemoteObjectProperty('get ' + property.name, property.getter, false);
        getterProperty.parentObject = value;
        tailProperties.push(getterProperty);
      }
      if (property.isOwn && property.setter) {
        const setterProperty = new SDK.RemoteObjectProperty('set ' + property.name, property.setter, false);
        setterProperty.parentObject = value;
        tailProperties.push(setterProperty);
      }
      const canShowProperty = property.getter || !property.isAccessorProperty();
      if (canShowProperty && property.name !== '__proto__')
        treeNode.appendChild(new ObjectUI.ObjectPropertyTreeElement(property, linkifier));
    }
    for (let i = 0; i < tailProperties.length; ++i)
      treeNode.appendChild(new ObjectUI.ObjectPropertyTreeElement(tailProperties[i], linkifier));
    if (!skipProto && protoProperty)
      treeNode.appendChild(new ObjectUI.ObjectPropertyTreeElement(protoProperty, linkifier));

    if (internalProperties) {
      for (let i = 0; i < internalProperties.length; i++) {
        internalProperties[i].parentObject = value;
        const treeElement = new ObjectUI.ObjectPropertyTreeElement(internalProperties[i], linkifier);
        if (internalProperties[i].name === '[[Entries]]') {
          treeElement.setExpandable(true);
          treeElement.expand();
        }
        treeNode.appendChild(treeElement);
      }
    }

    ObjectUI.ObjectPropertyTreeElement._appendEmptyPlaceholderIfNeeded(treeNode, emptyPlaceholder);
  }

  /**
   * @param {!UI.TreeElement} treeNode
   * @param {?string=} emptyPlaceholder
   */
  static _appendEmptyPlaceholderIfNeeded(treeNode, emptyPlaceholder) {
    if (treeNode.childCount())
      return;
    const title = createElementWithClass('div', 'gray-info-message');
    title.textContent = emptyPlaceholder || Common.UIString('No properties');
    const infoElement = new UI.TreeElement(title);
    treeNode.appendChild(infoElement);
  }

  /**
   * @param {?SDK.RemoteObject} object
   * @param {!Array.<string>} propertyPath
   * @param {function(!SDK.CallFunctionResult)} callback
   * @return {!Element}
   */
  static createRemoteObjectAccessorPropertySpan(object, propertyPath, callback) {
    const rootElement = createElement('span');
    const element = rootElement.createChild('span');
    element.textContent = Common.UIString('(...)');
    if (!object)
      return rootElement;
    element.classList.add('object-value-calculate-value-button');
    element.title = Common.UIString('Invoke property getter');
    element.addEventListener('click', onInvokeGetterClick, false);

    function onInvokeGetterClick(event) {
      event.consume();
      object.callFunction(invokeGetter, [{value: JSON.stringify(propertyPath)}]).then(callback);
    }

    /**
     * @param {string} arrayStr
     * @suppressReceiverCheck
     * @this {Object}
     */
    function invokeGetter(arrayStr) {
      let result = this;
      const properties = JSON.parse(arrayStr);
      for (let i = 0, n = properties.length; i < n; ++i)
        result = result[properties[i]];
      return result;
    }

    return rootElement;
  }

  /**
   * @param {!RegExp} regex
   * @param {string=} additionalCssClassName
   * @return {boolean}
   */
  setSearchRegex(regex, additionalCssClassName) {
    let cssClasses = UI.highlightedSearchResultClassName;
    if (additionalCssClassName)
      cssClasses += ' ' + additionalCssClassName;
    this.revertHighlightChanges();

    this._applySearch(regex, this.nameElement, cssClasses);
    const valueType = this.property.value.type;
    if (valueType !== 'object')
      this._applySearch(regex, this.valueElement, cssClasses);

    return !!this._highlightChanges.length;
  }

  /**
   * @param {!RegExp} regex
   * @param {!Element} element
   * @param {string} cssClassName
   */
  _applySearch(regex, element, cssClassName) {
    const ranges = [];
    const content = element.textContent;
    regex.lastIndex = 0;
    let match = regex.exec(content);
    while (match) {
      ranges.push(new TextUtils.SourceRange(match.index, match[0].length));
      match = regex.exec(content);
    }
    if (ranges.length)
      UI.highlightRangesWithStyleClass(element, ranges, cssClassName, this._highlightChanges);
  }

  revertHighlightChanges() {
    UI.revertDomChanges(this._highlightChanges);
    this._highlightChanges = [];
  }

  /**
   * @override
   */
  onpopulate() {
    const propertyValue = /** @type {!SDK.RemoteObject} */ (this.property.value);
    console.assert(propertyValue);
    const skipProto = this.treeOutline ? this.treeOutline._skipProto : true;
    const targetValue = this.property.name !== '__proto__' ? propertyValue : this.property.parentObject;
    ObjectUI.ObjectPropertyTreeElement._populate(
        this, propertyValue, skipProto, this._linkifier, undefined, undefined, undefined, targetValue);
  }

  /**
   * @override
   * @return {boolean}
   */
  ondblclick(event) {
    const inEditableElement = event.target.isSelfOrDescendant(this.valueElement) ||
        (this.expandedValueElement && event.target.isSelfOrDescendant(this.expandedValueElement));
    if (!this.property.value.customPreview() && inEditableElement && (this.property.writable || this.property.setter))
      this._startEditing();
    return false;
  }

  /**
   * @override
   */
  onattach() {
    this.update();
    this._updateExpandable();
  }

  /**
   * @override
   */
  onexpand() {
    this._showExpandedValueElement(true);
  }

  /**
   * @override
   */
  oncollapse() {
    this._showExpandedValueElement(false);
  }

  /**
   * @param {boolean} value
   */
  _showExpandedValueElement(value) {
    if (!this.expandedValueElement)
      return;
    if (value)
      this._rowContainer.replaceChild(this.expandedValueElement, this.valueElement);
    else
      this._rowContainer.replaceChild(this.valueElement, this.expandedValueElement);
  }

  /**
   * @param {!SDK.RemoteObject} value
   * @return {?Element}
   */
  _createExpandedValueElement(value) {
    const needsAlternateValue = value.hasChildren && !value.customPreview() && value.subtype !== 'node' &&
        value.type !== 'function' && (value.type !== 'object' || value.preview);
    if (!needsAlternateValue)
      return null;

    const valueElement = createElementWithClass('span', 'value');
    if (value.description === 'Object')
      valueElement.textContent = '';
    else
      valueElement.setTextContentTruncatedIfNeeded(value.description || '');
    valueElement.classList.add('object-value-' + (value.subtype || value.type));
    valueElement.title = value.description || '';
    return valueElement;
  }

  update() {
    this.nameElement = ObjectUI.ObjectPropertiesSection.createNameElement(this.property.name);
    if (!this.property.enumerable)
      this.nameElement.classList.add('object-properties-section-dimmed');
    if (this.property.synthetic)
      this.nameElement.classList.add('synthetic-property');

    this._updatePropertyPath();

    if (this.property.value) {
      const showPreview = this.property.name !== '__proto__';
      this.valueElement = ObjectUI.ObjectPropertiesSection.createValueElementWithCustomSupport(
          this.property.value, this.property.wasThrown, showPreview, this.listItemElement, this._linkifier);
    } else if (this.property.getter) {
      this.valueElement = ObjectUI.ObjectPropertyTreeElement.createRemoteObjectAccessorPropertySpan(
          this.property.parentObject, [this.property.name], this._onInvokeGetterClick.bind(this));
    } else {
      this.valueElement = createElementWithClass('span', 'object-value-undefined');
      this.valueElement.textContent = Common.UIString('<unreadable>');
      this.valueElement.title = Common.UIString('No property getter');
    }

    const valueText = this.valueElement.textContent;
    if (this.property.value && valueText && !this.property.wasThrown)
      this.expandedValueElement = this._createExpandedValueElement(this.property.value);

    this.listItemElement.removeChildren();
    this._rowContainer = UI.html`<span class='name-and-value'>${this.nameElement}: ${this.valueElement}</span>`;
    this.listItemElement.appendChild(this._rowContainer);
  }

  _updatePropertyPath() {
    if (this.nameElement.title)
      return;

    const name = this.property.name;

    if (this.property.synthetic) {
      this.nameElement.title = name;
      return;
    }

    const useDotNotation = /^(_|\$|[A-Z])(_|\$|[A-Z]|\d)*$/i;
    const isInteger = /^[1-9]\d*$/;

    const parentPath =
        (this.parent.nameElement && !this.parent.property.synthetic) ? this.parent.nameElement.title : '';

    if (useDotNotation.test(name))
      this.nameElement.title = parentPath ? `${parentPath}.${name}` : name;
    else if (isInteger.test(name))
      this.nameElement.title = parentPath + '[' + name + ']';
    else
      this.nameElement.title = parentPath + '["' + JSON.stringify(name) + '"]';
  }

  /**
   * @param {!Event} event
   */
  _contextMenuFired(event) {
    const contextMenu = new UI.ContextMenu(event);
    contextMenu.appendApplicableItems(this);
    if (this.property.symbol)
      contextMenu.appendApplicableItems(this.property.symbol);
    if (this.property.value)
      contextMenu.appendApplicableItems(this.property.value);
    if (!this.property.synthetic && this.nameElement && this.nameElement.title) {
      const copyPathHandler = InspectorFrontendHost.copyText.bind(InspectorFrontendHost, this.nameElement.title);
      contextMenu.clipboardSection().appendItem(ls`Copy property path`, copyPathHandler);
    }
    if (this.property.parentObject instanceof SDK.LocalJSONObject) {
      contextMenu.viewSection().appendItem(ls`Expand recursively`, this.expandRecursively.bind(this, Number.MAX_VALUE));
      contextMenu.viewSection().appendItem(ls`Collapse children`, this.collapseChildren.bind(this));
    }
    contextMenu.show();
  }

  _startEditing() {
    if (this._prompt || !this.treeOutline._editable || this._readOnly)
      return;

    this._editableDiv = this._rowContainer.createChild('span', 'editable-div');

    let text = this.property.value.description;
    if (this.property.value.type === 'string' && typeof text === 'string')
      text = '"' + text + '"';

    this._editableDiv.setTextContentTruncatedIfNeeded(text, Common.UIString('<string is too large to edit>'));
    const originalContent = this._editableDiv.textContent;

    // Lie about our children to prevent expanding on double click and to collapse subproperties.
    this.setExpandable(false);
    this.listItemElement.classList.add('editing-sub-part');
    this.valueElement.classList.add('hidden');

    this._prompt = new ObjectUI.ObjectPropertyPrompt();

    const proxyElement =
        this._prompt.attachAndStartEditing(this._editableDiv, this._editingCommitted.bind(this, originalContent));
    proxyElement.classList.add('property-prompt');
    this.listItemElement.getComponentSelection().selectAllChildren(this._editableDiv);
    proxyElement.addEventListener('keydown', this._promptKeyDown.bind(this, originalContent), false);
  }

  _editingEnded() {
    this._prompt.detach();
    delete this._prompt;
    this._editableDiv.remove();
    this._updateExpandable();
    this.listItemElement.scrollLeft = 0;
    this.listItemElement.classList.remove('editing-sub-part');
  }

  _editingCancelled() {
    this.valueElement.classList.remove('hidden');
    this._editingEnded();
  }

  /**
   * @param {string} originalContent
   */
  _editingCommitted(originalContent) {
    const userInput = this._prompt.text();
    if (userInput === originalContent) {
      this._editingCancelled();  // nothing changed, so cancel
      return;
    }

    this._editingEnded();
    this._applyExpression(userInput);
  }

  /**
   * @param {string} originalContent
   * @param {!Event} event
   */
  _promptKeyDown(originalContent, event) {
    if (isEnterKey(event)) {
      event.consume();
      this._editingCommitted(originalContent);
      return;
    }
    if (event.key === 'Escape') {
      event.consume();
      this._editingCancelled();
      return;
    }
  }

  /**
   * @param {string} expression
   */
  async _applyExpression(expression) {
    const property = SDK.RemoteObject.toCallArgument(this.property.symbol || this.property.name);
    expression = ObjectUI.JavaScriptREPL.wrapObjectLiteral(expression.trim());

    if (this.property.synthetic) {
      let invalidate = false;
      if (expression)
        invalidate = await this.property.setSyntheticValue(expression);
      if (invalidate) {
        const parent = this.parent;
        parent.invalidateChildren();
        parent.onpopulate();
      } else {
        this.update();
      }
      return;
    }

    const errorPromise = expression ? this.property.parentObject.setPropertyValue(property, expression) :
                                      this.property.parentObject.deleteProperty(property);
    const error = await errorPromise;
    if (error) {
      this.update();
      return;
    }

    if (!expression) {
      // The property was deleted, so remove this tree element.
      this.parent.removeChild(this);
    } else {
      // Call updateSiblings since their value might be based on the value that just changed.
      const parent = this.parent;
      parent.invalidateChildren();
      parent.onpopulate();
    }
  }

  /**
   * @param {!SDK.CallFunctionResult} result
   */
  _onInvokeGetterClick(result) {
    if (!result.object)
      return;
    this.property.value = result.object;
    this.property.wasThrown = result.wasThrown;

    this.update();
    this.invalidateChildren();
    this._updateExpandable();
  }

  _updateExpandable() {
    if (this.property.value) {
      this.setExpandable(
          !this.property.value.customPreview() && this.property.value.hasChildren && !this.property.wasThrown);
    } else {
      this.setExpandable(false);
    }
  }

  /**
   * @return {string}
   */
  path() {
    return this.nameElement.title;
  }
};


/**
 * @unrestricted
 */
ObjectUI.ArrayGroupingTreeElement = class extends UI.TreeElement {
  /**
   * @param {!SDK.RemoteObject} object
   * @param {number} fromIndex
   * @param {number} toIndex
   * @param {number} propertyCount
   * @param {!Components.Linkifier=} linkifier
   */
  constructor(object, fromIndex, toIndex, propertyCount, linkifier) {
    super(String.sprintf('[%d \u2026 %d]', fromIndex, toIndex), true);
    this.toggleOnClick = true;
    this._fromIndex = fromIndex;
    this._toIndex = toIndex;
    this._object = object;
    this._readOnly = true;
    this._propertyCount = propertyCount;
    this._linkifier = linkifier;
  }

  /**
   * @param {!UI.TreeElement} treeNode
   * @param {!SDK.RemoteObject} object
   * @param {number} fromIndex
   * @param {number} toIndex
   * @param {!Components.Linkifier=} linkifier
   */
  static _populateArray(treeNode, object, fromIndex, toIndex, linkifier) {
    ObjectUI.ArrayGroupingTreeElement._populateRanges(treeNode, object, fromIndex, toIndex, true, linkifier);
  }

  /**
   * @param {!UI.TreeElement} treeNode
   * @param {!SDK.RemoteObject} object
   * @param {number} fromIndex
   * @param {number} toIndex
   * @param {boolean} topLevel
   * @param {!Components.Linkifier=} linkifier
   * @this {ObjectUI.ArrayGroupingTreeElement}
   */
  static _populateRanges(treeNode, object, fromIndex, toIndex, topLevel, linkifier) {
    object
        .callFunctionJSON(
            packRanges,
            [
              {value: fromIndex}, {value: toIndex}, {value: ObjectUI.ArrayGroupingTreeElement._bucketThreshold},
              {value: ObjectUI.ArrayGroupingTreeElement._sparseIterationThreshold},
              {value: ObjectUI.ArrayGroupingTreeElement._getOwnPropertyNamesThreshold}
            ])
        .then(callback);

    /**
     * Note: must declare params as optional.
     * @param {number=} fromIndex
     * @param {number=} toIndex
     * @param {number=} bucketThreshold
     * @param {number=} sparseIterationThreshold
     * @param {number=} getOwnPropertyNamesThreshold
     * @suppressReceiverCheck
     * @this {Object}
     */
    function packRanges(fromIndex, toIndex, bucketThreshold, sparseIterationThreshold, getOwnPropertyNamesThreshold) {
      let ownPropertyNames = null;
      const consecutiveRange = (toIndex - fromIndex >= sparseIterationThreshold) && ArrayBuffer.isView(this);
      const skipGetOwnPropertyNames = consecutiveRange && (toIndex - fromIndex >= getOwnPropertyNamesThreshold);

      function* arrayIndexes(object) {
        if (toIndex - fromIndex < sparseIterationThreshold) {
          for (let i = fromIndex; i <= toIndex; ++i) {
            if (i in object)
              yield i;
          }
        } else {
          ownPropertyNames = ownPropertyNames || Object.getOwnPropertyNames(object);
          for (let i = 0; i < ownPropertyNames.length; ++i) {
            const name = ownPropertyNames[i];
            const index = name >>> 0;
            if (('' + index) === name && fromIndex <= index && index <= toIndex)
              yield index;
          }
        }
      }

      let count = 0;
      if (consecutiveRange) {
        count = toIndex - fromIndex + 1;
      } else {
        for (const i of arrayIndexes(this))  // eslint-disable-line
          ++count;
      }

      let bucketSize = count;
      if (count <= bucketThreshold)
        bucketSize = count;
      else
        bucketSize = Math.pow(bucketThreshold, Math.ceil(Math.log(count) / Math.log(bucketThreshold)) - 1);

      const ranges = [];
      if (consecutiveRange) {
        for (let i = fromIndex; i <= toIndex; i += bucketSize) {
          const groupStart = i;
          let groupEnd = groupStart + bucketSize - 1;
          if (groupEnd > toIndex)
            groupEnd = toIndex;
          ranges.push([groupStart, groupEnd, groupEnd - groupStart + 1]);
        }
      } else {
        count = 0;
        let groupStart = -1;
        let groupEnd = 0;
        for (const i of arrayIndexes(this)) {
          if (groupStart === -1)
            groupStart = i;
          groupEnd = i;
          if (++count === bucketSize) {
            ranges.push([groupStart, groupEnd, count]);
            count = 0;
            groupStart = -1;
          }
        }
        if (count > 0)
          ranges.push([groupStart, groupEnd, count]);
      }

      return {ranges: ranges, skipGetOwnPropertyNames: skipGetOwnPropertyNames};
    }

    function callback(result) {
      if (!result)
        return;
      const ranges = /** @type {!Array.<!Array.<number>>} */ (result.ranges);
      if (ranges.length === 1) {
        ObjectUI.ArrayGroupingTreeElement._populateAsFragment(treeNode, object, ranges[0][0], ranges[0][1], linkifier);
      } else {
        for (let i = 0; i < ranges.length; ++i) {
          const fromIndex = ranges[i][0];
          const toIndex = ranges[i][1];
          const count = ranges[i][2];
          if (fromIndex === toIndex)
            ObjectUI.ArrayGroupingTreeElement._populateAsFragment(treeNode, object, fromIndex, toIndex, linkifier);
          else
            treeNode.appendChild(new ObjectUI.ArrayGroupingTreeElement(object, fromIndex, toIndex, count, linkifier));
        }
      }
      if (topLevel) {
        ObjectUI.ArrayGroupingTreeElement._populateNonIndexProperties(
            treeNode, object, result.skipGetOwnPropertyNames, linkifier);
      }
    }
  }

  /**
   * @param {!UI.TreeElement} treeNode
   * @param {!SDK.RemoteObject} object
   * @param {number} fromIndex
   * @param {number} toIndex
   * @param {!Components.Linkifier=} linkifier
   * @return {!Promise}
   * @this {ObjectUI.ArrayGroupingTreeElement}
   */
  static async _populateAsFragment(treeNode, object, fromIndex, toIndex, linkifier) {
    const result = await object.callFunction(
        buildArrayFragment,
        [{value: fromIndex}, {value: toIndex}, {value: ObjectUI.ArrayGroupingTreeElement._sparseIterationThreshold}]);
    if (!result.object || result.wasThrown)
      return;
    const arrayFragment = result.object;
    const allProperties =
        await arrayFragment.getAllProperties(false /* accessorPropertiesOnly */, true /* generatePreview */);
    arrayFragment.release();
    const properties = allProperties.properties;
    if (!properties)
      return;
    properties.sort(ObjectUI.ObjectPropertiesSection.CompareProperties);
    for (let i = 0; i < properties.length; ++i) {
      properties[i].parentObject = this._object;
      const childTreeElement = new ObjectUI.ObjectPropertyTreeElement(properties[i], linkifier);
      childTreeElement._readOnly = true;
      treeNode.appendChild(childTreeElement);
    }

    /**
     * @suppressReceiverCheck
     * @this {Object}
     * @param {number=} fromIndex // must declare optional
     * @param {number=} toIndex // must declare optional
     * @param {number=} sparseIterationThreshold // must declare optional
     */
    function buildArrayFragment(fromIndex, toIndex, sparseIterationThreshold) {
      const result = Object.create(null);
      if (toIndex - fromIndex < sparseIterationThreshold) {
        for (let i = fromIndex; i <= toIndex; ++i) {
          if (i in this)
            result[i] = this[i];
        }
      } else {
        const ownPropertyNames = Object.getOwnPropertyNames(this);
        for (let i = 0; i < ownPropertyNames.length; ++i) {
          const name = ownPropertyNames[i];
          const index = name >>> 0;
          if (String(index) === name && fromIndex <= index && index <= toIndex)
            result[index] = this[index];
        }
      }
      return result;
    }
  }

  /**
   * @param {!UI.TreeElement} treeNode
   * @param {!SDK.RemoteObject} object
   * @param {boolean} skipGetOwnPropertyNames
   * @param {!Components.Linkifier=} linkifier
   * @return {!Promise<undefined>}
   * @this {ObjectUI.ArrayGroupingTreeElement}
   */
  static async _populateNonIndexProperties(treeNode, object, skipGetOwnPropertyNames, linkifier) {
    const result = await object.callFunction(buildObjectFragment, [{value: skipGetOwnPropertyNames}]);
    if (!result.object || result.wasThrown)
      return;
    const allProperties = await result.object.getOwnProperties(true /* generatePreview */);
    result.object.release();
    if (!allProperties.properties)
      return;
    const properties = allProperties.properties;
    properties.sort(ObjectUI.ObjectPropertiesSection.CompareProperties);
    for (let i = 0; i < properties.length; ++i) {
      properties[i].parentObject = this._object;
      const childTreeElement = new ObjectUI.ObjectPropertyTreeElement(properties[i], linkifier);
      childTreeElement._readOnly = true;
      treeNode.appendChild(childTreeElement);
    }

    /**
     * @param {boolean=} skipGetOwnPropertyNames
     * @suppressReceiverCheck
     * @this {Object}
     */
    function buildObjectFragment(skipGetOwnPropertyNames) {
      const result = {__proto__: this.__proto__};
      if (skipGetOwnPropertyNames)
        return result;
      const names = Object.getOwnPropertyNames(this);
      for (let i = 0; i < names.length; ++i) {
        const name = names[i];
        // Array index check according to the ES5-15.4.
        if (String(name >>> 0) === name && name >>> 0 !== 0xffffffff)
          continue;
        const descriptor = Object.getOwnPropertyDescriptor(this, name);
        if (descriptor)
          Object.defineProperty(result, name, descriptor);
      }
      return result;
    }
  }

  /**
   * @override
   */
  onpopulate() {
    if (this._propertyCount >= ObjectUI.ArrayGroupingTreeElement._bucketThreshold) {
      ObjectUI.ArrayGroupingTreeElement._populateRanges(
          this, this._object, this._fromIndex, this._toIndex, false, this._linkifier);
      return;
    }
    ObjectUI.ArrayGroupingTreeElement._populateAsFragment(
        this, this._object, this._fromIndex, this._toIndex, this._linkifier);
  }

  /**
   * @override
   */
  onattach() {
    this.listItemElement.classList.add('object-properties-section-name');
  }
};

ObjectUI.ArrayGroupingTreeElement._bucketThreshold = 100;
ObjectUI.ArrayGroupingTreeElement._sparseIterationThreshold = 250000;
ObjectUI.ArrayGroupingTreeElement._getOwnPropertyNamesThreshold = 500000;


/**
 * @unrestricted
 */
ObjectUI.ObjectPropertyPrompt = class extends UI.TextPrompt {
  constructor() {
    super();
    this.initialize(
        ObjectUI.javaScriptAutocomplete.completionsForTextInCurrentContext.bind(ObjectUI.javaScriptAutocomplete));
  }
};

/**
 * @unrestricted
 */
ObjectUI.ObjectPropertiesSectionExpandController = class {
  constructor() {
    /** @type {!Set.<string>} */
    this._expandedProperties = new Set();
  }

  /**
   * @param {string} id
   * @param {!ObjectUI.ObjectPropertiesSection} section
   */
  watchSection(id, section) {
    section.addEventListener(UI.TreeOutline.Events.ElementAttached, this._elementAttached, this);
    section.addEventListener(UI.TreeOutline.Events.ElementExpanded, this._elementExpanded, this);
    section.addEventListener(UI.TreeOutline.Events.ElementCollapsed, this._elementCollapsed, this);
    section[ObjectUI.ObjectPropertiesSectionExpandController._treeOutlineId] = id;

    if (this._expandedProperties.has(id))
      section.expand();
  }

  /**
   * @param {string} id
   */
  stopWatchSectionsWithId(id) {
    for (const property of this._expandedProperties) {
      if (property.startsWith(id + ':'))
        this._expandedProperties.delete(property);
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _elementAttached(event) {
    const element = /** @type {!UI.TreeElement} */ (event.data);
    if (element.isExpandable() && this._expandedProperties.has(this._propertyPath(element)))
      element.expand();
  }

  /**
   * @param {!Common.Event} event
   */
  _elementExpanded(event) {
    const element = /** @type {!UI.TreeElement} */ (event.data);
    this._expandedProperties.add(this._propertyPath(element));
  }

  /**
   * @param {!Common.Event} event
   */
  _elementCollapsed(event) {
    const element = /** @type {!UI.TreeElement} */ (event.data);
    this._expandedProperties.delete(this._propertyPath(element));
  }

  /**
   * @param {!UI.TreeElement} treeElement
   * @return {string}
   */
  _propertyPath(treeElement) {
    const cachedPropertyPath = treeElement[ObjectUI.ObjectPropertiesSectionExpandController._cachedPathSymbol];
    if (cachedPropertyPath)
      return cachedPropertyPath;

    let current = treeElement;
    const rootElement = treeElement.treeOutline.objectTreeElement();

    let result;

    while (current !== rootElement) {
      let currentName = '';
      if (current.property)
        currentName = current.property.name;
      else
        currentName = typeof current.title === 'string' ? current.title : current.title.textContent;

      result = currentName + (result ? '.' + result : '');
      current = current.parent;
    }
    const treeOutlineId = treeElement.treeOutline[ObjectUI.ObjectPropertiesSectionExpandController._treeOutlineId];
    result = treeOutlineId + (result ? ':' + result : '');
    treeElement[ObjectUI.ObjectPropertiesSectionExpandController._cachedPathSymbol] = result;
    return result;
  }
};

ObjectUI.ObjectPropertiesSectionExpandController._cachedPathSymbol = Symbol('cachedPath');
ObjectUI.ObjectPropertiesSectionExpandController._treeOutlineId = Symbol('treeOutlineId');

/**
 * @implements {Common.Renderer}
 */
ObjectUI.ObjectPropertiesSection.Renderer = class {
  /**
   * @override
   * @param {!Object} object
   * @param {!Common.Renderer.Options=} options
   * @return {!Promise<?Node>}
   */
  render(object, options) {
    if (!(object instanceof SDK.RemoteObject))
      return Promise.reject(new Error('Can\'t render ' + object));
    options = options || {};
    const title = options.title;
    const section = new ObjectUI.ObjectPropertiesSection(object, title);
    if (!title)
      section.titleLessMode();
    if (options.expanded)
      section.expand();
    section.editable = !!options.editable;
    return Promise.resolve(section.element);
  }
};