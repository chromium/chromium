/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
/**
 * @implements {UI.TextEditor}
 * @unrestricted
 */
TextEditor.CodeMirrorTextEditor = class extends UI.VBox {
  /**
   * @param {!UI.TextEditor.Options} options
   */
  constructor(options) {
    super();
    this._options = options;

    this.registerRequiredCSS('cm/codemirror.css');
    this.registerRequiredCSS('text_editor/cmdevtools.css');

    TextEditor.CodeMirrorUtils.appendThemeStyle(this.element);

    this._codeMirror = new CodeMirror(this.element, {
      lineNumbers: options.lineNumbers,
      matchBrackets: true,
      smartIndent: true,
      styleSelectedText: true,
      electricChars: true,
      styleActiveLine: true,
      indentUnit: 4,
      lineWrapping: options.lineWrapping,
      lineWiseCopyCut: false,
      tabIndex: 0,
      pollInterval: Math.pow(2, 31) - 1,  // ~25 days
      inputStyle: 'devToolsAccessibleTextArea'
    });
    this._codeMirrorElement = this.element.lastElementChild;

    this._codeMirror._codeMirrorTextEditor = this;

    CodeMirror.keyMap['devtools-common'] = {
      'Left': 'goCharLeft',
      'Right': 'goCharRight',
      'Up': 'goLineUp',
      'Down': 'goLineDown',
      'End': 'goLineEnd',
      'Home': 'goLineStartSmart',
      'PageUp': 'goSmartPageUp',
      'PageDown': 'goSmartPageDown',
      'Delete': 'delCharAfter',
      'Backspace': 'delCharBefore',
      'Tab': 'defaultTab',
      'Shift-Tab': 'indentLessOrPass',
      'Enter': 'newlineAndIndent',
      'Ctrl-Space': 'autocomplete',
      'Esc': 'dismiss',
      'Ctrl-M': 'gotoMatchingBracket'
    };

    CodeMirror.keyMap['devtools-pc'] = {
      'Ctrl-A': 'selectAll',
      'Ctrl-Z': 'undoAndReveal',
      'Shift-Ctrl-Z': 'redoAndReveal',
      'Ctrl-Y': 'redo',
      'Ctrl-Home': 'goDocStart',
      'Ctrl-Up': 'goDocStart',
      'Ctrl-End': 'goDocEnd',
      'Ctrl-Down': 'goDocEnd',
      'Ctrl-Left': 'goGroupLeft',
      'Ctrl-Right': 'goGroupRight',
      'Alt-Left': 'moveCamelLeft',
      'Alt-Right': 'moveCamelRight',
      'Shift-Alt-Left': 'selectCamelLeft',
      'Shift-Alt-Right': 'selectCamelRight',
      'Ctrl-Backspace': 'delGroupBefore',
      'Ctrl-Delete': 'delGroupAfter',
      'Ctrl-/': 'toggleComment',
      'Ctrl-D': 'selectNextOccurrence',
      'Ctrl-U': 'undoLastSelection',
      fallthrough: 'devtools-common'
    };

    CodeMirror.keyMap['devtools-mac'] = {
      'Cmd-A': 'selectAll',
      'Cmd-Z': 'undoAndReveal',
      'Shift-Cmd-Z': 'redoAndReveal',
      'Cmd-Up': 'goDocStart',
      'Cmd-Down': 'goDocEnd',
      'Alt-Left': 'goGroupLeft',
      'Alt-Right': 'goGroupRight',
      'Ctrl-Left': 'moveCamelLeft',
      'Ctrl-Right': 'moveCamelRight',
      'Ctrl-A': 'goLineLeft',
      'Ctrl-E': 'goLineRight',
      'Ctrl-B': 'goCharLeft',
      'Ctrl-F': 'goCharRight',
      'Ctrl-Alt-B': 'goGroupLeft',
      'Ctrl-Alt-F': 'goGroupRight',
      'Ctrl-H': 'delCharBefore',
      'Ctrl-D': 'delCharAfter',
      'Ctrl-K': 'killLine',
      'Ctrl-T': 'transposeChars',
      'Shift-Ctrl-Left': 'selectCamelLeft',
      'Shift-Ctrl-Right': 'selectCamelRight',
      'Cmd-Left': 'goLineStartSmart',
      'Cmd-Right': 'goLineEnd',
      'Cmd-Backspace': 'delLineLeft',
      'Alt-Backspace': 'delGroupBefore',
      'Alt-Delete': 'delGroupAfter',
      'Cmd-/': 'toggleComment',
      'Cmd-D': 'selectNextOccurrence',
      'Cmd-U': 'undoLastSelection',
      fallthrough: 'devtools-common'
    };

    if (options.bracketMatchingSetting)
      options.bracketMatchingSetting.addChangeListener(this._enableBracketMatchingIfNeeded, this);
    this._enableBracketMatchingIfNeeded();

    this._codeMirror.setOption('keyMap', Host.isMac() ? 'devtools-mac' : 'devtools-pc');

    this._codeMirror.setOption('flattenSpans', false);

    let maxHighlightLength = options.maxHighlightLength;
    if (typeof maxHighlightLength !== 'number')
      maxHighlightLength = TextEditor.CodeMirrorTextEditor.maxHighlightLength;
    this._codeMirror.setOption('maxHighlightLength', maxHighlightLength);
    this._codeMirror.setOption('mode', null);
    this._codeMirror.setOption('crudeMeasuringFrom', 1000);

    this._shouldClearHistory = true;
    this._lineSeparator = '\n';

    TextEditor.CodeMirrorTextEditor._fixWordMovement(this._codeMirror);

    this._selectNextOccurrenceController =
        new TextEditor.CodeMirrorTextEditor.SelectNextOccurrenceController(this, this._codeMirror);

    this._codeMirror.on('changes', this._changes.bind(this));
    this._codeMirror.on('beforeSelectionChange', this._beforeSelectionChange.bind(this));

    this.element.style.overflow = 'hidden';
    this._codeMirrorElement.classList.add('source-code');
    this._codeMirrorElement.classList.add('fill');

    /** @type {!Multimap<number, !TextEditor.CodeMirrorTextEditor.Decoration>} */
    this._decorations = new Multimap();

    this.element.addEventListener('keydown', this._handleKeyDown.bind(this), true);
    this.element.addEventListener('keydown', this._handlePostKeyDown.bind(this), false);

    this._needsRefresh = true;

    this._readOnly = false;

    this._mimeType = '';
    if (options.mimeType)
      this.setMimeType(options.mimeType);
    if (options.autoHeight)
      this._codeMirror.setSize(null, 'auto');

    this._placeholderElement = null;
    if (options.placeholder) {
      this._placeholderElement = createElement('pre');
      this._placeholderElement.classList.add('placeholder-text');
      this._placeholderElement.textContent = options.placeholder;
      this._updatePlaceholder();
    }
  }

  /**
   * @param {!CodeMirror} codeMirror
   */
  static autocompleteCommand(codeMirror) {
    const autocompleteController = codeMirror._codeMirrorTextEditor._autocompleteController;
    if (autocompleteController)
      autocompleteController.autocomplete(true);
  }

  /**
   * @param {!CodeMirror} codeMirror
   */
  static undoLastSelectionCommand(codeMirror) {
    codeMirror._codeMirrorTextEditor._selectNextOccurrenceController.undoLastSelection();
  }

  /**
   * @param {!CodeMirror} codeMirror
   */
  static selectNextOccurrenceCommand(codeMirror) {
    codeMirror._codeMirrorTextEditor._selectNextOccurrenceController.selectNextOccurrence();
  }

  /**
   * @param {boolean} shift
   * @param {!CodeMirror} codeMirror
   */
  static moveCamelLeftCommand(shift, codeMirror) {
    codeMirror._codeMirrorTextEditor._doCamelCaseMovement(-1, shift);
  }

  /**
   * @param {boolean} shift
   * @param {!CodeMirror} codeMirror
   */
  static moveCamelRightCommand(shift, codeMirror) {
    codeMirror._codeMirrorTextEditor._doCamelCaseMovement(1, shift);
  }

  /**
   * @param {string} modeName
   * @param {string} tokenPrefix
   */
  static _overrideModeWithPrefixedTokens(modeName, tokenPrefix) {
    const oldModeName = modeName + '-old';
    if (CodeMirror.modes[oldModeName])
      return;

    CodeMirror.defineMode(oldModeName, CodeMirror.modes[modeName]);
    CodeMirror.defineMode(modeName, modeConstructor);

    function modeConstructor(config, parserConfig) {
      const innerConfig = {};
      for (const i in parserConfig)
        innerConfig[i] = parserConfig[i];
      innerConfig.name = oldModeName;
      const codeMirrorMode = CodeMirror.getMode(config, innerConfig);
      codeMirrorMode.name = modeName;
      codeMirrorMode.token = tokenOverride.bind(null, codeMirrorMode.token);
      return codeMirrorMode;
    }

    function tokenOverride(superToken, stream, state) {
      const token = superToken(stream, state);
      return token ? tokenPrefix + token.split(/ +/).join(' ' + tokenPrefix) : token;
    }
  }

  /**
   * @param {string} mimeType
   * @return {!Array<!Runtime.Extension>}}
   */
  static _collectUninstalledModes(mimeType) {
    const installed = TextEditor.CodeMirrorTextEditor._loadedMimeModeExtensions;

    const nameToExtension = new Map();
    const extensions = self.runtime.extensions(TextEditor.CodeMirrorMimeMode);
    for (const extension of extensions)
      nameToExtension.set(extension.descriptor()['fileName'], extension);

    const modesToLoad = new Set();
    for (const extension of extensions) {
      const descriptor = extension.descriptor();
      if (installed.has(extension) || descriptor['mimeTypes'].indexOf(mimeType) === -1)
        continue;

      modesToLoad.add(extension);
      const deps = descriptor['dependencies'] || [];
      for (let i = 0; i < deps.length; ++i) {
        const extension = nameToExtension.get(deps[i]);
        if (extension && !installed.has(extension))
          modesToLoad.add(extension);
      }
    }
    return Array.from(modesToLoad);
  }

  /**
   * @param {!Array<!Runtime.Extension>} extensions
   * @return {!Promise}
   */
  static _installMimeTypeModes(extensions) {
    const promises = extensions.map(extension => extension.instance().then(installMode.bind(null, extension)));
    return Promise.all(promises);

    /**
     * @param {!Runtime.Extension} extension
     * @param {!Object} instance
     */
    function installMode(extension, instance) {
      if (TextEditor.CodeMirrorTextEditor._loadedMimeModeExtensions.has(extension))
        return;
      const mode = /** @type {!TextEditor.CodeMirrorMimeMode} */ (instance);
      mode.install(extension);
      TextEditor.CodeMirrorTextEditor._loadedMimeModeExtensions.add(extension);
    }
  }

  /**
   * @param {!CodeMirror} codeMirror
   */
  static _fixWordMovement(codeMirror) {
    function moveLeft(shift, codeMirror) {
      codeMirror.setExtending(shift);
      const cursor = codeMirror.getCursor('head');
      codeMirror.execCommand('goGroupLeft');
      const newCursor = codeMirror.getCursor('head');
      if (newCursor.ch === 0 && newCursor.line !== 0) {
        codeMirror.setExtending(false);
        return;
      }

      const skippedText = codeMirror.getRange(newCursor, cursor, '#');
      if (/^\s+$/.test(skippedText))
        codeMirror.execCommand('goGroupLeft');
      codeMirror.setExtending(false);
    }

    function moveRight(shift, codeMirror) {
      codeMirror.setExtending(shift);
      const cursor = codeMirror.getCursor('head');
      codeMirror.execCommand('goGroupRight');
      const newCursor = codeMirror.getCursor('head');
      if (newCursor.ch === 0 && newCursor.line !== 0) {
        codeMirror.setExtending(false);
        return;
      }

      const skippedText = codeMirror.getRange(cursor, newCursor, '#');
      if (/^\s+$/.test(skippedText))
        codeMirror.execCommand('goGroupRight');
      codeMirror.setExtending(false);
    }

    const modifierKey = Host.isMac() ? 'Alt' : 'Ctrl';
    const leftKey = modifierKey + '-Left';
    const rightKey = modifierKey + '-Right';
    const keyMap = {};
    keyMap[leftKey] = moveLeft.bind(null, false);
    keyMap[rightKey] = moveRight.bind(null, false);
    keyMap['Shift-' + leftKey] = moveLeft.bind(null, true);
    keyMap['Shift-' + rightKey] = moveRight.bind(null, true);
    codeMirror.addKeyMap(keyMap);
  }

  /**
   * @protected
   * @return {!CodeMirror}
   */
  codeMirror() {
    return this._codeMirror;
  }

  /**
   * @override
   * @return {!UI.Widget}
   */
  widget() {
    return this;
  }

  /**
   * @param {number} lineNumber
   * @param {number} lineLength
   * @param {number} charNumber
   * @return {{lineNumber: number, columnNumber: number}}
   */
  _normalizePositionForOverlappingColumn(lineNumber, lineLength, charNumber) {
    const linesCount = this._codeMirror.lineCount();
    let columnNumber = charNumber;
    if (charNumber < 0 && lineNumber > 0) {
      --lineNumber;
      columnNumber = this.line(lineNumber).length;
    } else if (charNumber >= lineLength && lineNumber < linesCount - 1) {
      ++lineNumber;
      columnNumber = 0;
    } else {
      columnNumber = Number.constrain(charNumber, 0, lineLength);
    }
    return {lineNumber: lineNumber, columnNumber: columnNumber};
  }

  /**
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {number} direction
   * @return {{lineNumber: number, columnNumber: number}}
   */
  _camelCaseMoveFromPosition(lineNumber, columnNumber, direction) {
    /**
     * @param {number} charNumber
     * @param {number} length
     * @return {boolean}
     */
    function valid(charNumber, length) {
      return charNumber >= 0 && charNumber < length;
    }

    /**
     * @param {string} text
     * @param {number} charNumber
     * @return {boolean}
     */
    function isWordStart(text, charNumber) {
      const position = charNumber;
      const nextPosition = charNumber + 1;
      return valid(position, text.length) && valid(nextPosition, text.length) &&
          TextUtils.TextUtils.isWordChar(text[position]) && TextUtils.TextUtils.isWordChar(text[nextPosition]) &&
          TextUtils.TextUtils.isUpperCase(text[position]) && TextUtils.TextUtils.isLowerCase(text[nextPosition]);
    }

    /**
     * @param {string} text
     * @param {number} charNumber
     * @return {boolean}
     */
    function isWordEnd(text, charNumber) {
      const position = charNumber;
      const prevPosition = charNumber - 1;
      return valid(position, text.length) && valid(prevPosition, text.length) &&
          TextUtils.TextUtils.isWordChar(text[position]) && TextUtils.TextUtils.isWordChar(text[prevPosition]) &&
          TextUtils.TextUtils.isUpperCase(text[position]) && TextUtils.TextUtils.isLowerCase(text[prevPosition]);
    }

    /**
     * @param {number} lineNumber
     * @param {number} lineLength
     * @param {number} columnNumber
     * @return {{lineNumber: number, columnNumber: number}}
     */
    function constrainPosition(lineNumber, lineLength, columnNumber) {
      return {lineNumber: lineNumber, columnNumber: Number.constrain(columnNumber, 0, lineLength)};
    }

    const text = this.line(lineNumber);
    const length = text.length;

    if ((columnNumber === length && direction === 1) || (columnNumber === 0 && direction === -1))
      return this._normalizePositionForOverlappingColumn(lineNumber, length, columnNumber + direction);

    let charNumber = direction === 1 ? columnNumber : columnNumber - 1;

    // Move through initial spaces if any.
    while (valid(charNumber, length) && TextUtils.TextUtils.isSpaceChar(text[charNumber]))
      charNumber += direction;
    if (!valid(charNumber, length))
      return constrainPosition(lineNumber, length, charNumber);

    if (TextUtils.TextUtils.isStopChar(text[charNumber])) {
      while (valid(charNumber, length) && TextUtils.TextUtils.isStopChar(text[charNumber]))
        charNumber += direction;
      if (!valid(charNumber, length))
        return constrainPosition(lineNumber, length, charNumber);
      return {lineNumber: lineNumber, columnNumber: direction === -1 ? charNumber + 1 : charNumber};
    }

    charNumber += direction;
    while (valid(charNumber, length) && !isWordStart(text, charNumber) && !isWordEnd(text, charNumber) &&
           TextUtils.TextUtils.isWordChar(text[charNumber]))
      charNumber += direction;

    if (!valid(charNumber, length))
      return constrainPosition(lineNumber, length, charNumber);
    if (isWordStart(text, charNumber) || isWordEnd(text, charNumber))
      return {lineNumber: lineNumber, columnNumber: charNumber};


    return {lineNumber: lineNumber, columnNumber: direction === -1 ? charNumber + 1 : charNumber};
  }

  /**
   * @param {number} direction
   * @param {boolean} shift
   */
  _doCamelCaseMovement(direction, shift) {
    const selections = this.selections();
    for (let i = 0; i < selections.length; ++i) {
      const selection = selections[i];
      const move = this._camelCaseMoveFromPosition(selection.endLine, selection.endColumn, direction);
      selection.endLine = move.lineNumber;
      selection.endColumn = move.columnNumber;
      if (!shift)
        selections[i] = selection.collapseToEnd();
    }
    this.setSelections(selections);
  }

  dispose() {
    if (this._options.bracketMatchingSetting)
      this._options.bracketMatchingSetting.removeChangeListener(this._enableBracketMatchingIfNeeded, this);
  }

  _enableBracketMatchingIfNeeded() {
    this._codeMirror.setOption(
        'autoCloseBrackets', (this._options.bracketMatchingSetting && this._options.bracketMatchingSetting.get()) ?
            {explode: false} :
            false);
  }

  /**
   * @override
   */
  wasShown() {
    if (this._needsRefresh)
      this.refresh();
  }

  /**
   * @protected
   */
  refresh() {
    if (this.isShowing()) {
      this._codeMirror.refresh();
      this._needsRefresh = false;
      return;
    }
    this._needsRefresh = true;
  }

  /**
   * @override
   */
  willHide() {
    delete this._editorSizeInSync;
  }

  undo() {
    this._codeMirror.undo();
  }

  redo() {
    this._codeMirror.redo();
  }

  /**
   * @param {!Event} e
   */
  _handleKeyDown(e) {
    if (this._autocompleteController && this._autocompleteController.keyDown(e))
      e.consume(true);
  }

  /**
   * @param {!Event} e
   */
  _handlePostKeyDown(e) {
    if (e.defaultPrevented)
      e.consume(true);
  }

  /**
   * @override
   * @param {?UI.AutocompleteConfig} config
   */
  configureAutocomplete(config) {
    if (this._autocompleteController) {
      this._autocompleteController.dispose();
      delete this._autocompleteController;
    }

    if (config)
      this._autocompleteController = new TextEditor.TextEditorAutocompleteController(this, this._codeMirror, config);
  }

  /**
   * @param {number} lineNumber
   * @param {number} column
   * @return {?{x: number, y: number, height: number}}
   */
  cursorPositionToCoordinates(lineNumber, column) {
    if (lineNumber >= this._codeMirror.lineCount() || lineNumber < 0 || column < 0 ||
        column > this._codeMirror.getLine(lineNumber).length)
      return null;
    const metrics = this._codeMirror.cursorCoords(new CodeMirror.Pos(lineNumber, column));
    return {x: metrics.left, y: metrics.top, height: metrics.bottom - metrics.top};
  }

  /**
   * @param {number} x
   * @param {number} y
   * @return {?TextUtils.TextRange}
   */
  coordinatesToCursorPosition(x, y) {
    const element = this.element.ownerDocument.elementFromPoint(x, y);
    if (!element || !element.isSelfOrDescendant(this._codeMirror.getWrapperElement()))
      return null;
    const gutterBox = this._codeMirror.getGutterElement().boxInWindow();
    if (x >= gutterBox.x && x <= gutterBox.x + gutterBox.width && y >= gutterBox.y &&
        y <= gutterBox.y + gutterBox.height)
      return null;
    const coords = this._codeMirror.coordsChar({left: x, top: y});
    return TextEditor.CodeMirrorUtils.toRange(coords, coords);
  }

  /**
   * @override
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {!{x: number, y: number}}
   */
  visualCoordinates(lineNumber, columnNumber) {
    const metrics = this._codeMirror.cursorCoords(new CodeMirror.Pos(lineNumber, columnNumber));
    return {x: metrics.left, y: metrics.top};
  }

  /**
   * @override
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?{startColumn: number, endColumn: number, type: string}}
   */
  tokenAtTextPosition(lineNumber, columnNumber) {
    if (lineNumber < 0 || lineNumber >= this._codeMirror.lineCount())
      return null;
    const token = this._codeMirror.getTokenAt(new CodeMirror.Pos(lineNumber, (columnNumber || 0) + 1));
    if (!token)
      return null;
    return {startColumn: token.start, endColumn: token.end, type: token.type};
  }

  /**
   * @param {number} generation
   * @return {boolean}
   */
  isClean(generation) {
    return this._codeMirror.isClean(generation);
  }

  /**
   * @return {number}
   */
  markClean() {
    return this._codeMirror.changeGeneration(true);
  }

  /**
   * @return {boolean}
   */
  _hasLongLines() {
    function lineIterator(lineHandle) {
      if (lineHandle.text.length > TextEditor.CodeMirrorTextEditor.LongLineModeLineLengthThreshold)
        hasLongLines = true;
      return hasLongLines;
    }
    let hasLongLines = false;
    this._codeMirror.eachLine(lineIterator);
    return hasLongLines;
  }

  _enableLongLinesMode() {
    this._codeMirror.setOption('styleSelectedText', false);
  }

  _disableLongLinesMode() {
    this._codeMirror.setOption('styleSelectedText', true);
  }

  /**
   * @param {string} mimeType
   */
  setMimeType(mimeType) {
    this._mimeType = mimeType;
    const modesToLoad = TextEditor.CodeMirrorTextEditor._collectUninstalledModes(mimeType);

    if (!modesToLoad.length)
      setMode.call(this);
    else
      TextEditor.CodeMirrorTextEditor._installMimeTypeModes(modesToLoad).then(setMode.bind(this));

    /**
     * @this {TextEditor.CodeMirrorTextEditor}
     */
    function setMode() {
      const rewrittenMimeType = this.rewriteMimeType(mimeType);
      if (this._codeMirror.options.mode !== rewrittenMimeType)
        this._codeMirror.setOption('mode', rewrittenMimeType);
    }
  }

  /**
   * @param {!Object} mode
   */
  setHighlightMode(mode) {
    this._mimeType = '';
    this._codeMirror.setOption('mode', mode);
  }

  /**
   * @protected
   * @param {string} mimeType
   */
  rewriteMimeType(mimeType) {
    // Overridden in SourcesTextEditor
    return mimeType;
  }

  /**
   * @protected
   * @return {string}
   */
  mimeType() {
    return this._mimeType;
  }

  /**
   * @param {boolean} readOnly
   */
  setReadOnly(readOnly) {
    if (this._readOnly === readOnly)
      return;
    this.clearPositionHighlight();
    this._readOnly = readOnly;
    this.element.classList.toggle('CodeMirror-readonly', readOnly);
    this._codeMirror.setOption('readOnly', readOnly);
  }

  /**
   * @return {boolean}
   */
  readOnly() {
    return !!this._codeMirror.getOption('readOnly');
  }

  /**
   * @param {function(number):string} formatter
   */
  setLineNumberFormatter(formatter) {
    this._codeMirror.setOption('lineNumberFormatter', formatter);
  }

  /**
   * @override
   * @param {function(!KeyboardEvent)} handler
   */
  addKeyDownHandler(handler) {
    this._codeMirror.on('keydown', (CodeMirror, event) => handler(event));
  }

  /**
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {!Element} element
   * @param {symbol} type
   * @param {boolean=} insertBefore
   * @return {!TextEditor.TextEditorBookMark}
   */
  addBookmark(lineNumber, columnNumber, element, type, insertBefore) {
    const bookmark = new TextEditor.TextEditorBookMark(
        this._codeMirror.setBookmark(
            new CodeMirror.Pos(lineNumber, columnNumber), {widget: element, insertLeft: insertBefore}),
        type, this);
    this._updateDecorations(lineNumber);
    return bookmark;
  }

  /**
   * @param {!TextUtils.TextRange} range
   * @param {symbol=} type
   * @return {!Array.<!TextEditor.TextEditorBookMark>}
   */
  bookmarks(range, type) {
    const pos = TextEditor.CodeMirrorUtils.toPos(range);
    let markers = this._codeMirror.findMarksAt(pos.start);
    if (!range.isEmpty()) {
      const middleMarkers = this._codeMirror.findMarks(pos.start, pos.end);
      const endMarkers = this._codeMirror.findMarksAt(pos.end);
      markers = markers.concat(middleMarkers, endMarkers);
    }
    const bookmarks = [];
    for (let i = 0; i < markers.length; i++) {
      const bookmark = markers[i][TextEditor.TextEditorBookMark._symbol];
      if (bookmark && (!type || bookmark.type() === type))
        bookmarks.push(bookmark);
    }
    return bookmarks;
  }

  /**
   * @override
   */
  focus() {
    this._codeMirror.focus();
  }

  /**
   * @override
   * @return {boolean}
   */
  hasFocus() {
    return this._codeMirror.hasFocus();
  }

  /**
   * @param {function()} operation
   */
  operation(operation) {
    this._codeMirror.operation(operation);
  }

  /**
   * @param {number} lineNumber
   */
  scrollLineIntoView(lineNumber) {
    this._innerRevealLine(lineNumber, this._codeMirror.getScrollInfo());
  }

  /**
   * @param {number} lineNumber
   * @param {!{left: number, top: number, width: number, height: number, clientWidth: number, clientHeight: number}} scrollInfo
   */
  _innerRevealLine(lineNumber, scrollInfo) {
    const topLine = this._codeMirror.lineAtHeight(scrollInfo.top, 'local');
    const bottomLine = this._codeMirror.lineAtHeight(scrollInfo.top + scrollInfo.clientHeight, 'local');
    const linesPerScreen = bottomLine - topLine + 1;
    if (lineNumber < topLine) {
      const topLineToReveal = Math.max(lineNumber - (linesPerScreen / 2) + 1, 0) | 0;
      this._codeMirror.scrollIntoView(new CodeMirror.Pos(topLineToReveal, 0));
    } else if (lineNumber > bottomLine) {
      const bottomLineToReveal = Math.min(lineNumber + (linesPerScreen / 2) - 1, this.linesCount - 1) | 0;
      this._codeMirror.scrollIntoView(new CodeMirror.Pos(bottomLineToReveal, 0));
    }
  }

  /**
   * @param {!Element} element
   * @param {number} lineNumber
   * @param {number=} startColumn
   * @param {number=} endColumn
   */
  addDecoration(element, lineNumber, startColumn, endColumn) {
    const widget = this._codeMirror.addLineWidget(lineNumber, element);
    let update = null;
    if (typeof startColumn !== 'undefined') {
      if (typeof endColumn === 'undefined')
        endColumn = Infinity;
      update = this._updateFloatingDecoration.bind(this, element, lineNumber, startColumn, endColumn);
      update();
    }

    this._decorations.set(lineNumber, {element: element, update: update, widget: widget});
  }

  /**
   * @param {!Element} element
   * @param {number} lineNumber
   * @param {number} startColumn
   * @param {number} endColumn
   */
  _updateFloatingDecoration(element, lineNumber, startColumn, endColumn) {
    const base = this._codeMirror.cursorCoords(new CodeMirror.Pos(lineNumber, 0), 'page');
    const start = this._codeMirror.cursorCoords(new CodeMirror.Pos(lineNumber, startColumn), 'page');
    const end = this._codeMirror.charCoords(new CodeMirror.Pos(lineNumber, endColumn), 'page');
    element.style.width = (end.right - start.left) + 'px';
    element.style.left = (start.left - base.left) + 'px';
  }

  /**
   * @param {number} lineNumber
   */
  _updateDecorations(lineNumber) {
    this._decorations.get(lineNumber).forEach(innerUpdateDecorations);

    /**
     * @param {!TextEditor.CodeMirrorTextEditor.Decoration} decoration
     */
    function innerUpdateDecorations(decoration) {
      if (decoration.update)
        decoration.update();
    }
  }

  /**
   * @param {!Element} element
   * @param {number} lineNumber
   */
  removeDecoration(element, lineNumber) {
    this._decorations.get(lineNumber).forEach(innerRemoveDecoration.bind(this));

    /**
     * @this {TextEditor.CodeMirrorTextEditor}
     * @param {!TextEditor.CodeMirrorTextEditor.Decoration} decoration
     */
    function innerRemoveDecoration(decoration) {
      if (decoration.element !== element)
        return;
      this._codeMirror.removeLineWidget(decoration.widget);
      this._decorations.delete(lineNumber, decoration);
    }
  }

  /**
   * @param {number} lineNumber 0-based
   * @param {number=} columnNumber
   * @param {boolean=} shouldHighlight
   */
  revealPosition(lineNumber, columnNumber, shouldHighlight) {
    lineNumber = Number.constrain(lineNumber, 0, this._codeMirror.lineCount() - 1);
    if (typeof columnNumber !== 'number')
      columnNumber = 0;
    columnNumber = Number.constrain(columnNumber, 0, this._codeMirror.getLine(lineNumber).length);

    this.clearPositionHighlight();
    this._highlightedLine = this._codeMirror.getLineHandle(lineNumber);
    if (!this._highlightedLine)
      return;
    this.scrollLineIntoView(lineNumber);
    if (shouldHighlight) {
      this._codeMirror.addLineClass(
          this._highlightedLine, null, this._readOnly ? 'cm-readonly-highlight' : 'cm-highlight');
      if (!this._readOnly)
        this._clearHighlightTimeout = setTimeout(this.clearPositionHighlight.bind(this), 2000);
    }
    this.setSelection(TextUtils.TextRange.createFromLocation(lineNumber, columnNumber));
  }

  clearPositionHighlight() {
    if (this._clearHighlightTimeout)
      clearTimeout(this._clearHighlightTimeout);
    delete this._clearHighlightTimeout;

    if (this._highlightedLine) {
      this._codeMirror.removeLineClass(
          this._highlightedLine, null, this._readOnly ? 'cm-readonly-highlight' : 'cm-highlight');
    }
    delete this._highlightedLine;
  }

  /**
   * @override
   * @return {!Array.<!Element>}
   */
  elementsToRestoreScrollPositionsFor() {
    return [];
  }

  /**
   * @param {number} width
   * @param {number} height
   */
  _updatePaddingBottom(width, height) {
    if (!this._options.padBottom)
      return;
    const scrollInfo = this._codeMirror.getScrollInfo();
    let newPaddingBottom;
    const linesElement = this._codeMirrorElement.querySelector('.CodeMirror-lines');
    const lineCount = this._codeMirror.lineCount();
    if (lineCount <= 1) {
      newPaddingBottom = 0;
    } else {
      newPaddingBottom =
          Math.max(scrollInfo.clientHeight - this._codeMirror.getLineHandle(this._codeMirror.lastLine()).height, 0);
    }
    newPaddingBottom += 'px';
    linesElement.style.paddingBottom = newPaddingBottom;
    this._codeMirror.setSize(width, height);
  }

  _resizeEditor() {
    const parentElement = this.element.parentElement;
    if (!parentElement || !this.isShowing())
      return;
    this._codeMirror.operation(() => {
      const scrollLeft = this._codeMirror.doc.scrollLeft;
      const scrollTop = this._codeMirror.doc.scrollTop;
      const width = parentElement.offsetWidth;
      const height = parentElement.offsetHeight - this.element.offsetTop;
      if (this._options.autoHeight) {
        this._codeMirror.setSize(width, 'auto');
      } else {
        this._codeMirror.setSize(width, height);
        this._updatePaddingBottom(width, height);
      }
      this._codeMirror.scrollTo(scrollLeft, scrollTop);
    });
  }

  /**
   * @override
   */
  onResize() {
    if (this._autocompleteController)
      this._autocompleteController.clearAutocomplete();
    this._resizeEditor();
    this._editorSizeInSync = true;
    if (this._selectionSetScheduled) {
      delete this._selectionSetScheduled;
      this.setSelection(this._lastSelection);
    }
  }

  /**
   * @param {!TextUtils.TextRange} range
   * @param {string} text
   * @param {string=} origin
   * @return {!TextUtils.TextRange}
   */
  editRange(range, text, origin) {
    const pos = TextEditor.CodeMirrorUtils.toPos(range);
    this._codeMirror.replaceRange(text, pos.start, pos.end, origin);
    const newRange = TextEditor.CodeMirrorUtils.toRange(
        pos.start, this._codeMirror.posFromIndex(this._codeMirror.indexFromPos(pos.start) + text.length));
    this.dispatchEventToListeners(UI.TextEditor.Events.TextChanged, {oldRange: range, newRange: newRange});
    return newRange;
  }

  /**
   * @override
   */
  clearAutocomplete() {
    if (this._autocompleteController)
      this._autocompleteController.clearAutocomplete();
  }

  /**
   * @param {number} lineNumber
   * @param {number} column
   * @param {function(string):boolean} isWordChar
   * @return {!TextUtils.TextRange}
   */
  wordRangeForCursorPosition(lineNumber, column, isWordChar) {
    const line = this.line(lineNumber);
    let wordStart = column;
    if (column !== 0 && isWordChar(line.charAt(column - 1))) {
      wordStart = column - 1;
      while (wordStart > 0 && isWordChar(line.charAt(wordStart - 1)))
        --wordStart;
    }
    let wordEnd = column;
    while (wordEnd < line.length && isWordChar(line.charAt(wordEnd)))
      ++wordEnd;
    return new TextUtils.TextRange(lineNumber, wordStart, lineNumber, wordEnd);
  }

  /**
   * @param {!CodeMirror} codeMirror
   * @param {!Array.<!CodeMirror.ChangeObject>} changes
   */
  _changes(codeMirror, changes) {
    if (!changes.length)
      return;

    this._updatePlaceholder();

    // We do not show "scroll beyond end of file" span for one line documents, so we need to check if "document has one line" changed.
    const hasOneLine = this._codeMirror.lineCount() === 1;
    if (hasOneLine !== this._hasOneLine)
      this._resizeEditor();
    this._hasOneLine = hasOneLine;

    this._decorations.valuesArray().forEach(decoration => this._codeMirror.removeLineWidget(decoration.widget));
    this._decorations.clear();

    const edits = [];
    let currentEdit;

    for (let changeIndex = 0; changeIndex < changes.length; ++changeIndex) {
      const changeObject = changes[changeIndex];
      const edit = TextEditor.CodeMirrorUtils.changeObjectToEditOperation(changeObject);
      if (currentEdit && edit.oldRange.equal(currentEdit.newRange)) {
        currentEdit.newRange = edit.newRange;
      } else {
        currentEdit = edit;
        edits.push(currentEdit);
      }
    }

    for (let i = 0; i < edits.length; i++) {
      this.dispatchEventToListeners(
          UI.TextEditor.Events.TextChanged, {oldRange: edits[i].oldRange, newRange: edits[i].newRange});
    }
  }

  /**
   * @param {!CodeMirror} codeMirror
   * @param {{ranges: !Array.<{head: !CodeMirror.Pos, anchor: !CodeMirror.Pos}>}} selection
   */
  _beforeSelectionChange(codeMirror, selection) {
    this._selectNextOccurrenceController.selectionWillChange();
  }

  /**
   * @param {number} lineNumber
   */
  scrollToLine(lineNumber) {
    const pos = new CodeMirror.Pos(lineNumber, 0);
    const coords = this._codeMirror.charCoords(pos, 'local');
    this._codeMirror.scrollTo(0, coords.top);
  }

  /**
   * @return {number}
   */
  firstVisibleLine() {
    return this._codeMirror.lineAtHeight(this._codeMirror.getScrollInfo().top, 'local');
  }

  /**
   * @return {number}
   */
  scrollTop() {
    return this._codeMirror.getScrollInfo().top;
  }

  /**
   * @param {number} scrollTop
   */
  setScrollTop(scrollTop) {
    this._codeMirror.scrollTo(0, scrollTop);
  }

  /**
   * @return {number}
   */
  lastVisibleLine() {
    const scrollInfo = this._codeMirror.getScrollInfo();
    return this._codeMirror.lineAtHeight(scrollInfo.top + scrollInfo.clientHeight, 'local');
  }

  /**
   * @override
   * @return {!TextUtils.TextRange}
   */
  selection() {
    const start = this._codeMirror.getCursor('anchor');
    const end = this._codeMirror.getCursor('head');

    return TextEditor.CodeMirrorUtils.toRange(start, end);
  }

  /**
   * @return {!Array.<!TextUtils.TextRange>}
   */
  selections() {
    const selectionList = this._codeMirror.listSelections();
    const result = [];
    for (let i = 0; i < selectionList.length; ++i) {
      const selection = selectionList[i];
      result.push(TextEditor.CodeMirrorUtils.toRange(selection.anchor, selection.head));
    }
    return result;
  }

  /**
   * @return {?TextUtils.TextRange}
   */
  lastSelection() {
    return this._lastSelection;
  }

  /**
   * @override
   * @param {!TextUtils.TextRange} textRange
   * @param {boolean=} dontScroll
   */
  setSelection(textRange, dontScroll) {
    this._lastSelection = textRange;
    if (!this._editorSizeInSync) {
      this._selectionSetScheduled = true;
      return;
    }
    const pos = TextEditor.CodeMirrorUtils.toPos(textRange);
    this._codeMirror.setSelection(pos.start, pos.end, {scroll: !dontScroll});
  }

  /**
   * @param {!Array.<!TextUtils.TextRange>} ranges
   * @param {number=} primarySelectionIndex
   */
  setSelections(ranges, primarySelectionIndex) {
    const selections = [];
    for (let i = 0; i < ranges.length; ++i) {
      const selection = TextEditor.CodeMirrorUtils.toPos(ranges[i]);
      selections.push({anchor: selection.start, head: selection.end});
    }
    primarySelectionIndex = primarySelectionIndex || 0;
    this._codeMirror.setSelections(selections, primarySelectionIndex, {scroll: false});
  }

  /**
   * @param {string} text
   */
  _detectLineSeparator(text) {
    this._lineSeparator = text.indexOf('\r\n') >= 0 ? '\r\n' : '\n';
  }

  /**
   * @override
   * @param {string} text
   */
  setText(text) {
    if (text.length > TextEditor.CodeMirrorTextEditor.MaxEditableTextSize) {
      this.configureAutocomplete(null);
      this.setReadOnly(true);
    }
    this._codeMirror.setValue(text);
    if (this._shouldClearHistory) {
      this._codeMirror.clearHistory();
      this._shouldClearHistory = false;
    }
    this._detectLineSeparator(text);

    if (this._hasLongLines())
      this._enableLongLinesMode();
    else
      this._disableLongLinesMode();

    if (!this.isShowing())
      this.refresh();
  }

  /**
   * @override
   * @param {!TextUtils.TextRange=} textRange
   * @return {string}
   */
  text(textRange) {
    if (!textRange)
      return this._codeMirror.getValue(this._lineSeparator);
    const pos = TextEditor.CodeMirrorUtils.toPos(textRange.normalize());
    return this._codeMirror.getRange(pos.start, pos.end, this._lineSeparator);
  }

  /**
   * @override
   * @return {string}
   */
  textWithCurrentSuggestion() {
    if (!this._autocompleteController)
      return this.text();
    return this._autocompleteController.textWithCurrentSuggestion();
  }

  /**
   * @override
   * @return {!TextUtils.TextRange}
   */
  fullRange() {
    const lineCount = this.linesCount;
    const lastLine = this._codeMirror.getLine(lineCount - 1);
    return TextEditor.CodeMirrorUtils.toRange(
        new CodeMirror.Pos(0, 0), new CodeMirror.Pos(lineCount - 1, lastLine.length));
  }

  /**
   * @override
   * @param {number} lineNumber
   * @return {string}
   */
  line(lineNumber) {
    return this._codeMirror.getLine(lineNumber);
  }

  /**
   * @return {number}
   */
  get linesCount() {
    return this._codeMirror.lineCount();
  }

  /**
   * @override
   */
  newlineAndIndent() {
    this._codeMirror.execCommand('newlineAndIndent');
  }

  /**
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {!TextEditor.TextEditorPositionHandle}
   */
  textEditorPositionHandle(lineNumber, columnNumber) {
    return new TextEditor.CodeMirrorPositionHandle(this._codeMirror, new CodeMirror.Pos(lineNumber, columnNumber));
  }

  _updatePlaceholder() {
    if (!this._placeholderElement)
      return;

    this._placeholderElement.remove();

    if (this.linesCount === 1 && !this.line(0)) {
      this._codeMirror.display.lineSpace.insertBefore(
          this._placeholderElement, this._codeMirror.display.lineSpace.firstChild);
    }
  }
};

