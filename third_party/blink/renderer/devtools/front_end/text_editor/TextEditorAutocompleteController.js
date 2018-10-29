// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @implements {UI.SuggestBoxDelegate}
 * @unrestricted
 */
TextEditor.TextEditorAutocompleteController = class {
  /**
   * @param {!TextEditor.CodeMirrorTextEditor} textEditor
   * @param {!CodeMirror} codeMirror
   * @param {!UI.AutocompleteConfig} config
   */
  constructor(textEditor, codeMirror, config) {
    this._textEditor = textEditor;
    this._codeMirror = codeMirror;
    this._config = config;
    this._initialized = false;

    this._onScroll = this._onScroll.bind(this);
    this._onCursorActivity = this._onCursorActivity.bind(this);
    this._changes = this._changes.bind(this);
    this._blur = this._blur.bind(this);
    this._beforeChange = this._beforeChange.bind(this);
    this._mouseDown = () => {
      this.clearAutocomplete();
      this._tooltipGlassPane.hide();
    };
    this._codeMirror.on('changes', this._changes);
    this._lastHintText = '';
    /** @type {?UI.SuggestBox} */
    this._suggestBox = null;
    /** @type {?string} */
    this._currentSuggestion = null;
    this._hintElement = createElementWithClass('span', 'auto-complete-text');

    this._tooltipGlassPane = new UI.GlassPane();
    this._tooltipGlassPane.setSizeBehavior(UI.GlassPane.SizeBehavior.MeasureContent);
    this._tooltipGlassPane.setOutsideClickCallback(this._tooltipGlassPane.hide.bind(this._tooltipGlassPane));
    this._tooltipElement = createElementWithClass('div', 'autocomplete-tooltip');
    const shadowRoot =
        UI.createShadowRootWithCoreStyles(this._tooltipGlassPane.contentElement, 'text_editor/autocompleteTooltip.css');
    shadowRoot.appendChild(this._tooltipElement);
  }

  _initializeIfNeeded() {
    if (this._initialized)
      return;
    this._initialized = true;
    this._codeMirror.on('scroll', this._onScroll);
    this._codeMirror.on('cursorActivity', this._onCursorActivity);
    this._codeMirror.on('mousedown', this._mouseDown);
    this._codeMirror.on('blur', this._blur);
    if (this._config.isWordChar) {
      this._codeMirror.on('beforeChange', this._beforeChange);
      this._dictionary = new Common.TextDictionary();
      this._addWordsFromText(this._codeMirror.getValue());
    }
  }

  dispose() {
    this._codeMirror.off('changes', this._changes);
    if (this._initialized) {
      this._codeMirror.off('scroll', this._onScroll);
      this._codeMirror.off('cursorActivity', this._onCursorActivity);
      this._codeMirror.off('mousedown', this._mouseDown);
      this._codeMirror.off('blur', this._blur);
    }
    if (this._dictionary) {
      this._codeMirror.off('beforeChange', this._beforeChange);
      this._dictionary.reset();
    }
  }

  /**
   * @param {!CodeMirror} codeMirror
   * @param {!CodeMirror.BeforeChangeObject} changeObject
   */
  _beforeChange(codeMirror, changeObject) {
    this._updatedLines = this._updatedLines || {};
    for (let i = changeObject.from.line; i <= changeObject.to.line; ++i) {
      if (this._updatedLines[i] === undefined)
        this._updatedLines[i] = this._codeMirror.getLine(i);
    }
  }

  /**
   * @param {string} text
   */
  _addWordsFromText(text) {
    TextUtils.TextUtils.textToWords(
        text, /** @type {function(string):boolean} */ (this._config.isWordChar), addWord.bind(this));

    /**
     * @param {string} word
     * @this {TextEditor.TextEditorAutocompleteController}
     */
    function addWord(word) {
      if (word.length && (word[0] < '0' || word[0] > '9'))
        this._dictionary.addWord(word);
    }
  }

  /**
   * @param {string} text
   */
  _removeWordsFromText(text) {
    TextUtils.TextUtils.textToWords(
        text, /** @type {function(string):boolean} */ (this._config.isWordChar),
        word => this._dictionary.removeWord(word));
  }

  /**
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?TextUtils.TextRange}
   */
  _substituteRange(lineNumber, columnNumber) {
    let range =
        this._config.substituteRangeCallback ? this._config.substituteRangeCallback(lineNumber, columnNumber) : null;
    if (!range && this._config.isWordChar)
      range = this._textEditor.wordRangeForCursorPosition(lineNumber, columnNumber, this._config.isWordChar);
    return range;
  }

  /**
   * @param {!TextUtils.TextRange} queryRange
   * @param {!TextUtils.TextRange} substituteRange
   * @param {boolean=} force
   * @return {!Promise.<!UI.SuggestBox.Suggestions>}
   */
  _wordsWithQuery(queryRange, substituteRange, force) {
    const external =
        this._config.suggestionsCallback ? this._config.suggestionsCallback(queryRange, substituteRange, force) : null;
    if (external)
      return external;

    if (!this._dictionary || (!force && queryRange.isEmpty()))
      return Promise.resolve([]);

    let completions = this._dictionary.wordsWithPrefix(this._textEditor.text(queryRange));
    const substituteWord = this._textEditor.text(substituteRange);
    if (this._dictionary.wordCount(substituteWord) === 1)
      completions = completions.filter(word => word !== substituteWord);

    completions.sort((a, b) => this._dictionary.wordCount(b) - this._dictionary.wordCount(a) || a.length - b.length);
    return Promise.resolve(completions.map(item => ({text: item})));
  }

  /**
   * @param {!CodeMirror} codeMirror
   * @param {!Array.<!CodeMirror.ChangeObject>} changes
   */
  _changes(codeMirror, changes) {
    if (!changes.length)
      return;

    if (this._dictionary && this._updatedLines) {
      for (const lineNumber in this._updatedLines)
        this._removeWordsFromText(this._updatedLines[lineNumber]);
      delete this._updatedLines;

      const linesToUpdate = {};
      for (let changeIndex = 0; changeIndex < changes.length; ++changeIndex) {
        const changeObject = changes[changeIndex];
        const editInfo = TextEditor.CodeMirrorUtils.changeObjectToEditOperation(changeObject);
        for (let i = editInfo.newRange.startLine; i <= editInfo.newRange.endLine; ++i)
          linesToUpdate[i] = this._codeMirror.getLine(i);
      }
      for (const lineNumber in linesToUpdate)
        this._addWordsFromText(linesToUpdate[lineNumber]);
    }

    let singleCharInput = false;
    let singleCharDelete = false;
    const cursor = this._codeMirror.getCursor('head');
    for (let changeIndex = 0; changeIndex < changes.length; ++changeIndex) {
      const changeObject = changes[changeIndex];
      if (changeObject.origin === '+input' && changeObject.text.length === 1 && changeObject.text[0].length === 1 &&
          changeObject.to.line === cursor.line && changeObject.to.ch + 1 === cursor.ch) {
        singleCharInput = true;
        break;
      }
      if (changeObject.origin === '+delete' && changeObject.removed.length === 1 &&
          changeObject.removed[0].length === 1 && changeObject.to.line === cursor.line &&
          changeObject.to.ch - 1 === cursor.ch) {
        singleCharDelete = true;
        break;
      }
    }
    if (this._queryRange) {
      if (singleCharInput)
        this._queryRange.endColumn++;
      else if (singleCharDelete)
        this._queryRange.endColumn--;
      if (singleCharDelete || singleCharInput)
        this._setHint(this._lastHintText);
    }

    if (singleCharInput || singleCharDelete)
      setImmediate(this.autocomplete.bind(this));
    else
      this.clearAutocomplete();
  }

  _blur() {
    this.clearAutocomplete();
  }

  /**
   * @param {!TextUtils.TextRange} mainSelection
   * @return {boolean}
   */
  _validateSelectionsContexts(mainSelection) {
    const selections = this._codeMirror.listSelections();
    if (selections.length <= 1)
      return true;
    const mainSelectionContext = this._textEditor.text(mainSelection);
    for (let i = 0; i < selections.length; ++i) {
      const wordRange = this._substituteRange(selections[i].head.line, selections[i].head.ch);
      if (!wordRange)
        return false;
      const context = this._textEditor.text(wordRange);
      if (context !== mainSelectionContext)
        return false;
    }
    return true;
  }

  /**
   * @param {boolean=} force
   */
  autocomplete(force) {
    this._initializeIfNeeded();
    if (this._codeMirror.somethingSelected()) {
      this._hideSuggestBox();
      return;
    }

    const cursor = this._codeMirror.getCursor('head');
    const substituteRange = this._substituteRange(cursor.line, cursor.ch);
    if (!substituteRange || !this._validateSelectionsContexts(substituteRange)) {
      this._hideSuggestBox();
      return;
    }

    const queryRange = substituteRange.clone();
    queryRange.endColumn = cursor.ch;
    const query = this._textEditor.text(queryRange);
    let hadSuggestBox = false;
    if (this._suggestBox)
      hadSuggestBox = true;
    this._wordsWithQuery(queryRange, substituteRange, force).then(wordsAcquired.bind(this));

    /**
     * @param {!UI.SuggestBox.Suggestions} wordsWithQuery
     * @this {TextEditor.TextEditorAutocompleteController}
     */
    function wordsAcquired(wordsWithQuery) {
      if (!wordsWithQuery.length || (wordsWithQuery.length === 1 && query === wordsWithQuery[0].text) ||
          (!this._suggestBox && hadSuggestBox)) {
        this._hideSuggestBox();
        this._onSuggestionsShownForTest([]);
        return;
      }
      if (!this._suggestBox) {
        this._suggestBox = new UI.SuggestBox(this, 20);
        if (this._config.anchorBehavior)
          this._suggestBox.setAnchorBehavior(this._config.anchorBehavior);
      }

      const oldQueryRange = this._queryRange;
      this._queryRange = queryRange;
      if (!oldQueryRange || queryRange.startLine !== oldQueryRange.startLine ||
          queryRange.startColumn !== oldQueryRange.startColumn)
        this._updateAnchorBox();
      this._suggestBox.updateSuggestions(this._anchorBox, wordsWithQuery, true, !this._isCursorAtEndOfLine(), query);
      if (this._suggestBox.visible)
        this._tooltipGlassPane.hide();
      this._onSuggestionsShownForTest(wordsWithQuery);
    }
  }

  /**
   * @param {string} hint
   */
  _setHint(hint) {
    const query = this._textEditor.text(this._queryRange);
    if (!hint || !this._isCursorAtEndOfLine() || !hint.startsWith(query)) {
      this._clearHint();
      return;
    }
    const suffix = hint.substring(query.length).split('\n')[0];
    this._hintElement.textContent = suffix.trimEnd(10000);
    const cursor = this._codeMirror.getCursor('to');
    if (this._hintMarker) {
      const position = this._hintMarker.position();
      if (!position || !position.equal(TextUtils.TextRange.createFromLocation(cursor.line, cursor.ch))) {
        this._hintMarker.clear();
        this._hintMarker = null;
      }
    }

    if (!this._hintMarker) {
      this._hintMarker = this._textEditor.addBookmark(
          cursor.line, cursor.ch, this._hintElement, TextEditor.TextEditorAutocompleteController.HintBookmark, true);
    } else if (this._lastHintText !== hint) {
      this._hintMarker.refresh();
    }
    this._lastHintText = hint;
  }

  _clearHint() {
    if (!this._hintElement.textContent)
      return;
    this._lastHintText = '';
    this._hintElement.textContent = '';
    if (this._hintMarker)
      this._hintMarker.refresh();
  }

  /**
   * @param {!UI.SuggestBox.Suggestions} suggestions
   */
  _onSuggestionsShownForTest(suggestions) {
  }

  _onSuggestionsHiddenForTest() {
  }

  clearAutocomplete() {
    this._tooltipGlassPane.hide();
    this._hideSuggestBox();
  }

  _hideSuggestBox() {
    if (!this._suggestBox)
      return;
    this._suggestBox.hide();
    this._suggestBox = null;
    this._queryRange = null;
    this._anchorBox = null;
    this._currentSuggestion = null;
    this._textEditor.dispatchEventToListeners(UI.TextEditor.Events.SuggestionChanged);
    this._clearHint();
    this._onSuggestionsHiddenForTest();
  }

  /**
   * @param {!KeyboardEvent} event
   * @return {boolean}
   */
  keyDown(event) {
    if (this._tooltipGlassPane.isShowing() && event.keyCode === UI.KeyboardShortcut.Keys.Esc.code) {
      this._tooltipGlassPane.hide();
      return true;
    }
    if (!this._suggestBox)
      return false;
    switch (event.keyCode) {
      case UI.KeyboardShortcut.Keys.Tab.code:
        this._suggestBox.acceptSuggestion();
        this.clearAutocomplete();
        return true;
      case UI.KeyboardShortcut.Keys.End.code:
      case UI.KeyboardShortcut.Keys.Right.code:
        if (this._isCursorAtEndOfLine()) {
          this._suggestBox.acceptSuggestion();
          this.clearAutocomplete();
          return true;
        } else {
          this.clearAutocomplete();
          return false;
        }
      case UI.KeyboardShortcut.Keys.Left.code:
      case UI.KeyboardShortcut.Keys.Home.code:
        this.clearAutocomplete();
        return false;
      case UI.KeyboardShortcut.Keys.Esc.code:
        this.clearAutocomplete();
        return true;
    }
    return this._suggestBox.keyPressed(event);
  }

  /**
   * @return {boolean}
   */
  _isCursorAtEndOfLine() {
    const cursor = this._codeMirror.getCursor('to');
    return cursor.ch === this._codeMirror.getLine(cursor.line).length;
  }

  /**
   * @override
   * @param {string} suggestion
   * @param {boolean=} isIntermediateSuggestion
   */
  applySuggestion(suggestion, isIntermediateSuggestion) {
    const oldSuggestion = this._currentSuggestion;
    this._currentSuggestion = suggestion;
    this._setHint(suggestion);
    if (oldSuggestion !== suggestion)
      this._textEditor.dispatchEventToListeners(UI.TextEditor.Events.SuggestionChanged);
  }

  /**
   * @override
   */
  acceptSuggestion() {
    const selections = this._codeMirror.listSelections().slice();
    const queryLength = this._queryRange.endColumn - this._queryRange.startColumn;
    const suggestion = this._currentSuggestion;
    this._codeMirror.operation(() => {
      for (let i = selections.length - 1; i >= 0; --i) {
        const start = selections[i].head;
        const end = new CodeMirror.Pos(start.line, start.ch - queryLength);
        this._codeMirror.replaceRange(suggestion, start, end, '+autocomplete');
      }
    });
  }

  /**
   * @return {string}
   */
  textWithCurrentSuggestion() {
    if (!this._queryRange || this._currentSuggestion === null)
      return this._codeMirror.getValue();

    const selections = this._codeMirror.listSelections().slice();
    let last = {line: 0, column: 0};
    let text = '';
    const queryLength = this._queryRange.endColumn - this._queryRange.startColumn;
    for (const selection of selections) {
      const range =
          new TextUtils.TextRange(last.line, last.column, selection.head.line, selection.head.ch - queryLength);
      text += this._textEditor.text(range);
      text += this._currentSuggestion;
      last = {line: selection.head.line, column: selection.head.ch};
    }
    const range = new TextUtils.TextRange(last.line, last.column, Infinity, Infinity);
    text += this._textEditor.text(range);
    return text;
  }

  _onScroll() {
    this._tooltipGlassPane.hide();
    if (!this._suggestBox)
      return;
    const cursor = this._codeMirror.getCursor();
    const scrollInfo = this._codeMirror.getScrollInfo();
    const topmostLineNumber = this._codeMirror.lineAtHeight(scrollInfo.top, 'local');
    const bottomLine = this._codeMirror.lineAtHeight(scrollInfo.top + scrollInfo.clientHeight, 'local');
    if (cursor.line < topmostLineNumber || cursor.line > bottomLine) {
      this.clearAutocomplete();
    } else {
      this._updateAnchorBox();
      this._suggestBox.setPosition(this._anchorBox);
    }
  }

  async _updateTooltip() {
    const cursor = this._codeMirror.getCursor();
    const tooltip = this._config.tooltipCallback ? await this._config.tooltipCallback(cursor.line, cursor.ch) : null;
    const newCursor = this._codeMirror.getCursor();

    if (newCursor.line !== cursor.line && newCursor.ch !== cursor.ch)
      return;
    if (this._suggestBox && this._suggestBox.visible)
      return;

    if (!tooltip) {
      this._tooltipGlassPane.hide();
      return;
    }
    const metrics = this._textEditor.cursorPositionToCoordinates(cursor.line, cursor.ch);
    if (!metrics) {
      this._tooltipGlassPane.hide();
      return;
    }

    this._tooltipGlassPane.setContentAnchorBox(new AnchorBox(metrics.x, metrics.y, 0, metrics.height));
    this._tooltipElement.removeChildren();
    this._tooltipElement.appendChild(tooltip);
    this._tooltipGlassPane.show(/** @type {!Document} */ (this._textEditor.element.ownerDocument));
  }

  _onCursorActivity() {
    this._updateTooltip();
    if (!this._suggestBox)
      return;
    const cursor = this._codeMirror.getCursor();
    let shouldCloseAutocomplete =
        !(cursor.line === this._queryRange.startLine && this._queryRange.startColumn <= cursor.ch &&
          cursor.ch <= this._queryRange.endColumn);
    // Try not to hide autocomplete when user types in.
    if (cursor.line === this._queryRange.startLine && cursor.ch === this._queryRange.endColumn + 1) {
      const line = this._codeMirror.getLine(cursor.line);
      shouldCloseAutocomplete = this._config.isWordChar ? !this._config.isWordChar(line.charAt(cursor.ch - 1)) : false;
    }
    if (shouldCloseAutocomplete)
      this.clearAutocomplete();
    this._onCursorActivityHandledForTest();
  }

  _onCursorActivityHandledForTest() {
  }

  _updateAnchorBox() {
    const line = this._queryRange.startLine;
    const column = this._queryRange.startColumn;
    const metrics = this._textEditor.cursorPositionToCoordinates(line, column);
    this._anchorBox = metrics ? new AnchorBox(metrics.x, metrics.y, 0, metrics.height) : null;
  }
};

TextEditor.TextEditorAutocompleteController.HintBookmark = Symbol('hint');
