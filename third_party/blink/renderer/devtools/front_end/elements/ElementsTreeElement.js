/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Joseph Pecoraro
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
Elements.ElementsTreeElement = class extends UI.TreeElement {
  /**
   * @param {!SDK.DOMNode} node
   * @param {boolean=} elementCloseTag
   */
  constructor(node, elementCloseTag) {
    // The title will be updated in onattach.
    super();
    this._node = node;

    this._gutterContainer = this.listItemElement.createChild('div', 'gutter-container');
    this._gutterContainer.addEventListener('click', this._showContextMenu.bind(this));
    const gutterMenuIcon = UI.Icon.create('largeicon-menu', 'gutter-menu-icon');
    this._gutterContainer.appendChild(gutterMenuIcon);
    this._decorationsElement = this._gutterContainer.createChild('div', 'hidden');

    this._elementCloseTag = elementCloseTag;

    if (this._node.nodeType() === Node.ELEMENT_NODE && !elementCloseTag)
      this._canAddAttributes = true;
    this._searchQuery = null;
    this._expandedChildrenLimit = Elements.ElementsTreeElement.InitialChildrenLimit;
    this._decorationsThrottler = new Common.Throttler(100);
  }

  /**
   * @param {!Elements.ElementsTreeElement} treeElement
   */
  static animateOnDOMUpdate(treeElement) {
    const tagName = treeElement.listItemElement.querySelector('.webkit-html-tag-name');
    UI.runCSSAnimationOnce(tagName || treeElement.listItemElement, 'dom-update-highlight');
  }

  /**
   * @param {!SDK.DOMNode} node
   * @return {!Array<!SDK.DOMNode>}
   */
  static visibleShadowRoots(node) {
    let roots = node.shadowRoots();
    if (roots.length && !Common.moduleSetting('showUAShadowDOM').get())
      roots = roots.filter(filter);

    /**
     * @param {!SDK.DOMNode} root
     */
    function filter(root) {
      return root.shadowRootType() !== SDK.DOMNode.ShadowRootTypes.UserAgent;
    }
    return roots;
  }

  /**
   * @param {!SDK.DOMNode} node
   * @return {boolean}
   */
  static canShowInlineText(node) {
    if (node.contentDocument() || node.importedDocument() || node.templateContent() ||
        Elements.ElementsTreeElement.visibleShadowRoots(node).length || node.hasPseudoElements())
      return false;
    if (node.nodeType() !== Node.ELEMENT_NODE)
      return false;
    if (!node.firstChild || node.firstChild !== node.lastChild || node.firstChild.nodeType() !== Node.TEXT_NODE)
      return false;
    const textChild = node.firstChild;
    const maxInlineTextChildLength = 80;
    if (textChild.nodeValue().length < maxInlineTextChildLength)
      return true;
    return false;
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   * @param {!SDK.DOMNode} node
   * @suppressGlobalPropertiesCheck
   */
  static populateForcedPseudoStateItems(contextMenu, node) {
    const pseudoClasses = ['active', 'hover', 'focus', 'visited', 'focus-within'];
    try {
      document.querySelector(':focus-visible');  // Will throw if not supported
      pseudoClasses.push('focus-visible');
    } catch (e) {
    }
    const forcedPseudoState = node.domModel().cssModel().pseudoState(node);
    const stateMenu = contextMenu.debugSection().appendSubMenuItem(Common.UIString('Force state'));
    for (let i = 0; i < pseudoClasses.length; ++i) {
      const pseudoClassForced = forcedPseudoState.indexOf(pseudoClasses[i]) >= 0;
      stateMenu.defaultSection().appendCheckboxItem(
          ':' + pseudoClasses[i], setPseudoStateCallback.bind(null, pseudoClasses[i], !pseudoClassForced),
          pseudoClassForced, false);
    }

    /**
     * @param {string} pseudoState
     * @param {boolean} enabled
     */
    function setPseudoStateCallback(pseudoState, enabled) {
      node.domModel().cssModel().forcePseudoState(node, pseudoState, enabled);
    }
  }

  /**
   * @return {boolean}
   */
  isClosingTag() {
    return !!this._elementCloseTag;
  }

  /**
   * @return {!SDK.DOMNode}
   */
  node() {
    return this._node;
  }

  /**
   * @return {boolean}
   */
  isEditing() {
    return !!this._editing;
  }

  /**
   * @param {string} searchQuery
   */
  highlightSearchResults(searchQuery) {
    if (this._searchQuery !== searchQuery)
      this._hideSearchHighlight();

    this._searchQuery = searchQuery;
    this._searchHighlightsVisible = true;
    this.updateTitle(null, true);
  }

  hideSearchHighlights() {
    delete this._searchHighlightsVisible;
    this._hideSearchHighlight();
  }

  _hideSearchHighlight() {
    if (!this._highlightResult)
      return;

    function updateEntryHide(entry) {
      switch (entry.type) {
        case 'added':
          entry.node.remove();
          break;
        case 'changed':
          entry.node.textContent = entry.oldText;
          break;
      }
    }

    for (let i = (this._highlightResult.length - 1); i >= 0; --i)
      updateEntryHide(this._highlightResult[i]);

    delete this._highlightResult;
  }

  /**
   * @param {boolean} inClipboard
   */
  setInClipboard(inClipboard) {
    if (this._inClipboard === inClipboard)
      return;
    this._inClipboard = inClipboard;
    this.listItemElement.classList.toggle('in-clipboard', inClipboard);
  }

  get hovered() {
    return this._hovered;
  }

  set hovered(x) {
    if (this._hovered === x)
      return;

    this._hovered = x;

    if (this.listItemElement) {
      if (x) {
        this._createSelection();
        this.listItemElement.classList.add('hovered');
      } else {
        this.listItemElement.classList.remove('hovered');
      }
    }
  }

  /**
   * @return {number}
   */
  expandedChildrenLimit() {
    return this._expandedChildrenLimit;
  }

  /**
   * @param {number} expandedChildrenLimit
   */
  setExpandedChildrenLimit(expandedChildrenLimit) {
    this._expandedChildrenLimit = expandedChildrenLimit;
  }

  _createSelection() {
    const listItemElement = this.listItemElement;
    if (!listItemElement)
      return;

    if (!this.selectionElement) {
      this.selectionElement = createElement('div');
      this.selectionElement.className = 'selection fill';
      this.selectionElement.style.setProperty('margin-left', (-this._computeLeftIndent()) + 'px');
      listItemElement.insertBefore(this.selectionElement, listItemElement.firstChild);
    }
  }

  _createHint() {
    if (this.listItemElement && !this._hintElement) {
      this._hintElement = this.listItemElement.createChild('span', 'selected-hint');
      this._hintElement.title = Common.UIString('Use $0 in the console to refer to this element.');
      UI.ARIAUtils.markAsHidden(this._hintElement);
    }
  }

  /**
   * @override
   */
  onbind() {
    if (!this._elementCloseTag)
      this._node[this.treeOutline.treeElementSymbol()] = this;
  }

  /**
   * @override
   */
  onunbind() {
    if (this._node[this.treeOutline.treeElementSymbol()] === this)
      this._node[this.treeOutline.treeElementSymbol()] = null;
  }

  /**
   * @override
   */
  onattach() {
    if (this._hovered) {
      this._createSelection();
      this.listItemElement.classList.add('hovered');
    }

    this.updateTitle();
    this.listItemElement.draggable = true;
  }

  /**
   * @override
   */
  onpopulate() {
    this.treeOutline.populateTreeElement(this);
  }

  /**
   * @override
   */
  expandRecursively() {
    this._node.getSubtree(-1, true).then(UI.TreeElement.prototype.expandRecursively.bind(this, Number.MAX_VALUE));
  }

  /**
   * @override
   */
  onexpand() {
    if (this._elementCloseTag)
      return;

    this.updateTitle();
  }

  /**
   * @override
   */
  oncollapse() {
    if (this._elementCloseTag)
      return;

    this.updateTitle();
  }

  /**
   * @override
   * @param {boolean=} omitFocus
   * @param {boolean=} selectedByUser
   * @return {boolean}
   */
  select(omitFocus, selectedByUser) {
    if (this._editing)
      return false;
    return super.select(omitFocus, selectedByUser);
  }

  /**
   * @override
   * @param {boolean=} selectedByUser
   * @return {boolean}
   */
  onselect(selectedByUser) {
    this.treeOutline.suppressRevealAndSelect = true;
    this.treeOutline.selectDOMNode(this._node, selectedByUser);
    if (selectedByUser) {
      this._node.highlight();
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.ChangeInspectedNodeInElementsPanel);
    }
    this._createSelection();
    this._createHint();
    this.treeOutline.suppressRevealAndSelect = false;
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  ondelete() {
    const startTagTreeElement = this.treeOutline.findTreeElement(this._node);
    startTagTreeElement ? startTagTreeElement.remove() : this.remove();
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  onenter() {
    // On Enter or Return start editing the first attribute
    // or create a new attribute on the selected element.
    if (this._editing)
      return false;

    this._startEditing();

    // prevent a newline from being immediately inserted
    return true;
  }

  /**
   * @override
   */
  selectOnMouseDown(event) {
    super.selectOnMouseDown(event);

    if (this._editing)
      return;

    // Prevent selecting the nearest word on double click.
    if (event.detail >= 2)
      event.preventDefault();
  }

  /**
   * @override
   * @return {boolean}
   */
  ondblclick(event) {
    if (this._editing || this._elementCloseTag)
      return false;

    if (this._startEditingTarget(/** @type {!Element} */ (event.target)))
      return false;

    if (this.isExpandable() && !this.expanded)
      this.expand();
    return false;
  }

  /**
   * @return {boolean}
   */
  hasEditableNode() {
    return !this._node.isShadowRoot() && !this._node.ancestorUserAgentShadowRoot();
  }

  _insertInLastAttributePosition(tag, node) {
    if (tag.getElementsByClassName('webkit-html-attribute').length > 0) {
      tag.insertBefore(node, tag.lastChild);
    } else {
      const nodeName = tag.textContent.match(/^<(.*?)>$/)[1];
      tag.textContent = '';
      tag.createTextChild('<' + nodeName);
      tag.appendChild(node);
      tag.createTextChild('>');
    }
  }

  /**
   * @param {!Element} eventTarget
   * @return {boolean}
   */
  _startEditingTarget(eventTarget) {
    if (this.treeOutline.selectedDOMNode() !== this._node)
      return false;

    if (this._node.nodeType() !== Node.ELEMENT_NODE && this._node.nodeType() !== Node.TEXT_NODE)
      return false;

    const textNode = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-text-node');
    if (textNode)
      return this._startEditingTextNode(textNode);

    const attribute = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-attribute');
    if (attribute)
      return this._startEditingAttribute(attribute, eventTarget);

    const tagName = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-tag-name');
    if (tagName)
      return this._startEditingTagName(tagName);

    const newAttribute = eventTarget.enclosingNodeOrSelfWithClass('add-attribute');
    if (newAttribute)
      return this._addNewAttribute();

    return false;
  }

  /**
   * @param {!Event} event
   */
  _showContextMenu(event) {
    this.treeOutline.showContextMenu(this, event);
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   * @param {!Event} event
   */
  populateTagContextMenu(contextMenu, event) {
    // Add attribute-related actions.
    const treeElement = this._elementCloseTag ? this.treeOutline.findTreeElement(this._node) : this;
    contextMenu.editSection().appendItem(
        Common.UIString('Add attribute'), treeElement._addNewAttribute.bind(treeElement));

    const attribute = event.target.enclosingNodeOrSelfWithClass('webkit-html-attribute');
    const newAttribute = event.target.enclosingNodeOrSelfWithClass('add-attribute');
    if (attribute && !newAttribute) {
      contextMenu.editSection().appendItem(
          Common.UIString('Edit attribute'), this._startEditingAttribute.bind(this, attribute, event.target));
    }
    this.populateNodeContextMenu(contextMenu);
    Elements.ElementsTreeElement.populateForcedPseudoStateItems(contextMenu, treeElement.node());
    this.populateScrollIntoView(contextMenu);
    contextMenu.viewSection().appendItem(Common.UIString('Focus'), async () => {
      await this._node.focus();
    });
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   */
  populateScrollIntoView(contextMenu) {
    contextMenu.viewSection().appendItem(Common.UIString('Scroll into view'), () => this._node.scrollIntoView());
  }

  populateTextContextMenu(contextMenu, textNode) {
    if (!this._editing) {
      contextMenu.editSection().appendItem(
          Common.UIString('Edit text'), this._startEditingTextNode.bind(this, textNode));
    }
    this.populateNodeContextMenu(contextMenu);
  }

  populateNodeContextMenu(contextMenu) {
    // Add free-form node-related actions.
    const isEditable = this.hasEditableNode();
    if (isEditable && !this._editing)
      contextMenu.editSection().appendItem(Common.UIString('Edit as HTML'), this._editAsHTML.bind(this));
    const isShadowRoot = this._node.isShadowRoot();

    // Place it here so that all "Copy"-ing items stick together.
    const copyMenu = contextMenu.clipboardSection().appendSubMenuItem(Common.UIString('Copy'));
    const createShortcut = UI.KeyboardShortcut.shortcutToString;
    const modifier = UI.KeyboardShortcut.Modifiers.CtrlOrMeta;
    const treeOutline = this.treeOutline;
    let menuItem;
    let section;
    if (!isShadowRoot) {
      section = copyMenu.section();
      menuItem = section.appendItem(
          Common.UIString('Copy outerHTML'), treeOutline.performCopyOrCut.bind(treeOutline, false, this._node));
      menuItem.setShortcut(createShortcut('V', modifier));
    }
    if (this._node.nodeType() === Node.ELEMENT_NODE)
      section.appendItem(Common.UIString('Copy selector'), this._copyCSSPath.bind(this));
    if (!isShadowRoot)
      section.appendItem(Common.UIString('Copy XPath'), this._copyXPath.bind(this));
    if (!isShadowRoot) {
      menuItem = copyMenu.clipboardSection().appendItem(
          Common.UIString('Cut element'), treeOutline.performCopyOrCut.bind(treeOutline, true, this._node),
          !this.hasEditableNode());
      menuItem.setShortcut(createShortcut('X', modifier));
      menuItem = copyMenu.clipboardSection().appendItem(
          Common.UIString('Copy element'), treeOutline.performCopyOrCut.bind(treeOutline, false, this._node));
      menuItem.setShortcut(createShortcut('C', modifier));
      menuItem = copyMenu.clipboardSection().appendItem(
          Common.UIString('Paste element'), treeOutline.pasteNode.bind(treeOutline, this._node),
          !treeOutline.canPaste(this._node));
      menuItem.setShortcut(createShortcut('V', modifier));
    }

    menuItem = contextMenu.debugSection().appendCheckboxItem(
        Common.UIString('Hide element'), treeOutline.toggleHideElement.bind(treeOutline, this._node),
        treeOutline.isToggledToHidden(this._node));
    menuItem.setShortcut(UI.shortcutRegistry.shortcutTitleForAction('elements.hide-element'));

    if (isEditable)
      contextMenu.editSection().appendItem(Common.UIString('Delete element'), this.remove.bind(this));

    contextMenu.viewSection().appendItem(ls`Expand recursively`, this.expandRecursively.bind(this));
    contextMenu.viewSection().appendItem(ls`Collapse children`, this.collapseChildren.bind(this));
  }

  _startEditing() {
    if (this.treeOutline.selectedDOMNode() !== this._node)
      return;

    const listItem = this.listItemElement;

    if (this._canAddAttributes) {
      const attribute = listItem.getElementsByClassName('webkit-html-attribute')[0];
      if (attribute) {
        return this._startEditingAttribute(
            attribute, attribute.getElementsByClassName('webkit-html-attribute-value')[0]);
      }

      return this._addNewAttribute();
    }

    if (this._node.nodeType() === Node.TEXT_NODE) {
      const textNode = listItem.getElementsByClassName('webkit-html-text-node')[0];
      if (textNode)
        return this._startEditingTextNode(textNode);
      return;
    }
  }

  _addNewAttribute() {
    // Cannot just convert the textual html into an element without
    // a parent node. Use a temporary span container for the HTML.
    const container = createElement('span');
    this._buildAttributeDOM(container, ' ', '', null);
    const attr = container.firstElementChild;
    attr.style.marginLeft = '2px';   // overrides the .editing margin rule
    attr.style.marginRight = '2px';  // overrides the .editing margin rule

    const tag = this.listItemElement.getElementsByClassName('webkit-html-tag')[0];
    this._insertInLastAttributePosition(tag, attr);
    attr.scrollIntoViewIfNeeded(true);
    return this._startEditingAttribute(attr, attr);
  }

  _triggerEditAttribute(attributeName) {
    const attributeElements = this.listItemElement.getElementsByClassName('webkit-html-attribute-name');
    for (let i = 0, len = attributeElements.length; i < len; ++i) {
      if (attributeElements[i].textContent === attributeName) {
        for (let elem = attributeElements[i].nextSibling; elem; elem = elem.nextSibling) {
          if (elem.nodeType !== Node.ELEMENT_NODE)
            continue;

          if (elem.classList.contains('webkit-html-attribute-value'))
            return this._startEditingAttribute(elem.parentNode, elem);
        }
      }
    }
  }

  _startEditingAttribute(attribute, elementForSelection) {
    console.assert(this.listItemElement.isAncestor(attribute));

    if (UI.isBeingEdited(attribute))
      return true;

    const attributeNameElement = attribute.getElementsByClassName('webkit-html-attribute-name')[0];
    if (!attributeNameElement)
      return false;

    const attributeName = attributeNameElement.textContent;
    const attributeValueElement = attribute.getElementsByClassName('webkit-html-attribute-value')[0];

    // Make sure elementForSelection is not a child of attributeValueElement.
    elementForSelection =
        attributeValueElement.isAncestor(elementForSelection) ? attributeValueElement : elementForSelection;

    function removeZeroWidthSpaceRecursive(node) {
      if (node.nodeType === Node.TEXT_NODE) {
        node.nodeValue = node.nodeValue.replace(/\u200B/g, '');
        return;
      }

      if (node.nodeType !== Node.ELEMENT_NODE)
        return;

      for (let child = node.firstChild; child; child = child.nextSibling)
        removeZeroWidthSpaceRecursive(child);
    }

    const attributeValue = attributeName && attributeValueElement ? this._node.getAttribute(attributeName) : undefined;
    if (attributeValue !== undefined) {
      attributeValueElement.setTextContentTruncatedIfNeeded(
          attributeValue, Common.UIString('<value is too large to edit>'));
    }

    // Remove zero-width spaces that were added by nodeTitleInfo.
    removeZeroWidthSpaceRecursive(attribute);

    const config = new UI.InplaceEditor.Config(
        this._attributeEditingCommitted.bind(this), this._editingCancelled.bind(this), attributeName);

    /**
     * @param {!Event} event
     * @return {string}
     */
    function postKeyDownFinishHandler(event) {
      UI.handleElementValueModifications(event, attribute);
      return '';
    }

    if (!attributeValueElement.textContent.asParsedURL())
      config.setPostKeydownFinishHandler(postKeyDownFinishHandler);

    this._editing = UI.InplaceEditor.startEditing(attribute, config);

    this.listItemElement.getComponentSelection().selectAllChildren(elementForSelection);

    return true;
  }

  /**
   * @param {!Element} textNodeElement
   */
  _startEditingTextNode(textNodeElement) {
    if (UI.isBeingEdited(textNodeElement))
      return true;

    let textNode = this._node;
    // We only show text nodes inline in elements if the element only
    // has a single child, and that child is a text node.
    if (textNode.nodeType() === Node.ELEMENT_NODE && textNode.firstChild)
      textNode = textNode.firstChild;

    const container = textNodeElement.enclosingNodeOrSelfWithClass('webkit-html-text-node');
    if (container)
      container.textContent = textNode.nodeValue();  // Strip the CSS or JS highlighting if present.
    const config = new UI.InplaceEditor.Config(
        this._textNodeEditingCommitted.bind(this, textNode), this._editingCancelled.bind(this));
    this._editing = UI.InplaceEditor.startEditing(textNodeElement, config);
    this.listItemElement.getComponentSelection().selectAllChildren(textNodeElement);

    return true;
  }

  /**
   * @param {!Element=} tagNameElement
   */
  _startEditingTagName(tagNameElement) {
    if (!tagNameElement) {
      tagNameElement = this.listItemElement.getElementsByClassName('webkit-html-tag-name')[0];
      if (!tagNameElement)
        return false;
    }

    const tagName = tagNameElement.textContent;
    if (Elements.ElementsTreeElement.EditTagBlacklist.has(tagName.toLowerCase()))
      return false;

    if (UI.isBeingEdited(tagNameElement))
      return true;

    const closingTagElement = this._distinctClosingTagElement();

    /**
     * @param {!Event} event
     */
    function keyupListener(event) {
      if (closingTagElement)
        closingTagElement.textContent = '</' + tagNameElement.textContent + '>';
    }

    /**
     * @param {!Event} event
     */
    const keydownListener = event => {
      if (event.key !== ' ')
        return;
      this._editing.commit();
      event.consume(true);
    };

    /**
     * @param {!Element} element
     * @param {string} newTagName
     * @this {Elements.ElementsTreeElement}
     */
    function editingComitted(element, newTagName) {
      tagNameElement.removeEventListener('keyup', keyupListener, false);
      tagNameElement.removeEventListener('keydown', keydownListener, false);
      this._tagNameEditingCommitted.apply(this, arguments);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function editingCancelled() {
      tagNameElement.removeEventListener('keyup', keyupListener, false);
      tagNameElement.removeEventListener('keydown', keydownListener, false);
      this._editingCancelled.apply(this, arguments);
    }

    tagNameElement.addEventListener('keyup', keyupListener, false);
    tagNameElement.addEventListener('keydown', keydownListener, false);

    const config = new UI.InplaceEditor.Config(editingComitted.bind(this), editingCancelled.bind(this), tagName);
    this._editing = UI.InplaceEditor.startEditing(tagNameElement, config);
    this.listItemElement.getComponentSelection().selectAllChildren(tagNameElement);
    return true;
  }

  /**
   * @param {function(string, string)} commitCallback
   * @param {function()} disposeCallback
   * @param {?string} maybeInitialValue
   */
  _startEditingAsHTML(commitCallback, disposeCallback, maybeInitialValue) {
    if (maybeInitialValue === null)
      return;
    let initialValue = maybeInitialValue;  // To suppress a compiler warning.
    if (this._editing)
      return;

    initialValue = this._convertWhitespaceToEntities(initialValue).text;

    this._htmlEditElement = createElement('div');
    this._htmlEditElement.className = 'source-code elements-tree-editor';

    // Hide header items.
    let child = this.listItemElement.firstChild;
    while (child) {
      child.style.display = 'none';
      child = child.nextSibling;
    }
    // Hide children item.
    if (this.childrenListElement)
      this.childrenListElement.style.display = 'none';
    // Append editor.
    this.listItemElement.appendChild(this._htmlEditElement);

    self.runtime.extension(UI.TextEditorFactory).instance().then(gotFactory.bind(this));

    /**
     * @param {!UI.TextEditorFactory} factory
     * @this {Elements.ElementsTreeElement}
     */
    function gotFactory(factory) {
      const editor = factory.createEditor({
        lineNumbers: false,
        lineWrapping: Common.moduleSetting('domWordWrap').get(),
        mimeType: 'text/html',
        autoHeight: false,
        padBottom: false
      });
      this._editing =
          {commit: commit.bind(this), cancel: dispose.bind(this), editor: editor, resize: resize.bind(this)};
      resize.call(this);

      editor.widget().show(this._htmlEditElement);
      editor.setText(initialValue);
      editor.widget().focus();
      editor.widget().element.addEventListener('focusout', event => {
        // The relatedTarget is null when no element gains focus, e.g. switching windows.
        if (event.relatedTarget && !event.relatedTarget.isSelfOrDescendant(editor.widget().element))
          this._editing.commit();
      }, false);
      editor.widget().element.addEventListener('keydown', keydown.bind(this), true);

      this.treeOutline.setMultilineEditing(this._editing);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function resize() {
      this._htmlEditElement.style.width = this.treeOutline.visibleWidth() - this._computeLeftIndent() - 30 + 'px';
      this._editing.editor.onResize();
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function commit() {
      commitCallback(initialValue, this._editing.editor.text());
      dispose.call(this);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function dispose() {
      this._editing.editor.widget().element.removeEventListener('blur', this._editing.commit, true);
      this._editing.editor.widget().detach();
      delete this._editing;

      // Remove editor.
      this.listItemElement.removeChild(this._htmlEditElement);
      delete this._htmlEditElement;
      // Unhide children item.
      if (this.childrenListElement)
        this.childrenListElement.style.removeProperty('display');
      // Unhide header items.
      let child = this.listItemElement.firstChild;
      while (child) {
        child.style.removeProperty('display');
        child = child.nextSibling;
      }

      if (this.treeOutline) {
        this.treeOutline.setMultilineEditing(null);
        this.treeOutline.focus();
      }

      disposeCallback();
    }

    /**
     * @param {!Event} event
     * @this {!Elements.ElementsTreeElement}
     */
    function keydown(event) {
      const isMetaOrCtrl = UI.KeyboardShortcut.eventHasCtrlOrMeta(/** @type {!KeyboardEvent} */ (event)) &&
          !event.altKey && !event.shiftKey;
      if (isEnterKey(event) && (isMetaOrCtrl || event.isMetaOrCtrlForTest)) {
        event.consume(true);
        this._editing.commit();
      } else if (event.keyCode === UI.KeyboardShortcut.Keys.Esc.code || event.key === 'Escape') {
        event.consume(true);
        this._editing.cancel();
      }
    }
  }

  _attributeEditingCommitted(element, newText, oldText, attributeName, moveDirection) {
    delete this._editing;

    const treeOutline = this.treeOutline;

    /**
     * @param {?Protocol.Error=} error
     * @this {Elements.ElementsTreeElement}
     */
    function moveToNextAttributeIfNeeded(error) {
      if (error)
        this._editingCancelled(element, attributeName);

      if (!moveDirection)
        return;

      treeOutline.runPendingUpdates();
      treeOutline.focus();

      // Search for the attribute's position, and then decide where to move to.
      const attributes = this._node.attributes();
      for (let i = 0; i < attributes.length; ++i) {
        if (attributes[i].name !== attributeName)
          continue;

        if (moveDirection === 'backward') {
          if (i === 0)
            this._startEditingTagName();
          else
            this._triggerEditAttribute(attributes[i - 1].name);
        } else {
          if (i === attributes.length - 1)
            this._addNewAttribute();
          else
            this._triggerEditAttribute(attributes[i + 1].name);
        }
        return;
      }

      // Moving From the "New Attribute" position.
      if (moveDirection === 'backward') {
        if (newText === ' ') {
          // Moving from "New Attribute" that was not edited
          if (attributes.length > 0)
            this._triggerEditAttribute(attributes[attributes.length - 1].name);
        } else {
          // Moving from "New Attribute" that holds new value
          if (attributes.length > 1)
            this._triggerEditAttribute(attributes[attributes.length - 2].name);
        }
      } else if (moveDirection === 'forward') {
        if (!newText.isWhitespace())
          this._addNewAttribute();
        else
          this._startEditingTagName();
      }
    }

    if ((attributeName.trim() || newText.trim()) && oldText !== newText) {
      this._node.setAttribute(attributeName, newText, moveToNextAttributeIfNeeded.bind(this));
      return;
    }

    this.updateTitle();
    moveToNextAttributeIfNeeded.call(this);
  }

  _tagNameEditingCommitted(element, newText, oldText, tagName, moveDirection) {
    delete this._editing;
    const self = this;

    function cancel() {
      const closingTagElement = self._distinctClosingTagElement();
      if (closingTagElement)
        closingTagElement.textContent = '</' + tagName + '>';

      self._editingCancelled(element, tagName);
      moveToNextAttributeIfNeeded.call(self);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function moveToNextAttributeIfNeeded() {
      if (moveDirection !== 'forward') {
        this._addNewAttribute();
        return;
      }

      const attributes = this._node.attributes();
      if (attributes.length > 0)
        this._triggerEditAttribute(attributes[0].name);
      else
        this._addNewAttribute();
    }

    newText = newText.trim();
    if (newText === oldText) {
      cancel();
      return;
    }

    const treeOutline = this.treeOutline;
    const wasExpanded = this.expanded;

    this._node.setNodeName(newText, (error, newNode) => {
      if (error || !newNode) {
        cancel();
        return;
      }
      const newTreeItem = treeOutline.selectNodeAfterEdit(wasExpanded, error, newNode);
      moveToNextAttributeIfNeeded.call(newTreeItem);
    });
  }

  /**
   * @param {!SDK.DOMNode} textNode
   * @param {!Element} element
   * @param {string} newText
   */
  _textNodeEditingCommitted(textNode, element, newText) {
    delete this._editing;

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function callback() {
      this.updateTitle();
    }
    textNode.setNodeValue(newText, callback.bind(this));
  }

  /**
   * @param {!Element} element
   * @param {*} context
   */
  _editingCancelled(element, context) {
    delete this._editing;

    // Need to restore attributes structure.
    this.updateTitle();
  }

  /**
   * @return {!Element}
   */
  _distinctClosingTagElement() {
    // FIXME: Improve the Tree Element / Outline Abstraction to prevent crawling the DOM

    // For an expanded element, it will be the last element with class "close"
    // in the child element list.
    if (this.expanded) {
      const closers = this.childrenListElement.querySelectorAll('.close');
      return closers[closers.length - 1];
    }

    // Remaining cases are single line non-expanded elements with a closing
    // tag, or HTML elements without a closing tag (such as <br>). Return
    // null in the case where there isn't a closing tag.
    const tags = this.listItemElement.getElementsByClassName('webkit-html-tag');
    return (tags.length === 1 ? null : tags[tags.length - 1]);
  }

  /**
   * @param {?Elements.ElementsTreeOutline.UpdateRecord=} updateRecord
   * @param {boolean=} onlySearchQueryChanged
   */
  updateTitle(updateRecord, onlySearchQueryChanged) {
    // If we are editing, return early to prevent canceling the edit.
    // After editing is committed updateTitle will be called.
    if (this._editing)
      return;

    if (onlySearchQueryChanged) {
      this._hideSearchHighlight();
    } else {
      const nodeInfo = this._nodeTitleInfo(updateRecord || null);
      if (this._node.nodeType() === Node.DOCUMENT_FRAGMENT_NODE && this._node.isInShadowTree() &&
          this._node.shadowRootType()) {
        this.childrenListElement.classList.add('shadow-root');
        let depth = 4;
        for (let node = this._node; depth && node; node = node.parentNode) {
          if (node.nodeType() === Node.DOCUMENT_FRAGMENT_NODE)
            depth--;
        }
        if (!depth)
          this.childrenListElement.classList.add('shadow-root-deep');
        else
          this.childrenListElement.classList.add('shadow-root-depth-' + depth);
      }
      const highlightElement = createElement('span');
      highlightElement.className = 'highlight';
      highlightElement.appendChild(nodeInfo);
      this.title = highlightElement;
      this.updateDecorations();
      this.listItemElement.insertBefore(this._gutterContainer, this.listItemElement.firstChild);
      delete this._highlightResult;
      delete this.selectionElement;
      delete this._hintElement;
      if (this.selected) {
        this._createSelection();
        this._createHint();
      }
    }

    this._highlightSearchResults();
  }

  /**
   * @return {number}
   */
  _computeLeftIndent() {
    let treeElement = this.parent;
    let depth = 0;
    while (treeElement !== null) {
      depth++;
      treeElement = treeElement.parent;
    }

    /** Keep it in sync with elementsTreeOutline.css **/
    return 12 * (depth - 2) + (this.isExpandable() ? 1 : 12);
  }

  updateDecorations() {
    this._gutterContainer.style.left = (-this._computeLeftIndent()) + 'px';

    if (this.isClosingTag())
      return;

    if (this._node.nodeType() !== Node.ELEMENT_NODE)
      return;

    this._decorationsThrottler.schedule(this._updateDecorationsInternal.bind(this));
  }

  /**
   * @return {!Promise}
   */
  _updateDecorationsInternal() {
    if (!this.treeOutline)
      return Promise.resolve();

    const node = this._node;

    if (!this.treeOutline._decoratorExtensions)
      /** @type {!Array.<!Runtime.Extension>} */
      this.treeOutline._decoratorExtensions = self.runtime.extensions(Elements.MarkerDecorator);

    const markerToExtension = new Map();
    for (let i = 0; i < this.treeOutline._decoratorExtensions.length; ++i) {
      markerToExtension.set(
          this.treeOutline._decoratorExtensions[i].descriptor()['marker'], this.treeOutline._decoratorExtensions[i]);
    }

    const promises = [];
    const decorations = [];
    const descendantDecorations = [];
    node.traverseMarkers(visitor);

    /**
     * @param {!SDK.DOMNode} n
     * @param {string} marker
     */
    function visitor(n, marker) {
      const extension = markerToExtension.get(marker);
      if (!extension)
        return;
      promises.push(extension.instance().then(collectDecoration.bind(null, n)));
    }

    /**
     * @param {!SDK.DOMNode} n
     * @param {!Elements.MarkerDecorator} decorator
     */
    function collectDecoration(n, decorator) {
      const decoration = decorator.decorate(n);
      if (!decoration)
        return;
      (n === node ? decorations : descendantDecorations).push(decoration);
    }

    return Promise.all(promises).then(updateDecorationsUI.bind(this));

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function updateDecorationsUI() {
      this._decorationsElement.removeChildren();
      this._decorationsElement.classList.add('hidden');
      this._gutterContainer.classList.toggle('has-decorations', decorations.length || descendantDecorations.length);

      if (!decorations.length && !descendantDecorations.length)
        return;

      const colors = new Set();
      const titles = createElement('div');

      for (const decoration of decorations) {
        const titleElement = titles.createChild('div');
        titleElement.textContent = decoration.title;
        colors.add(decoration.color);
      }
      if (this.expanded && !decorations.length)
        return;

      const descendantColors = new Set();
      if (descendantDecorations.length) {
        let element = titles.createChild('div');
        element.textContent = Common.UIString('Children:');
        for (const decoration of descendantDecorations) {
          element = titles.createChild('div');
          element.style.marginLeft = '15px';
          element.textContent = decoration.title;
          descendantColors.add(decoration.color);
        }
      }

      let offset = 0;
      processColors.call(this, colors, 'elements-gutter-decoration');
      if (!this.expanded)
        processColors.call(this, descendantColors, 'elements-gutter-decoration elements-has-decorated-children');
      UI.Tooltip.install(this._decorationsElement, titles);

      /**
       * @param {!Set<string>} colors
       * @param {string} className
       * @this {Elements.ElementsTreeElement}
       */
      function processColors(colors, className) {
        for (const color of colors) {
          const child = this._decorationsElement.createChild('div', className);
          this._decorationsElement.classList.remove('hidden');
          child.style.backgroundColor = color;
          child.style.borderColor = color;
          if (offset)
            child.style.marginLeft = offset + 'px';
          offset += 3;
        }
      }
    }
  }

  /**
   * @param {!Node} parentElement
   * @param {string} name
   * @param {string} value
   * @param {?Elements.ElementsTreeOutline.UpdateRecord} updateRecord
   * @param {boolean=} forceValue
   * @param {!SDK.DOMNode=} node
   */
  _buildAttributeDOM(parentElement, name, value, updateRecord, forceValue, node) {
    const closingPunctuationRegex = /[\/;:\)\]\}]/g;
    let highlightIndex = 0;
    let highlightCount;
    let additionalHighlightOffset = 0;
    let result;

    /**
     * @param {string} match
     * @param {number} replaceOffset
     * @return {string}
     */
    function replacer(match, replaceOffset) {
      while (highlightIndex < highlightCount && result.entityRanges[highlightIndex].offset < replaceOffset) {
        result.entityRanges[highlightIndex].offset += additionalHighlightOffset;
        ++highlightIndex;
      }
      additionalHighlightOffset += 1;
      return match + '\u200B';
    }

    /**
     * @param {!Element} element
     * @param {string} value
     * @this {Elements.ElementsTreeElement}
     */
    function setValueWithEntities(element, value) {
      result = this._convertWhitespaceToEntities(value);
      highlightCount = result.entityRanges.length;
      value = result.text.replace(closingPunctuationRegex, replacer);
      while (highlightIndex < highlightCount) {
        result.entityRanges[highlightIndex].offset += additionalHighlightOffset;
        ++highlightIndex;
      }
      element.setTextContentTruncatedIfNeeded(value);
      UI.highlightRangesWithStyleClass(element, result.entityRanges, 'webkit-html-entity-value');
    }

    const hasText = (forceValue || value.length > 0);
    const attrSpanElement = parentElement.createChild('span', 'webkit-html-attribute');
    const attrNameElement = attrSpanElement.createChild('span', 'webkit-html-attribute-name');
    attrNameElement.textContent = name;

    if (hasText)
      attrSpanElement.createTextChild('=\u200B"');

    const attrValueElement = attrSpanElement.createChild('span', 'webkit-html-attribute-value');

    if (updateRecord && updateRecord.isAttributeModified(name))
      UI.runCSSAnimationOnce(hasText ? attrValueElement : attrNameElement, 'dom-update-highlight');

    /**
     * @this {Elements.ElementsTreeElement}
     * @param {string} value
     * @return {!Element}
     */
    function linkifyValue(value) {
      const rewrittenHref = node.resolveURL(value);
      if (rewrittenHref === null) {
        const span = createElement('span');
        setValueWithEntities.call(this, span, value);
        return span;
      }
      value = value.replace(closingPunctuationRegex, '$&\u200B');
      if (value.startsWith('data:'))
        value = value.trimMiddle(60);
      const link = node.nodeName().toLowerCase() === 'a' ?
          UI.XLink.create(rewrittenHref, value, '', true /* preventClick */) :
          Components.Linkifier.linkifyURL(rewrittenHref, {text: value, preventClick: true});
      link[Elements.ElementsTreeElement.HrefSymbol] = rewrittenHref;
      return link;
    }

    const nodeName = node ? node.nodeName().toLowerCase() : '';
    if (nodeName && (name === 'src' || name === 'href'))
      attrValueElement.appendChild(linkifyValue.call(this, value));
    else if ((nodeName === 'img' || nodeName === 'source') && name === 'srcset')
      attrValueElement.appendChild(linkifySrcset.call(this, value));
    else
      setValueWithEntities.call(this, attrValueElement, value);

    if (hasText)
      attrSpanElement.createTextChild('"');

    /**
     * @param {string} value
     * @return {!DocumentFragment}
     * @this {!Elements.ElementsTreeElement}
     */
    function linkifySrcset(value) {
      // Splitting normally on commas or spaces will break on valid srcsets "foo 1x,bar 2x" and "data:,foo 1x".
      // 1) Let the index of the next space be `indexOfSpace`.
      // 2a) If the character at `indexOfSpace - 1` is a comma, collect the preceding characters up to
      //     `indexOfSpace - 1` as a URL and repeat step 1).
      // 2b) Else, collect the preceding characters as a URL.
      // 3) Collect the characters from `indexOfSpace` up to the next comma as the size descriptor and repeat step 1).
      // https://html.spec.whatwg.org/multipage/embedded-content.html#parse-a-srcset-attribute
      const fragment = createDocumentFragment();
      let i = 0;
      while (value.length) {
        if (i++ > 0)
          fragment.createTextChild(' ');
        value = value.trim();
        // The url and descriptor may end with a separating comma.
        let url = '';
        let descriptor = '';
        const indexOfSpace = value.search(/\s/);
        if (indexOfSpace === -1) {
          url = value;
        } else if (indexOfSpace > 0 && value[indexOfSpace - 1] === ',') {
          url = value.substring(0, indexOfSpace);
        } else {
          url = value.substring(0, indexOfSpace);
          const indexOfComma = value.indexOf(',', indexOfSpace);
          if (indexOfComma !== -1)
            descriptor = value.substring(indexOfSpace, indexOfComma + 1);
          else
            descriptor = value.substring(indexOfSpace);
        }

        if (url) {
          // Up to one trailing comma should be removed from `url`.
          if (url.endsWith(',')) {
            fragment.appendChild(linkifyValue.call(this, url.substring(0, url.length - 1)));
            fragment.createTextChild(',');
          } else {
            fragment.appendChild(linkifyValue.call(this, url));
          }
        }
        if (descriptor)
          fragment.createTextChild(descriptor);
        value = value.substring(url.length + descriptor.length);
      }
      return fragment;
    }
  }

  /**
   * @param {!Node} parentElement
   * @param {string} pseudoElementName
   */
  _buildPseudoElementDOM(parentElement, pseudoElementName) {
    const pseudoElement = parentElement.createChild('span', 'webkit-html-pseudo-element');
    pseudoElement.textContent = '::' + pseudoElementName;
    parentElement.createTextChild('\u200B');
  }

  /**
   * @param {!Node} parentElement
   * @param {string} tagName
   * @param {boolean} isClosingTag
   * @param {boolean} isDistinctTreeElement
   * @param {?Elements.ElementsTreeOutline.UpdateRecord} updateRecord
   */
  _buildTagDOM(parentElement, tagName, isClosingTag, isDistinctTreeElement, updateRecord) {
    const node = this._node;
    const classes = ['webkit-html-tag'];
    if (isClosingTag && isDistinctTreeElement)
      classes.push('close');
    const tagElement = parentElement.createChild('span', classes.join(' '));
    tagElement.createTextChild('<');
    const tagNameElement =
        tagElement.createChild('span', isClosingTag ? 'webkit-html-close-tag-name' : 'webkit-html-tag-name');
    tagNameElement.textContent = (isClosingTag ? '/' : '') + tagName;
    if (!isClosingTag) {
      if (node.hasAttributes()) {
        const attributes = node.attributes();
        for (let i = 0; i < attributes.length; ++i) {
          const attr = attributes[i];
          tagElement.createTextChild(' ');
          this._buildAttributeDOM(tagElement, attr.name, attr.value, updateRecord, false, node);
        }
      }
      if (updateRecord) {
        let hasUpdates = updateRecord.hasRemovedAttributes() || updateRecord.hasRemovedChildren();
        hasUpdates |= !this.expanded && updateRecord.hasChangedChildren();
        if (hasUpdates)
          UI.runCSSAnimationOnce(tagNameElement, 'dom-update-highlight');
      }
    }

    tagElement.createTextChild('>');
    parentElement.createTextChild('\u200B');
  }

  /**
   * @param {string} text
   * @return {!{text: string, entityRanges: !Array.<!TextUtils.SourceRange>}}
   */
  _convertWhitespaceToEntities(text) {
    let result = '';
    let lastIndexAfterEntity = 0;
    const entityRanges = [];
    const charToEntity = Elements.ElementsTreeOutline.MappedCharToEntity;
    for (let i = 0, size = text.length; i < size; ++i) {
      const char = text.charAt(i);
      if (charToEntity[char]) {
        result += text.substring(lastIndexAfterEntity, i);
        const entityValue = '&' + charToEntity[char] + ';';
        entityRanges.push({offset: result.length, length: entityValue.length});
        result += entityValue;
        lastIndexAfterEntity = i + 1;
      }
    }
    if (result)
      result += text.substring(lastIndexAfterEntity);
    return {text: result || text, entityRanges: entityRanges};
  }

  /**
   * @param {?Elements.ElementsTreeOutline.UpdateRecord} updateRecord
   * @return {!DocumentFragment} result
   */
  _nodeTitleInfo(updateRecord) {
    const node = this._node;
    const titleDOM = createDocumentFragment();

    switch (node.nodeType()) {
      case Node.ATTRIBUTE_NODE:
        this._buildAttributeDOM(
            titleDOM, /** @type {string} */ (node.name), /** @type {string} */ (node.value), updateRecord, true);
        break;

      case Node.ELEMENT_NODE:
        const pseudoType = node.pseudoType();
        if (pseudoType) {
          this._buildPseudoElementDOM(titleDOM, pseudoType);
          break;
        }

        const tagName = node.nodeNameInCorrectCase();
        if (this._elementCloseTag) {
          this._buildTagDOM(titleDOM, tagName, true, true, updateRecord);
          break;
        }

        this._buildTagDOM(titleDOM, tagName, false, false, updateRecord);

        if (this.isExpandable()) {
          if (!this.expanded) {
            const textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node bogus');
            textNodeElement.textContent = '\u2026';
            titleDOM.createTextChild('\u200B');
            this._buildTagDOM(titleDOM, tagName, true, false, updateRecord);
          }
          break;
        }

        if (Elements.ElementsTreeElement.canShowInlineText(node)) {
          const textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node');
          const result = this._convertWhitespaceToEntities(node.firstChild.nodeValue());
          textNodeElement.textContent = result.text;
          UI.highlightRangesWithStyleClass(textNodeElement, result.entityRanges, 'webkit-html-entity-value');
          titleDOM.createTextChild('\u200B');
          this._buildTagDOM(titleDOM, tagName, true, false, updateRecord);
          if (updateRecord && updateRecord.hasChangedChildren())
            UI.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          if (updateRecord && updateRecord.isCharDataModified())
            UI.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          break;
        }

        if (this.treeOutline.isXMLMimeType || !Elements.ElementsTreeElement.ForbiddenClosingTagElements.has(tagName))
          this._buildTagDOM(titleDOM, tagName, true, false, updateRecord);
        break;

      case Node.TEXT_NODE:
        if (node.parentNode && node.parentNode.nodeName().toLowerCase() === 'script') {
          const newNode = titleDOM.createChild('span', 'webkit-html-text-node webkit-html-js-node');
          const text = node.nodeValue();
          newNode.textContent = text.startsWith('\n') ? text.substring(1) : text;

          const javascriptSyntaxHighlighter = new UI.SyntaxHighlighter('text/javascript', true);
          javascriptSyntaxHighlighter.syntaxHighlightNode(newNode).then(updateSearchHighlight.bind(this));
        } else if (node.parentNode && node.parentNode.nodeName().toLowerCase() === 'style') {
          const newNode = titleDOM.createChild('span', 'webkit-html-text-node webkit-html-css-node');
          const text = node.nodeValue();
          newNode.textContent = text.startsWith('\n') ? text.substring(1) : text;

          const cssSyntaxHighlighter = new UI.SyntaxHighlighter('text/css', true);
          cssSyntaxHighlighter.syntaxHighlightNode(newNode).then(updateSearchHighlight.bind(this));
        } else {
          titleDOM.createTextChild('"');
          const textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node');
          const result = this._convertWhitespaceToEntities(node.nodeValue());
          textNodeElement.textContent = result.text;
          UI.highlightRangesWithStyleClass(textNodeElement, result.entityRanges, 'webkit-html-entity-value');
          titleDOM.createTextChild('"');
          if (updateRecord && updateRecord.isCharDataModified())
            UI.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
        }
        break;

      case Node.COMMENT_NODE:
        const commentElement = titleDOM.createChild('span', 'webkit-html-comment');
        commentElement.createTextChild('<!--' + node.nodeValue() + '-->');
        break;

      case Node.DOCUMENT_TYPE_NODE:
        const docTypeElement = titleDOM.createChild('span', 'webkit-html-doctype');
        docTypeElement.createTextChild('<!doctype ' + node.nodeName());
        if (node.publicId) {
          docTypeElement.createTextChild(' PUBLIC "' + node.publicId + '"');
          if (node.systemId)
            docTypeElement.createTextChild(' "' + node.systemId + '"');
        } else if (node.systemId) {
          docTypeElement.createTextChild(' SYSTEM "' + node.systemId + '"');
        }

        if (node.internalSubset)
          docTypeElement.createTextChild(' [' + node.internalSubset + ']');

        docTypeElement.createTextChild('>');
        break;

      case Node.CDATA_SECTION_NODE:
        const cdataElement = titleDOM.createChild('span', 'webkit-html-text-node');
        cdataElement.createTextChild('<![CDATA[' + node.nodeValue() + ']]>');
        break;

      case Node.DOCUMENT_FRAGMENT_NODE:
        const fragmentElement = titleDOM.createChild('span', 'webkit-html-fragment');
        fragmentElement.textContent = node.nodeNameInCorrectCase().collapseWhitespace();
        break;
      default:
        titleDOM.createTextChild(node.nodeNameInCorrectCase().collapseWhitespace());
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function updateSearchHighlight() {
      delete this._highlightResult;
      this._highlightSearchResults();
    }

    return titleDOM;
  }

  remove() {
    if (this._node.pseudoType())
      return;
    const parentElement = this.parent;
    if (!parentElement)
      return;

    if (!this._node.parentNode || this._node.parentNode.nodeType() === Node.DOCUMENT_NODE)
      return;
    this._node.removeNode();
  }

  /**
   * @param {function(boolean)=} callback
   * @param {boolean=} startEditing
   */
  toggleEditAsHTML(callback, startEditing) {
    if (this._editing && this._htmlEditElement) {
      this._editing.commit();
      return;
    }

    if (startEditing === false)
      return;

    /**
     * @param {?Protocol.Error} error
     */
    function selectNode(error) {
      if (callback)
        callback(!error);
    }

    /**
     * @param {string} initialValue
     * @param {string} value
     */
    function commitChange(initialValue, value) {
      if (initialValue !== value)
        node.setOuterHTML(value, selectNode);
    }

    function disposeCallback() {
      if (callback)
        callback(false);
    }

    const node = this._node;
    node.getOuterHTML().then(this._startEditingAsHTML.bind(this, commitChange, disposeCallback));
  }

  _copyCSSPath() {
    InspectorFrontendHost.copyText(Elements.DOMPath.cssPath(this._node, true));
  }

  _copyXPath() {
    InspectorFrontendHost.copyText(Elements.DOMPath.xPath(this._node, true));
  }

  _highlightSearchResults() {
    if (!this._searchQuery || !this._searchHighlightsVisible)
      return;
    this._hideSearchHighlight();

    const text = this.listItemElement.textContent;
    const regexObject = createPlainTextSearchRegex(this._searchQuery, 'gi');

    let match = regexObject.exec(text);
    const matchRanges = [];
    while (match) {
      matchRanges.push(new TextUtils.SourceRange(match.index, match[0].length));
      match = regexObject.exec(text);
    }

    // Fall back for XPath, etc. matches.
    if (!matchRanges.length)
      matchRanges.push(new TextUtils.SourceRange(0, text.length));

    this._highlightResult = [];
    UI.highlightSearchResults(this.listItemElement, matchRanges, this._highlightResult);
  }

  _editAsHTML() {
    const promise = Common.Revealer.reveal(this.node());
    promise.then(() => UI.actionRegistry.action('elements.edit-as-html').execute());
  }
};

Elements.ElementsTreeElement.HrefSymbol = Symbol('ElementsTreeElement.Href');

Elements.ElementsTreeElement.InitialChildrenLimit = 500;

// A union of HTML4 and HTML5-Draft elements that explicitly
// or implicitly (for HTML5) forbid the closing tag.
Elements.ElementsTreeElement.ForbiddenClosingTagElements = new Set([
  'area', 'base',  'basefont', 'br',   'canvas',   'col',  'command', 'embed',  'frame', 'hr',
  'img',  'input', 'keygen',   'link', 'menuitem', 'meta', 'param',   'source', 'track', 'wbr'
]);

// These tags we do not allow editing their tag name.
Elements.ElementsTreeElement.EditTagBlacklist = new Set(['html', 'head', 'body']);

/** @typedef {{cancel: function(), commit: function(), resize: function(), editor:!UI.TextEditor}} */
Elements.MultilineEditorController;