TextEditor.CodeMirrorTextEditor.maxHighlightLength = 1000;


CodeMirror.commands.autocomplete = TextEditor.CodeMirrorTextEditor.autocompleteCommand;


CodeMirror.commands.undoLastSelection = TextEditor.CodeMirrorTextEditor.undoLastSelectionCommand;


CodeMirror.commands.selectNextOccurrence = TextEditor.CodeMirrorTextEditor.selectNextOccurrenceCommand;


CodeMirror.commands.moveCamelLeft = TextEditor.CodeMirrorTextEditor.moveCamelLeftCommand.bind(null, false);
CodeMirror.commands.selectCamelLeft = TextEditor.CodeMirrorTextEditor.moveCamelLeftCommand.bind(null, true);


CodeMirror.commands.moveCamelRight = TextEditor.CodeMirrorTextEditor.moveCamelRightCommand.bind(null, false);
CodeMirror.commands.selectCamelRight = TextEditor.CodeMirrorTextEditor.moveCamelRightCommand.bind(null, true);

/**
 * @param {!CodeMirror} codeMirror
 * @return {!Object|undefined}
 */
CodeMirror.commands.indentLessOrPass = function(codeMirror) {
  const selections = codeMirror.listSelections();
  if (selections.length === 1) {
    const range = TextEditor.CodeMirrorUtils.toRange(selections[0].anchor, selections[0].head);
    if (range.isEmpty() && !/^\s/.test(codeMirror.getLine(range.startLine)))
      return CodeMirror.Pass;
  }
  codeMirror.execCommand('indentLess');
};

