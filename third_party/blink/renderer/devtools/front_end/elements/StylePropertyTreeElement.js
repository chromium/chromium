// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Elements.StylePropertyTreeElement = class extends UI.TreeElement {
  /**
   * @param {!Elements.StylesSidebarPane} stylesPane
   * @param {!SDK.CSSMatchedStyles} matchedStyles
   * @param {!SDK.CSSProperty} property
   * @param {boolean} isShorthand
   * @param {boolean} inherited
   * @param {boolean} overloaded
   * @param {boolean} newProperty
   */
  constructor(stylesPane, matchedStyles, property, isShorthand, inherited, overloaded, newProperty) {
    // Pass an empty title, the title gets made later in onattach.
    super('', isShorthand);
    this._style = property.ownerStyle;
    this._matchedStyles = matchedStyles;
    this.property = property;
    this._inherited = inherited;
    this._overloaded = overloaded;
    this.selectable = false;
    this._parentPane = stylesPane;
    this.isShorthand = isShorthand;
    this._applyStyleThrottler = new Common.Throttler(0);
    this._newProperty = newProperty;
    if (this._newProperty)
      this.listItemElement.textContent = '';
    this._expandedDueToFilter = false;
    this.valueElement = null;
    this.nameElement = null;
    this._expandElement = null;
    this._originalPropertyText = '';
    this._prompt = null;
    this._propertyHasBeenEditedIncrementally = false;
    this._lastComputedValue = null;
  }

  /**
   * @return {!SDK.CSSMatchedStyles}
   */
  matchedStyles() {
    return this._matchedStyles;
  }

  /**
   * @return {boolean}
   */
  _editable() {
    return !!(this._style.styleSheetId && this._style.range);
  }

  /**
   * @return {boolean}
   */
  inherited() {
    return this._inherited;
  }

  /**
   * @return {boolean}
   */
  overloaded() {
    return this._overloaded;
  }

  /**
   * @param {boolean} x
   */
  setOverloaded(x) {
    if (x === this._overloaded)
      return;
    this._overloaded = x;
    this._updateState();
  }

  get name() {
    return this.property.name;
  }

  get value() {
    return this.property.value;
  }

  /**
   * @return {boolean}
   */
  _updateFilter() {
    const regex = this._parentPane.filterRegex();
    const matches = !!regex && (regex.test(this.property.name) || regex.test(this.property.value));
    this.listItemElement.classList.toggle('filter-match', matches);

    this.onpopulate();
    let hasMatchingChildren = false;
    for (let i = 0; i < this.childCount(); ++i)
      hasMatchingChildren |= this.childAt(i)._updateFilter();

    if (!regex) {
      if (this._expandedDueToFilter)
        this.collapse();
      this._expandedDueToFilter = false;
    } else if (hasMatchingChildren && !this.expanded) {
      this.expand();
      this._expandedDueToFilter = true;
    } else if (!hasMatchingChildren && this.expanded && this._expandedDueToFilter) {
      this.collapse();
      this._expandedDueToFilter = false;
    }
    return matches;
  }

  /**
   * @param {string} text
   * @return {!Node}
   */
  _processColor(text) {
    // We can be called with valid non-color values of |text| (like 'none' from border style)
    const color = Common.Color.parse(text);
    if (!color)
      return createTextNode(text);

    if (!this._editable()) {
      const swatch = InlineEditor.ColorSwatch.create();
      swatch.setColor(color);
      return swatch;
    }

    const swatchPopoverHelper = this._parentPane.swatchPopoverHelper();
    const swatch = InlineEditor.ColorSwatch.create();
    swatch.setColor(color);
    swatch.setFormat(Common.Color.detectColorFormat(swatch.color()));
    this._addColorContrastInfo(new Elements.ColorSwatchPopoverIcon(this, swatchPopoverHelper, swatch));

    return swatch;
  }

  /**
   * @param {string} text
   * @return {!Node}
   */
  _processVar(text) {
    const computedValue = this._matchedStyles.computeValue(this._style, text);
    if (!computedValue)
      return createTextNode(text);
    const color = Common.Color.parse(computedValue);
    if (!color) {
      const node = createElement('span');
      node.textContent = text;
      node.title = computedValue;
      return node;
    }
    if (!this._editable()) {
      const swatch = InlineEditor.ColorSwatch.create();
      swatch.setText(text, computedValue);
      swatch.setColor(color);
      return swatch;
    }

    const swatchPopoverHelper = this._parentPane.swatchPopoverHelper();
    const swatch = InlineEditor.ColorSwatch.create();
    swatch.setColor(color);
    swatch.setFormat(Common.Color.detectColorFormat(swatch.color()));
    swatch.setText(text, computedValue);
    this._addColorContrastInfo(new Elements.ColorSwatchPopoverIcon(this, swatchPopoverHelper, swatch));
    return swatch;
  }

  /**
   * @param {!Elements.ColorSwatchPopoverIcon} swatchIcon
   */
  async _addColorContrastInfo(swatchIcon) {
    if (!Runtime.experiments.isEnabled('colorContrastRatio') || this.property.name !== 'color' ||
        !this._parentPane.cssModel() || !this.node())
      return;
    const cssModel = this._parentPane.cssModel();
    const contrastInfo = await cssModel.backgroundColorsPromise(this.node().id);
    swatchIcon.setContrastInfo(contrastInfo);
  }

  /**
   * @return {string}
   */
  renderedPropertyText() {
    return this.nameElement.textContent + ': ' + this.valueElement.textContent;
  }

  /**
   * @param {string} text
   * @return {!Node}
   */
  _processBezier(text) {
    if (!this._editable() || !UI.Geometry.CubicBezier.parse(text))
      return createTextNode(text);
    const swatchPopoverHelper = this._parentPane.swatchPopoverHelper();
    const swatch = InlineEditor.BezierSwatch.create();
    swatch.setBezierText(text);
    new Elements.BezierPopoverIcon(this, swatchPopoverHelper, swatch);
    return swatch;
  }

  /**
   * @param {string} propertyValue
   * @param {string} propertyName
   * @return {!Node}
   */
  _processShadow(propertyValue, propertyName) {
    if (!this._editable())
      return createTextNode(propertyValue);
    let shadows;
    if (propertyName === 'text-shadow')
      shadows = InlineEditor.CSSShadowModel.parseTextShadow(propertyValue);
    else
      shadows = InlineEditor.CSSShadowModel.parseBoxShadow(propertyValue);
    if (!shadows.length)
      return createTextNode(propertyValue);
    const container = createDocumentFragment();
    const swatchPopoverHelper = this._parentPane.swatchPopoverHelper();
    for (let i = 0; i < shadows.length; i++) {
      if (i !== 0)
        container.appendChild(createTextNode(', '));  // Add back commas and spaces between each shadow.
      // TODO(flandy): editing the property value should use the original value with all spaces.
      const cssShadowSwatch = InlineEditor.CSSShadowSwatch.create();
      cssShadowSwatch.setCSSShadow(shadows[i]);
      new Elements.ShadowSwatchPopoverHelper(this, swatchPopoverHelper, cssShadowSwatch);
      const colorSwatch = cssShadowSwatch.colorSwatch();
      if (colorSwatch)
        new Elements.ColorSwatchPopoverIcon(this, swatchPopoverHelper, colorSwatch);
      container.appendChild(cssShadowSwatch);
    }
    return container;
  }

  _updateState() {
    if (!this.listItemElement)
      return;

    if (this._style.isPropertyImplicit(this.name))
      this.listItemElement.classList.add('implicit');
    else
      this.listItemElement.classList.remove('implicit');

    const hasIgnorableError =
        !this.property.parsedOk && Elements.StylesSidebarPane.ignoreErrorsForProperty(this.property);
    if (hasIgnorableError)
      this.listItemElement.classList.add('has-ignorable-error');
    else
      this.listItemElement.classList.remove('has-ignorable-error');

    if (this.inherited())
      this.listItemElement.classList.add('inherited');
    else
      this.listItemElement.classList.remove('inherited');

    if (this.overloaded())
      this.listItemElement.classList.add('overloaded');
    else
      this.listItemElement.classList.remove('overloaded');

    if (this.property.disabled)
      this.listItemElement.classList.add('disabled');
    else
      this.listItemElement.classList.remove('disabled');
  }

  /**
   * @return {?SDK.DOMNode}
   */
  node() {
    return this._parentPane.node();
  }

  /**
   * @return {!Elements.StylesSidebarPane}
   */
  parentPane() {
    return this._parentPane;
  }

  /**
   * @return {?Elements.StylePropertiesSection}
   */
  section() {
    return this.treeOutline && this.treeOutline.section;
  }

  _updatePane() {
    const section = this.section();
    if (section)
      section.refreshUpdate(this);
  }

  /**
   * @param {!Event} event
   */
  _toggleEnabled(event) {
    const disabled = !event.target.checked;
    const oldStyleRange = this._style.range;
    if (!oldStyleRange)
      return;

    /**
     * @param {boolean} success
     * @this {Elements.StylePropertyTreeElement}
     */
    function callback(success) {
      this._parentPane.setUserOperation(false);

      if (!success)
        return;
      this._matchedStyles.resetActiveProperties();
      this._updatePane();
      this.styleTextAppliedForTest();
    }

    event.consume();
    this._parentPane.setUserOperation(true);
    this.property.setDisabled(disabled).then(callback.bind(this));
  }

  /**
   * @override
   */
  onpopulate() {
    // Only populate once and if this property is a shorthand.
    if (this.childCount() || !this.isShorthand)
      return;

    const longhandProperties = this._style.longhandProperties(this.name);
    for (let i = 0; i < longhandProperties.length; ++i) {
      const name = longhandProperties[i].name;
      let inherited = false;
      let overloaded = false;

      const section = this.section();
      if (section) {
        inherited = section.isPropertyInherited(name);
        overloaded =
            this._matchedStyles.propertyState(longhandProperties[i]) === SDK.CSSMatchedStyles.PropertyState.Overloaded;
      }

      const item = new Elements.StylePropertyTreeElement(
          this._parentPane, this._matchedStyles, longhandProperties[i], false, inherited, overloaded, false);
      this.appendChild(item);
    }
  }

  /**
   * @override
   */
  onattach() {
    this.updateTitle();

    this.listItemElement.addEventListener('mousedown', event => {
      if (event.which === 1)
        this._parentPane[Elements.StylePropertyTreeElement.ActiveSymbol] = this;
    }, false);
    this.listItemElement.addEventListener('mouseup', this._mouseUp.bind(this));
    this.listItemElement.addEventListener('click', event => {
      if (!event.target.hasSelection() && event.target !== this.listItemElement)
        event.consume(true);
    });
  }

  /**
   * @override
   */
  onexpand() {
    this._updateExpandElement();
  }

  /**
   * @override
   */
  oncollapse() {
    this._updateExpandElement();
  }

  _updateExpandElement() {
    if (!this._expandElement)
      return;
    if (this.expanded)
      this._expandElement.setIconType('smallicon-triangle-down');
    else
      this._expandElement.setIconType('smallicon-triangle-right');
  }

  updateTitleIfComputedValueChanged() {
    const computedValue = this._matchedStyles.computeValue(this.property.ownerStyle, this.property.value);
    if (computedValue === this._lastComputedValue)
      return;
    this._lastComputedValue = computedValue;
    this._innerUpdateTitle();
  }

  updateTitle() {
    this._lastComputedValue = this._matchedStyles.computeValue(this.property.ownerStyle, this.property.value);
    this._innerUpdateTitle();
  }

  _innerUpdateTitle() {
    this._updateState();
    if (this.isExpandable())
      this._expandElement = UI.Icon.create('smallicon-triangle-right', 'expand-icon');
    else
      this._expandElement = null;

    const propertyRenderer =
        new Elements.StylesSidebarPropertyRenderer(this._style.parentRule, this.node(), this.name, this.value);
    if (this.property.parsedOk) {
      propertyRenderer.setVarHandler(this._processVar.bind(this));
      propertyRenderer.setColorHandler(this._processColor.bind(this));
      propertyRenderer.setBezierHandler(this._processBezier.bind(this));
      propertyRenderer.setShadowHandler(this._processShadow.bind(this));
    }

    this.listItemElement.removeChildren();
    this.nameElement = propertyRenderer.renderName();
    if (this.property.name.startsWith('--'))
      this.nameElement.title = this._matchedStyles.computeCSSVariable(this._style, this.property.name) || '';
    this.valueElement = propertyRenderer.renderValue();
    if (!this.treeOutline)
      return;

    const indent = Common.moduleSetting('textEditorIndent').get();
    this.listItemElement.createChild('span', 'styles-clipboard-only')
        .createTextChild(indent + (this.property.disabled ? '/* ' : ''));
    this.listItemElement.appendChild(this.nameElement);
    this.listItemElement.createTextChild(': ');
    if (this._expandElement)
      this.listItemElement.appendChild(this._expandElement);
    this.listItemElement.appendChild(this.valueElement);
    this.listItemElement.createTextChild(';');
    if (this.property.disabled)
      this.listItemElement.createChild('span', 'styles-clipboard-only').createTextChild(' */');

    if (!this.property.parsedOk) {
      // Avoid having longhands under an invalid shorthand.
      this.listItemElement.classList.add('not-parsed-ok');

      // Add a separate exclamation mark IMG element with a tooltip.
      this.listItemElement.insertBefore(
          Elements.StylesSidebarPane.createExclamationMark(this.property), this.listItemElement.firstChild);
    }
    if (!this.property.activeInStyle())
      this.listItemElement.classList.add('inactive');
    this._updateFilter();

    if (this.property.parsedOk && this.section() && this.parent.root) {
      const enabledCheckboxElement = createElement('input');
      enabledCheckboxElement.className = 'enabled-button';
      enabledCheckboxElement.type = 'checkbox';
      enabledCheckboxElement.checked = !this.property.disabled;
      enabledCheckboxElement.addEventListener('mousedown', event => event.consume(), false);
      enabledCheckboxElement.addEventListener('click', this._toggleEnabled.bind(this), false);
      this.listItemElement.insertBefore(enabledCheckboxElement, this.listItemElement.firstChild);
    }
  }

  /**
   * @param {!Event} event
   */
  _mouseUp(event) {
    const activeTreeElement = this._parentPane[Elements.StylePropertyTreeElement.ActiveSymbol];
    this._parentPane[Elements.StylePropertyTreeElement.ActiveSymbol] = null;
    if (activeTreeElement !== this)
      return;
    if (this.listItemElement.hasSelection())
      return;
    if (UI.isBeingEdited(/** @type {!Node} */ (event.target)))
      return;

    event.consume(true);

    if (event.target === this.listItemElement)
      return;

    if (UI.KeyboardShortcut.eventHasCtrlOrMeta(/** @type {!MouseEvent} */ (event)) && this.section().navigable) {
      this._navigateToSource(/** @type {!Element} */ (event.target));
      return;
    }

    this.startEditing(/** @type {!Element} */ (event.target));
  }

  /**
   * @param {!Element} element
   * @param {boolean=} omitFocus
   */
  _navigateToSource(element, omitFocus) {
    if (!this.section().navigable)
      return;
    const propertyNameClicked = element === this.nameElement;
    const uiLocation = Bindings.cssWorkspaceBinding.propertyUILocation(this.property, propertyNameClicked);
    if (uiLocation)
      Common.Revealer.reveal(uiLocation, omitFocus);
  }

  /**
   * @param {?Element=} selectElement
   */
  startEditing(selectElement) {
    // FIXME: we don't allow editing of longhand properties under a shorthand right now.
    if (this.parent.isShorthand)
      return;

    if (this._expandElement && selectElement === this._expandElement)
      return;

    const section = this.section();
    if (section && !section.editable)
      return;

    if (selectElement) {
      selectElement = selectElement.enclosingNodeOrSelfWithClass('webkit-css-property') ||
          selectElement.enclosingNodeOrSelfWithClass('value');
    }
    if (!selectElement)
      selectElement = this.nameElement;

    if (UI.isBeingEdited(selectElement))
      return;

    const isEditingName = selectElement === this.nameElement;
    if (!isEditingName)
      this.valueElement.textContent = restoreURLs(this.valueElement.textContent, this.value);

    /**
     * @param {string} fieldValue
     * @param {string} modelValue
     * @return {string}
     */
    function restoreURLs(fieldValue, modelValue) {
      const urlRegex = /\b(url\([^)]*\))/g;
      const splitFieldValue = fieldValue.split(urlRegex);
      if (splitFieldValue.length === 1)
        return fieldValue;
      const modelUrlRegex = new RegExp(urlRegex);
      for (let i = 1; i < splitFieldValue.length; i += 2) {
        const match = modelUrlRegex.exec(modelValue);
        if (match)
          splitFieldValue[i] = match[0];
      }
      return splitFieldValue.join('');
    }

    /** @type {!Elements.StylePropertyTreeElement.Context} */
    const context = {
      expanded: this.expanded,
      hasChildren: this.isExpandable(),
      isEditingName: isEditingName,
      previousContent: selectElement.textContent
    };

    // Lie about our children to prevent expanding on double click and to collapse shorthands.
    this.setExpandable(false);

    if (selectElement.parentElement)
      selectElement.parentElement.classList.add('child-editing');
    selectElement.textContent = selectElement.textContent;  // remove color swatch and the like

    /**
     * @param {!Elements.StylePropertyTreeElement.Context} context
     * @param {!Event} event
     * @this {Elements.StylePropertyTreeElement}
     */
    function pasteHandler(context, event) {
      const data = event.clipboardData.getData('Text');
      if (!data)
        return;
      const colonIdx = data.indexOf(':');
      if (colonIdx < 0)
        return;
      const name = data.substring(0, colonIdx).trim();
      const value = data.substring(colonIdx + 1).trim();

      event.preventDefault();

      if (!('originalName' in context)) {
        context.originalName = this.nameElement.textContent;
        context.originalValue = this.valueElement.textContent;
      }
      this.property.name = name;
      this.property.value = value;
      this.nameElement.textContent = name;
      this.valueElement.textContent = value;
      this.nameElement.normalize();
      this.valueElement.normalize();

      this._editingCommitted(event.target.textContent, context, 'forward');
    }

    /**
     * @param {!Elements.StylePropertyTreeElement.Context} context
     * @param {!Event} event
     * @this {Elements.StylePropertyTreeElement}
     */
    function blurListener(context, event) {
      let text = event.target.textContent;
      if (!context.isEditingName)
        text = this.value || text;
      this._editingCommitted(text, context, '');
    }

    this._originalPropertyText = this.property.propertyText;

    this._parentPane.setEditingStyle(true);
    if (selectElement.parentElement)
      selectElement.parentElement.scrollIntoViewIfNeeded(false);

    this._prompt = new Elements.StylesSidebarPane.CSSPropertyPrompt(this, isEditingName);
    this._prompt.setAutocompletionTimeout(0);
    if (section)
      section.startEditing();

    // Do not live-edit "content" property of pseudo elements. crbug.com/433889
    if (!isEditingName && (!this._parentPane.node().pseudoType() || this.name !== 'content'))
      this._prompt.addEventListener(UI.TextPrompt.Events.TextChanged, this._applyFreeFlowStyleTextEdit.bind(this));

    const proxyElement = this._prompt.attachAndStartEditing(selectElement, blurListener.bind(this, context));
    this._navigateToSource(selectElement, true);

    proxyElement.addEventListener('keydown', this._editingNameValueKeyDown.bind(this, context), false);
    proxyElement.addEventListener('keypress', this._editingNameValueKeyPress.bind(this, context), false);
    if (isEditingName)
      proxyElement.addEventListener('paste', pasteHandler.bind(this, context), false);

    selectElement.getComponentSelection().selectAllChildren(selectElement);
  }

  /**
   * @param {!Elements.StylePropertyTreeElement.Context} context
   * @param {!Event} event
   */
  _editingNameValueKeyDown(context, event) {
    if (event.handled)
      return;

    let result;

    if (isEnterKey(event)) {
      result = 'forward';
    } else if (event.keyCode === UI.KeyboardShortcut.Keys.Esc.code || event.key === 'Escape') {
      result = 'cancel';
    } else if (
        !context.isEditingName && this._newProperty && event.keyCode === UI.KeyboardShortcut.Keys.Backspace.code) {
      // For a new property, when Backspace is pressed at the beginning of new property value, move back to the property name.
      const selection = event.target.getComponentSelection();
      if (selection.isCollapsed && !selection.focusOffset) {
        event.preventDefault();
        result = 'backward';
      }
    } else if (event.key === 'Tab') {
      result = event.shiftKey ? 'backward' : 'forward';
      event.preventDefault();
    }

    if (result) {
      switch (result) {
        case 'cancel':
          this.editingCancelled(null, context);
          break;
        case 'forward':
        case 'backward':
          this._editingCommitted(event.target.textContent, context, result);
          break;
      }

      event.consume();
      return;
    }
  }

  /**
   * @param {!Elements.StylePropertyTreeElement.Context} context
   * @param {!Event} event
   */
  _editingNameValueKeyPress(context, event) {
    /**
     * @param {string} text
     * @param {number} cursorPosition
     * @return {boolean}
     */
    function shouldCommitValueSemicolon(text, cursorPosition) {
      // FIXME: should this account for semicolons inside comments?
      let openQuote = '';
      for (let i = 0; i < cursorPosition; ++i) {
        const ch = text[i];
        if (ch === '\\' && openQuote !== '')
          ++i;  // skip next character inside string
        else if (!openQuote && (ch === '"' || ch === '\''))
          openQuote = ch;
        else if (openQuote === ch)
          openQuote = '';
      }
      return !openQuote;
    }

    const keyChar = String.fromCharCode(event.charCode);
    const isFieldInputTerminated =
        (context.isEditingName ? keyChar === ':' :
                                 keyChar === ';' &&
                 shouldCommitValueSemicolon(event.target.textContent, event.target.selectionLeftOffset()));
    if (isFieldInputTerminated) {
      // Enter or colon (for name)/semicolon outside of string (for value).
      event.consume(true);
      this._editingCommitted(event.target.textContent, context, 'forward');
      return;
    }
  }

  /**
   * @return {!Promise}
   */
  async _applyFreeFlowStyleTextEdit() {
    const valueText = this._prompt.textWithCurrentSuggestion();
    if (valueText.indexOf(';') === -1)
      await this.applyStyleText(this.nameElement.textContent + ': ' + valueText, false);
  }

  /**
   * @return {!Promise}
   */
  kickFreeFlowStyleEditForTest() {
    return this._applyFreeFlowStyleTextEdit();
  }

  /**
   * @param {!Elements.StylePropertyTreeElement.Context} context
   */
  editingEnded(context) {
    this.setExpandable(context.hasChildren);
    if (context.expanded)
      this.expand();
    const editedElement = context.isEditingName ? this.nameElement : this.valueElement;
    // The proxyElement has been deleted, no need to remove listener.
    if (editedElement.parentElement)
      editedElement.parentElement.classList.remove('child-editing');

    this._parentPane.setEditingStyle(false);
  }

  /**
   * @param {?Element} element
   * @param {!Elements.StylePropertyTreeElement.Context} context
   */
  editingCancelled(element, context) {
    this._removePrompt();
    this._revertStyleUponEditingCanceled();
    // This should happen last, as it clears the info necessary to restore the property value after [Page]Up/Down changes.
    this.editingEnded(context);
  }

  _revertStyleUponEditingCanceled() {
    if (this._propertyHasBeenEditedIncrementally) {
      this.applyStyleText(this._originalPropertyText, false);
      this._originalPropertyText = '';
    } else if (this._newProperty) {
      this.treeOutline.removeChild(this);
    } else {
      this.updateTitle();
    }
  }

  /**
   * @param {string} moveDirection
   * @return {?Elements.StylePropertyTreeElement}
   */
  _findSibling(moveDirection) {
    let target = this;
    do
      target = (moveDirection === 'forward' ? target.nextSibling : target.previousSibling);
    while (target && target.inherited());

    return target;
  }

  /**
   * @param {string} userInput
   * @param {!Elements.StylePropertyTreeElement.Context} context
   * @param {string} moveDirection
   */
  async _editingCommitted(userInput, context, moveDirection) {
    const hadFocus = this._parentPane.element.hasFocus();
    this._removePrompt();
    this.editingEnded(context);
    const isEditingName = context.isEditingName;

    // Determine where to move to before making changes
    let createNewProperty, moveToSelector;
    const isDataPasted = 'originalName' in context;
    const isDirtyViaPaste = isDataPasted &&
        (this.nameElement.textContent !== context.originalName ||
         this.valueElement.textContent !== context.originalValue);
    const isPropertySplitPaste =
        isDataPasted && isEditingName && this.valueElement.textContent !== context.originalValue;
    let moveTo = this;
    const moveToOther = (isEditingName ^ (moveDirection === 'forward'));
    const abandonNewProperty = this._newProperty && !userInput && (moveToOther || isEditingName);
    if (moveDirection === 'forward' && (!isEditingName || isPropertySplitPaste) ||
        moveDirection === 'backward' && isEditingName) {
      moveTo = moveTo._findSibling(moveDirection);
      if (!moveTo) {
        if (moveDirection === 'forward' && (!this._newProperty || userInput))
          createNewProperty = true;
        else if (moveDirection === 'backward')
          moveToSelector = true;
      }
    }

    // Make the Changes and trigger the moveToNextCallback after updating.
    let moveToIndex = moveTo && this.treeOutline ? this.treeOutline.rootElement().indexOfChild(moveTo) : -1;
    const blankInput = userInput.isWhitespace();
    const shouldCommitNewProperty = this._newProperty &&
        (isPropertySplitPaste || moveToOther || (!moveDirection && !isEditingName) || (isEditingName && blankInput));
    const section = /** @type {!Elements.StylePropertiesSection} */ (this.section());
    if (((userInput !== context.previousContent || isDirtyViaPaste) && !this._newProperty) || shouldCommitNewProperty) {
      if (hadFocus)
        this._parentPane.element.focus();
      let propertyText;
      if (blankInput || (this._newProperty && this.valueElement.textContent.isWhitespace())) {
        propertyText = '';
      } else {
        if (isEditingName)
          propertyText = userInput + ': ' + this.property.value;
        else
          propertyText = this.property.name + ': ' + userInput;
      }
      await this.applyStyleText(propertyText, true);
      moveToNextCallback.call(this, this._newProperty, !blankInput, section);
    } else {
      if (isEditingName)
        this.property.name = userInput;
      else
        this.property.value = userInput;
      if (!isDataPasted && !this._newProperty)
        this.updateTitle();
      moveToNextCallback.call(this, this._newProperty, false, section);
    }

    /**
     * The Callback to start editing the next/previous property/selector.
     * @param {boolean} alreadyNew
     * @param {boolean} valueChanged
     * @param {!Elements.StylePropertiesSection} section
     * @this {Elements.StylePropertyTreeElement}
     */
    function moveToNextCallback(alreadyNew, valueChanged, section) {
      if (!moveDirection)
        return;

      // User just tabbed through without changes.
      if (moveTo && moveTo.parent) {
        moveTo.startEditing(!isEditingName ? moveTo.nameElement : moveTo.valueElement);
        return;
      }

      // User has made a change then tabbed, wiping all the original treeElements.
      // Recalculate the new treeElement for the same property we were going to edit next.
      if (moveTo && !moveTo.parent) {
        const rootElement = section.propertiesTreeOutline.rootElement();
        if (moveDirection === 'forward' && blankInput && !isEditingName)
          --moveToIndex;
        if (moveToIndex >= rootElement.childCount() && !this._newProperty) {
          createNewProperty = true;
        } else {
          const treeElement = moveToIndex >= 0 ? rootElement.childAt(moveToIndex) : null;
          if (treeElement) {
            let elementToEdit =
                !isEditingName || isPropertySplitPaste ? treeElement.nameElement : treeElement.valueElement;
            if (alreadyNew && blankInput)
              elementToEdit = moveDirection === 'forward' ? treeElement.nameElement : treeElement.valueElement;
            treeElement.startEditing(elementToEdit);
            return;
          } else if (!alreadyNew) {
            moveToSelector = true;
          }
        }
      }

      // Create a new attribute in this section (or move to next editable selector if possible).
      if (createNewProperty) {
        if (alreadyNew && !valueChanged && (isEditingName ^ (moveDirection === 'backward')))
          return;

        section.addNewBlankProperty().startEditing();
        return;
      }

      if (abandonNewProperty) {
        moveTo = this._findSibling(moveDirection);
        const sectionToEdit = (moveTo || moveDirection === 'backward') ? section : section.nextEditableSibling();
        if (sectionToEdit) {
          if (sectionToEdit.style().parentRule)
            sectionToEdit.startEditingSelector();
          else
            sectionToEdit.moveEditorFromSelector(moveDirection);
        }
        return;
      }

      if (moveToSelector) {
        if (section.style().parentRule)
          section.startEditingSelector();
        else
          section.moveEditorFromSelector(moveDirection);
      }
    }
  }

  _removePrompt() {
    // BUG 53242. This cannot go into editingEnded(), as it should always happen first for any editing outcome.
    if (this._prompt) {
      this._prompt.detach();
      this._prompt = null;
    }
    const section = this.section();
    if (section)
      section.stopEditing();
  }

  styleTextAppliedForTest() {
  }

  /**
   * @param {string} styleText
   * @param {boolean} majorChange
   * @return {!Promise}
   */
  applyStyleText(styleText, majorChange) {
    return this._applyStyleThrottler.schedule(this._innerApplyStyleText.bind(this, styleText, majorChange));
  }

  /**
   * @param {string} styleText
   * @param {boolean} majorChange
   * @return {!Promise}
   */
  async _innerApplyStyleText(styleText, majorChange) {
    if (!this.treeOutline)
      return;

    const oldStyleRange = this._style.range;
    if (!oldStyleRange)
      return;

    styleText = styleText.replace(/\s/g, ' ').trim();  // Replace &nbsp; with whitespace.
    if (!styleText.length && majorChange && this._newProperty && !this._propertyHasBeenEditedIncrementally) {
      // The user deleted everything and never applied a new property value via Up/Down scrolling/live editing, so remove the tree element and update.
      this.parent.removeChild(this);
      return;
    }

    const currentNode = this._parentPane.node();
    this._parentPane.setUserOperation(true);

    // Append a ";" if the new text does not end in ";".
    // FIXME: this does not handle trailing comments.
    if (styleText.length && !/;\s*$/.test(styleText))
      styleText += ';';
    const overwriteProperty = !this._newProperty || this._propertyHasBeenEditedIncrementally;
    let success = await this.property.setText(styleText, majorChange, overwriteProperty);
    // Revert to the original text if applying the new text failed
    if (this._propertyHasBeenEditedIncrementally && majorChange && !success) {
      majorChange = false;
      success = await this.property.setText(this._originalPropertyText, majorChange, overwriteProperty);
    }
    this._parentPane.setUserOperation(false);

    if (!success) {
      if (majorChange) {
        // It did not apply, cancel editing.
        if (this._newProperty)
          this.treeOutline.removeChild(this);
        else
          this.updateTitle();
      }
      this.styleTextAppliedForTest();
      return;
    }

    this._matchedStyles.resetActiveProperties();
    this._propertyHasBeenEditedIncrementally = true;
    this.property = this._style.propertyAt(this.property.index);

    if (currentNode === this.node())
      this._updatePane();

    this.styleTextAppliedForTest();
  }

  /**
   * @override
   * @return {boolean}
   */
  ondblclick() {
    return true;  // handled
  }

  /**
   * @override
   * @param {!Event} event
   * @return {boolean}
   */
  isEventWithinDisclosureTriangle(event) {
    return event.target === this._expandElement;
  }
};

/** @typedef {{expanded: boolean, hasChildren: boolean, isEditingName: boolean, previousContent: string}} */
Elements.StylePropertyTreeElement.Context;
Elements.StylePropertyTreeElement.ActiveSymbol = Symbol('ActiveSymbol');