/**
 * @param {!CodeMirror} codeMirror
 */
CodeMirror.commands.gotoMatchingBracket = function(codeMirror) {
  const updatedSelections = [];
  const selections = codeMirror.listSelections();
  for (let i = 0; i < selections.length; ++i) {
    const selection = selections[i];
    const cursor = selection.head;
    const matchingBracket = codeMirror.findMatchingBracket(cursor, false, {maxScanLines: 10000});
    let updatedHead = cursor;
    if (matchingBracket && matchingBracket.match) {
      const columnCorrection = CodeMirror.cmpPos(matchingBracket.from, cursor) === 0 ? 1 : 0;
      updatedHead = new CodeMirror.Pos(matchingBracket.to.line, matchingBracket.to.ch + columnCorrection);
    }
    updatedSelections.push({anchor: updatedHead, head: updatedHead});
  }
  codeMirror.setSelections(updatedSelections);
};

/**
 * @param {!CodeMirror} codemirror
 */
CodeMirror.commands.undoAndReveal = function(codemirror) {
  const scrollInfo = codemirror.getScrollInfo();
  codemirror.execCommand('undo');
  const cursor = codemirror.getCursor('start');
  codemirror._codeMirrorTextEditor._innerRevealLine(cursor.line, scrollInfo);
  const autocompleteController = codemirror._codeMirrorTextEditor._autocompleteController;
  if (autocompleteController)
    autocompleteController.clearAutocomplete();
};

/**
 * @param {!CodeMirror} codemirror
 */
CodeMirror.commands.redoAndReveal = function(codemirror) {
  const scrollInfo = codemirror.getScrollInfo();
  codemirror.execCommand('redo');
  const cursor = codemirror.getCursor('start');
  codemirror._codeMirrorTextEditor._innerRevealLine(cursor.line, scrollInfo);
  const autocompleteController = codemirror._codeMirrorTextEditor._autocompleteController;
  if (autocompleteController)
    autocompleteController.clearAutocomplete();
};

/**
 * @param {!CodeMirror} codemirror
 * @return {!Object|undefined}
 */
CodeMirror.commands.dismiss = function(codemirror) {
  const selections = codemirror.listSelections();
  const selection = selections[0];
  if (selections.length === 1) {
    if (TextEditor.CodeMirrorUtils.toRange(selection.anchor, selection.head).isEmpty())
      return CodeMirror.Pass;
    codemirror.setSelection(selection.anchor, selection.anchor, {scroll: false});
    codemirror._codeMirrorTextEditor.scrollLineIntoView(selection.anchor.line);
    return;
  }

  codemirror.setSelection(selection.anchor, selection.head, {scroll: false});
  codemirror._codeMirrorTextEditor.scrollLineIntoView(selection.anchor.line);
};

/**
 * @param {!CodeMirror} codemirror
 * @return {!Object|undefined}
 */
CodeMirror.commands.goSmartPageUp = function(codemirror) {
  if (codemirror._codeMirrorTextEditor.selection().equal(TextUtils.TextRange.createFromLocation(0, 0)))
    return CodeMirror.Pass;
  codemirror.execCommand('goPageUp');
};

/**
 * @param {!CodeMirror} codemirror
 * @return {!Object|undefined}
 */
CodeMirror.commands.goSmartPageDown = function(codemirror) {
  if (codemirror._codeMirrorTextEditor.selection().equal(codemirror._codeMirrorTextEditor.fullRange().collapseToEnd()))
    return CodeMirror.Pass;
  codemirror.execCommand('goPageDown');
};

TextEditor.CodeMirrorTextEditor.LongLineModeLineLengthThreshold = 2000;
TextEditor.CodeMirrorTextEditor.MaxEditableTextSize = 1024 * 1024 * 10;

/**
 * @implements {TextEditor.TextEditorPositionHandle}
 * @unrestricted
 */
TextEditor.CodeMirrorPositionHandle = class {
  /**
   * @param {!CodeMirror} codeMirror
   * @param {!CodeMirror.Pos} pos
   */
  constructor(codeMirror, pos) {
    this._codeMirror = codeMirror;
    this._lineHandle = codeMirror.getLineHandle(pos.line);
    this._columnNumber = pos.ch;
  }

  /**
   * @override
   * @return {?{lineNumber: number, columnNumber: number}}
   */
  resolve() {
    const lineNumber = this._lineHandle ? this._codeMirror.getLineNumber(this._lineHandle) : null;
    if (typeof lineNumber !== 'number')
      return null;
    return {lineNumber: lineNumber, columnNumber: this._columnNumber};
  }

  /**
   * @override
   * @param {!TextEditor.TextEditorPositionHandle} positionHandle
   * @return {boolean}
   */
  equal(positionHandle) {
    return positionHandle._lineHandle === this._lineHandle && positionHandle._columnNumber === this._columnNumber &&
        positionHandle._codeMirror === this._codeMirror;
  }
};

/**
 * @unrestricted
 */
TextEditor.CodeMirrorTextEditor.SelectNextOccurrenceController = class {
  /**
   * @param {!TextEditor.CodeMirrorTextEditor} textEditor
   * @param {!CodeMirror} codeMirror
   */
  constructor(textEditor, codeMirror) {
    this._textEditor = textEditor;
    this._codeMirror = codeMirror;
  }

  selectionWillChange() {
    if (!this._muteSelectionListener)
      delete this._fullWordSelection;
  }

  /**
   * @param {!Array.<!TextUtils.TextRange>} selections
   * @param {!TextUtils.TextRange} range
   * @return {boolean}
   */
  _findRange(selections, range) {
    for (let i = 0; i < selections.length; ++i) {
      if (range.equal(selections[i]))
        return true;
    }
    return false;
  }

  undoLastSelection() {
    this._muteSelectionListener = true;
    this._codeMirror.execCommand('undoSelection');
    this._muteSelectionListener = false;
  }

  selectNextOccurrence() {
    const selections = this._textEditor.selections();
    let anyEmptySelection = false;
    for (let i = 0; i < selections.length; ++i) {
      const selection = selections[i];
      anyEmptySelection = anyEmptySelection || selection.isEmpty();
      if (selection.startLine !== selection.endLine)
        return;
    }
    if (anyEmptySelection) {
      this._expandSelectionsToWords(selections);
      return;
    }

    const last = selections[selections.length - 1];
    let next = last;
    do
      next = this._findNextOccurrence(next, !!this._fullWordSelection);
    while (next && this._findRange(selections, next) && !next.equal(last));

    if (!next)
      return;
    selections.push(next);

    this._muteSelectionListener = true;
    this._textEditor.setSelections(selections, selections.length - 1);
    delete this._muteSelectionListener;

    this._textEditor.scrollLineIntoView(next.startLine);
  }

  /**
   * @param {!Array.<!TextUtils.TextRange>} selections
   */
  _expandSelectionsToWords(selections) {
    const newSelections = [];
    for (let i = 0; i < selections.length; ++i) {
      const selection = selections[i];
      const startRangeWord = this._textEditor.wordRangeForCursorPosition(
                                 selection.startLine, selection.startColumn, TextUtils.TextUtils.isWordChar) ||
          TextUtils.TextRange.createFromLocation(selection.startLine, selection.startColumn);
      const endRangeWord = this._textEditor.wordRangeForCursorPosition(
                               selection.endLine, selection.endColumn, TextUtils.TextUtils.isWordChar) ||
          TextUtils.TextRange.createFromLocation(selection.endLine, selection.endColumn);
      const newSelection = new TextUtils.TextRange(
          startRangeWord.startLine, startRangeWord.startColumn, endRangeWord.endLine, endRangeWord.endColumn);
      newSelections.push(newSelection);
    }
    this._textEditor.setSelections(newSelections, newSelections.length - 1);
    this._fullWordSelection = true;
  }

  /**
   * @param {!TextUtils.TextRange} range
   * @param {boolean} fullWord
   * @return {?TextUtils.TextRange}
   */
  _findNextOccurrence(range, fullWord) {
    range = range.normalize();
    let matchedLineNumber;
    let matchedColumnNumber;
    const textToFind = this._textEditor.text(range);
    function findWordInLine(wordRegex, lineNumber, lineText, from, to) {
      if (typeof matchedLineNumber === 'number')
        return true;
      wordRegex.lastIndex = from;
      const result = wordRegex.exec(lineText);
      if (!result || result.index + textToFind.length > to)
        return false;
      matchedLineNumber = lineNumber;
      matchedColumnNumber = result.index;
      return true;
    }

    let iteratedLineNumber;
    function lineIterator(regex, lineHandle) {
      if (findWordInLine(regex, iteratedLineNumber++, lineHandle.text, 0, lineHandle.text.length))
        return true;
    }

    let regexSource = textToFind.escapeForRegExp();
    if (fullWord)
      regexSource = '\\b' + regexSource + '\\b';
    const wordRegex = new RegExp(regexSource, 'g');
    const currentLineText = this._codeMirror.getLine(range.startLine);

    findWordInLine(wordRegex, range.startLine, currentLineText, range.endColumn, currentLineText.length);
    iteratedLineNumber = range.startLine + 1;
    this._codeMirror.eachLine(range.startLine + 1, this._codeMirror.lineCount(), lineIterator.bind(null, wordRegex));
    iteratedLineNumber = 0;
    this._codeMirror.eachLine(0, range.startLine, lineIterator.bind(null, wordRegex));
    findWordInLine(wordRegex, range.startLine, currentLineText, 0, range.startColumn);

    if (typeof matchedLineNumber !== 'number')
      return null;
    return new TextUtils.TextRange(
        matchedLineNumber, matchedColumnNumber, matchedLineNumber, matchedColumnNumber + textToFind.length);
  }
};


/**
 * @interface
 */
TextEditor.TextEditorPositionHandle = function() {};

TextEditor.TextEditorPositionHandle.prototype = {
  /**
   * @return {?{lineNumber: number, columnNumber: number}}
   */
  resolve() {},

  /**
   * @param {!TextEditor.TextEditorPositionHandle} positionHandle
   * @return {boolean}
   */
  equal(positionHandle) {}
};

TextEditor.CodeMirrorTextEditor._overrideModeWithPrefixedTokens('css', 'css-');
TextEditor.CodeMirrorTextEditor._overrideModeWithPrefixedTokens('javascript', 'js-');
TextEditor.CodeMirrorTextEditor._overrideModeWithPrefixedTokens('xml', 'xml-');

/** @type {!Set<!Runtime.Extension>} */
TextEditor.CodeMirrorTextEditor._loadedMimeModeExtensions = new Set();


/**
 * @interface
 */
TextEditor.CodeMirrorMimeMode = function() {};

TextEditor.CodeMirrorMimeMode.prototype = {
  /**
   * @param {!Runtime.Extension} extension
   */
  install(extension) {}
};

/**
 * @unrestricted
 */
TextEditor.TextEditorBookMark = class {
  /**
   * @param {!CodeMirror.TextMarker} marker
   * @param {symbol} type
   * @param {!TextEditor.CodeMirrorTextEditor} editor
   */
  constructor(marker, type, editor) {
    marker[TextEditor.TextEditorBookMark._symbol] = this;

    this._marker = marker;
    this._type = type;
    this._editor = editor;
  }

  clear() {
    const position = this._marker.find();
    this._marker.clear();
    if (position)
      this._editor._updateDecorations(position.line);
  }

  refresh() {
    this._marker.changed();
    const position = this._marker.find();
    if (position)
      this._editor._updateDecorations(position.line);
  }

  /**
   * @return {symbol}
   */
  type() {
    return this._type;
  }

  /**
   * @return {?TextUtils.TextRange}
   */
  position() {
    const pos = this._marker.find();
    return pos ? TextUtils.TextRange.createFromLocation(pos.line, pos.ch) : null;
  }
};

TextEditor.TextEditorBookMark._symbol = Symbol('TextEditor.TextEditorBookMark');

/**
 * @typedef {{
 *  element: !Element,
 *  widget: !CodeMirror.LineWidget,
 *  update: ?function()
 * }}
 */
TextEditor.CodeMirrorTextEditor.Decoration;

/**
 * @implements {UI.TextEditorFactory}
 * @unrestricted
 */
TextEditor.CodeMirrorTextEditorFactory = class {
  /**
   * @override
   * @param {!UI.TextEditor.Options} options
   * @return {!TextEditor.CodeMirrorTextEditor}
   */
  createEditor(options) {
    return new TextEditor.CodeMirrorTextEditor(options);
  }
};

// CodeMirror uses an offscreen <textarea> to detect input. Due to inconsistencies in the many browsers it supports,
// it simplifies things by regularly checking if something is in the textarea, adding those characters to the document,
// and then clearing the textarea. This breaks assistive technology that wants to read from CodeMirror, because the
// <textarea> that they interact with is constantly empty.
// Because we target up-to-date Chrome, we can gaurantee consistent input events. This lets us leave the current
// line from the editor in our <textarea>. CodeMirror still expects a mostly empty <textarea>, so we pass CodeMirror a
// fake <textarea> that only contains the users input.
CodeMirror.inputStyles.devToolsAccessibleTextArea = class extends CodeMirror.inputStyles.textarea {
  /**
   * @override
   * @param {!Object} display
   */
  init(display) {
    super.init(display);
    UI.ARIAUtils.setAccessibleName(this.textarea, ls`Code editor`);
    this.textarea.addEventListener('compositionstart', this._onCompositionStart.bind(this));
  }

  _onCompositionStart() {
    if (this.textarea.selectionEnd === this.textarea.value.length)
      return;
    // CodeMirror always expects the caret to be at the end of the textarea
    // When in IME composition mode, clip the textarea to how CodeMirror expects it,
    // and then let CodeMirror do it's thing.
    this.textarea.value = this.textarea.value.substring(0, this.textarea.selectionEnd);
    this.textarea.setSelectionRange(this.textarea.value.length, this.textarea.value.length);
    this.prevInput = this.textarea.value;
  }

  /**
   * @override
   * @param {boolean=} typing
   */
  reset(typing) {
    if (typing || this.contextMenuPending || this.composing || this.cm.somethingSelected()) {
      super.reset(typing);
      return;
    }

    // When navigating around the document, keep the current visual line in the textarea.
    const cursor = this.cm.getCursor();
    let start, end;
    if (this.cm.options.lineWrapping) {
      // To get the visual line, compute the leftmost and rightmost character positions.
      const top = this.cm.charCoords(cursor, 'page').top;
      start = this.cm.coordsChar({left: -Infinity, top});
      end = this.cm.coordsChar({left: Infinity, top});
    } else {
      // Limit the line to 1000 characters to prevent lag.
      const offset = Math.floor(cursor.ch / 1000) * 1000;
      start = {ch: offset, line: cursor.line};
      end = {ch: offset + 1000, line: cursor.line};
    }
    this.textarea.value = this.cm.getRange(start, end);
    const caretPosition = cursor.ch - start.ch;
    this.textarea.setSelectionRange(caretPosition, caretPosition);
    this.prevInput = this.textarea.value;
  }

  /**
   * @override
   * @return {boolean}
   */
  poll() {
    if (this.contextMenuPending || this.composing)
      return super.poll();
    const text = this.textarea.value;
    let start = 0;
    const length = Math.min(this.prevInput.length, text.length);
    while (start < length && this.prevInput[start] === text[start])
      ++start;
    let end = 0;
    while (end < length - start && this.prevInput[this.prevInput.length - end - 1] === text[text.length - end - 1])
      ++end;

    // CodeMirror expects the user to be typing into a blank <textarea>.
    // Pass a fake textarea into super.poll that only contains the users input.
    /** @type {!HTMLTextAreaElement} */
    const placeholder = this.textarea;
    this.textarea = /** @type {!HTMLTextAreaElement} */ (createElement('textarea'));
    this.textarea.value = text.substring(start, text.length - end);
    this.textarea.setSelectionRange(placeholder.selectionStart - start, placeholder.selectionEnd - start);
    this.prevInput = '';
    const result = super.poll();
    this.prevInput = text;
    this.textarea = placeholder;
    return result;
  }
};
