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

Sources.DebuggerPlugin = class extends Sources.UISourceCodeFrame.Plugin {
  /**
   * @param {!SourceFrame.SourcesTextEditor} textEditor
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @param {!SourceFrame.Transformer} transformer
   */
  constructor(textEditor, uiSourceCode, transformer) {
    super();
    this._textEditor = textEditor;
    this._uiSourceCode = uiSourceCode;
    this._transformer = transformer;

    /** @type {?Workspace.UILocation} */
    this._executionLocation = null;
    this._controlDown = false;
    this._asyncStepInHoveredLine = 0;
    this._asyncStepInHovered = false;
    /** @type {?number} */
    this._clearValueWidgetsTimer = null;
    /** @type {?UI.Infobar} */
    this._sourceMapInfobar = null;
    /** @type {?number} */
    this._controlTimeout = null;

    this._scriptsPanel = Sources.SourcesPanel.instance();
    this._breakpointManager = Bindings.breakpointManager;
    if (uiSourceCode.project().type() === Workspace.projectTypes.Debugger)
      this._textEditor.element.classList.add('source-frame-debugger-script');

    this._popoverHelper = new UI.PopoverHelper(this._scriptsPanel.element, this._getPopoverRequest.bind(this));
    this._popoverHelper.setDisableOnClick(true);
    this._popoverHelper.setTimeout(250, 250);
    this._popoverHelper.setHasPadding(true);
    this._boundPopoverHelperHide = this._popoverHelper.hidePopover.bind(this._popoverHelper);
    this._scriptsPanel.element.addEventListener('scroll', this._boundPopoverHelperHide, true);

    this._boundKeyDown = /** @type {function(!Event)} */ (this._onKeyDown.bind(this));
    this._textEditor.element.addEventListener('keydown', this._boundKeyDown, true);
    this._boundKeyUp = /** @type {function(!Event)} */ (this._onKeyUp.bind(this));
    this._textEditor.element.addEventListener('keyup', this._boundKeyUp, true);
    this._boundMouseMove = /** @type {function(!Event)} */ (this._onMouseMove.bind(this));
    this._textEditor.element.addEventListener('mousemove', this._boundMouseMove, false);
    this._boundMouseDown = /** @type {function(!Event)} */ (this._onMouseDown.bind(this));
    this._textEditor.element.addEventListener('mousedown', this._boundMouseDown, true);
    this._boundBlur = this._onBlur.bind(this);
    this._textEditor.element.addEventListener('focusout', this._boundBlur, false);
    this._boundWheel = event => {
      if (this._executionLocation && UI.KeyboardShortcut.eventHasCtrlOrMeta(event))
        event.preventDefault();
    };
    this._textEditor.element.addEventListener('wheel', this._boundWheel, true);

    this._textEditor.addEventListener(SourceFrame.SourcesTextEditor.Events.GutterClick, this._handleGutterClick, this);

    this._breakpointManager.addEventListener(
        Bindings.BreakpointManager.Events.BreakpointAdded, this._breakpointAdded, this);
    this._breakpointManager.addEventListener(
        Bindings.BreakpointManager.Events.BreakpointRemoved, this._breakpointRemoved, this);

    this._uiSourceCode.addEventListener(
        Workspace.UISourceCode.Events.WorkingCopyChanged, this._workingCopyChanged, this);
    this._uiSourceCode.addEventListener(
        Workspace.UISourceCode.Events.WorkingCopyCommitted, this._workingCopyCommitted, this);

    /** @type {!Set<!Sources.DebuggerPlugin.BreakpointDecoration>} */
    this._breakpointDecorations = new Set();
    /** @type {!Map<!Bindings.BreakpointManager.Breakpoint, !Sources.DebuggerPlugin.BreakpointDecoration>} */
    this._decorationByBreakpoint = new Map();
    /** @type {!Set<number>} */
    this._possibleBreakpointsRequested = new Set();

    /** @type {!Map.<!SDK.DebuggerModel, !Bindings.ResourceScriptFile>}*/
    this._scriptFileForDebuggerModel = new Map();

    Common.moduleSetting('skipStackFramesPattern').addChangeListener(this._showBlackboxInfobarIfNeeded, this);
    Common.moduleSetting('skipContentScripts').addChangeListener(this._showBlackboxInfobarIfNeeded, this);

    /** @type {!Map.<number, !Element>} */
    this._valueWidgets = new Map();
    /** @type {?Map<!Object, !Function>} */
    this._continueToLocationDecorations = null;

    UI.context.addFlavorChangeListener(SDK.DebuggerModel.CallFrame, this._callFrameChanged, this);
    this._liveLocationPool = new Bindings.LiveLocationPool();
    this._callFrameChanged();

    this._updateScriptFiles();

    if (this._uiSourceCode.isDirty()) {
      this._muted = true;
      this._mutedFromStart = true;
    } else {
      this._muted = false;
      this._mutedFromStart = false;
      this._initializeBreakpoints();
    }

    /** @type {?UI.Infobar} */
    this._blackboxInfobar = null;
    this._showBlackboxInfobarIfNeeded();

    const scriptFiles = this._scriptFileForDebuggerModel.valuesArray();
    for (let i = 0; i < scriptFiles.length; ++i)
      scriptFiles[i].checkMapping();

    this._hasLineWithoutMapping = false;
    this._updateLinesWithoutMappingHighlight();
    if (!Runtime.experiments.isEnabled('sourcesPrettyPrint')) {
      /** @type {?UI.Infobar} */
      this._prettyPrintInfobar = null;
      this._detectMinified();
    }
  }

  /**
   * @override
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @return {boolean}
   */
  static accepts(uiSourceCode) {
    return uiSourceCode.contentType().hasScripts();
  }

  /**
   * @override
   * @return {!Array<!UI.ToolbarItem>}
   */
  rightToolbarItems() {
    const originURL = Bindings.CompilerScriptMapping.uiSourceCodeOrigin(this._uiSourceCode);
    if (originURL) {
      const parsedURL = originURL.asParsedURL();
      if (parsedURL)
        return [new UI.ToolbarText(Common.UIString('(source mapped from %s)', parsedURL.displayName))];
    }

    return [];
  }

  _showBlackboxInfobarIfNeeded() {
    const uiSourceCode = this._uiSourceCode;
    if (!uiSourceCode.contentType().hasScripts())
      return;
    const projectType = uiSourceCode.project().type();
    if (!Bindings.blackboxManager.isBlackboxedUISourceCode(uiSourceCode)) {
      this._hideBlackboxInfobar();
      return;
    }

    if (this._blackboxInfobar)
      this._blackboxInfobar.dispose();

    const infobar = new UI.Infobar(UI.Infobar.Type.Warning, Common.UIString('This script is blackboxed in debugger'));
    this._blackboxInfobar = infobar;

    infobar.createDetailsRowMessage(
        Common.UIString('Debugger will skip stepping through this script, and will not stop on exceptions'));

    const scriptFile = this._scriptFileForDebuggerModel.size ? this._scriptFileForDebuggerModel.valuesArray()[0] : null;
    if (scriptFile && scriptFile.hasSourceMapURL())
      infobar.createDetailsRowMessage(Common.UIString('Source map found, but ignored for blackboxed file.'));
    infobar.createDetailsRowMessage();
    infobar.createDetailsRowMessage(Common.UIString('Possible ways to cancel this behavior are:'));

    infobar.createDetailsRowMessage(' - ').createTextChild(
        Common.UIString('Go to "%s" tab in settings', Common.UIString('Blackboxing')));
    const unblackboxLink = infobar.createDetailsRowMessage(' - ').createChild('span', 'link');
    unblackboxLink.textContent = Common.UIString('Unblackbox this script');
    unblackboxLink.addEventListener('click', unblackbox, false);

    function unblackbox() {
      Bindings.blackboxManager.unblackboxUISourceCode(uiSourceCode);
      if (projectType === Workspace.projectTypes.ContentScripts)
        Bindings.blackboxManager.unblackboxContentScripts();
    }
    this._textEditor.attachInfobar(this._blackboxInfobar);
  }

  _hideBlackboxInfobar() {
    if (!this._blackboxInfobar)
      return;
    this._blackboxInfobar.dispose();
    this._blackboxInfobar = null;
  }

  /**
   * @override
   */
  wasShown() {
    if (this._executionLocation) {
      // We need SourcesTextEditor to be initialized prior to this call. @see crbug.com/499889
      setImmediate(() => {
        this._generateValuesInSource();
      });
    }
  }

  /**
   * @override
   */
  willHide() {
    this._popoverHelper.hidePopover();
  }

  /**
   * @override
   * @param {!UI.ContextMenu} contextMenu
   * @param {number} editorLineNumber
   * @return {!Promise}
   */
  populateLineGutterContextMenu(contextMenu, editorLineNumber) {
    /**
     * @this {Sources.DebuggerPlugin}
     */
    function populate(resolve, reject) {
      const uiLocation = new Workspace.UILocation(this._uiSourceCode, editorLineNumber, 0);
      this._scriptsPanel.appendUILocationItems(contextMenu, uiLocation);
      const breakpoints = this._lineBreakpointDecorations(editorLineNumber)
                              .map(decoration => decoration.breakpoint)
                              .filter(breakpoint => !!breakpoint);
      if (!breakpoints.length) {
        contextMenu.debugSection().appendItem(
            Common.UIString('Add breakpoint'), this._createNewBreakpoint.bind(this, editorLineNumber, '', true));
        contextMenu.debugSection().appendItem(
            Common.UIString('Add conditional breakpoint\u2026'),
            this._editBreakpointCondition.bind(this, editorLineNumber, null, null));
        contextMenu.debugSection().appendItem(
            Common.UIString('Never pause here'), this._createNewBreakpoint.bind(this, editorLineNumber, 'false', true));
      } else {
        const hasOneBreakpoint = breakpoints.length === 1;
        const removeTitle =
            hasOneBreakpoint ? Common.UIString('Remove breakpoint') : Common.UIString('Remove all breakpoints in line');
        contextMenu.debugSection().appendItem(removeTitle, () => breakpoints.map(breakpoint => breakpoint.remove()));
        if (hasOneBreakpoint) {
          contextMenu.debugSection().appendItem(
              Common.UIString('Edit breakpoint\u2026'),
              this._editBreakpointCondition.bind(this, editorLineNumber, breakpoints[0], null));
        }
        const hasEnabled = breakpoints.some(breakpoint => breakpoint.enabled());
        if (hasEnabled) {
          const title = hasOneBreakpoint ? Common.UIString('Disable breakpoint') :
                                           Common.UIString('Disable all breakpoints in line');
          contextMenu.debugSection().appendItem(
              title, () => breakpoints.map(breakpoint => breakpoint.setEnabled(false)));
        }
        const hasDisabled = breakpoints.some(breakpoint => !breakpoint.enabled());
        if (hasDisabled) {
          const title = hasOneBreakpoint ? Common.UIString('Enable breakpoint') :
                                           Common.UIString('Enabled all breakpoints in line');
          contextMenu.debugSection().appendItem(
              title, () => breakpoints.map(breakpoint => breakpoint.setEnabled(true)));
        }
      }
      resolve();
    }
    return new Promise(populate.bind(this));
  }

  /**
   * @override
   * @param {!UI.ContextMenu} contextMenu
   * @param {number} editorLineNumber
   * @param {number} editorColumnNumber
   * @return {!Promise}
   */
  populateTextAreaContextMenu(contextMenu, editorLineNumber, editorColumnNumber) {
    /**
     * @param {!Bindings.ResourceScriptFile} scriptFile
     */
    function addSourceMapURL(scriptFile) {
      Sources.AddSourceMapURLDialog.show(addSourceMapURLDialogCallback.bind(null, scriptFile));
    }

    /**
     * @param {!Bindings.ResourceScriptFile} scriptFile
     * @param {string} url
     */
    function addSourceMapURLDialogCallback(scriptFile, url) {
      if (!url)
        return;
      scriptFile.addSourceMapURL(url);
    }

    /**
     * @this {Sources.DebuggerPlugin}
     */
    function populateSourceMapMembers() {
      if (this._uiSourceCode.project().type() === Workspace.projectTypes.Network &&
          Common.moduleSetting('jsSourceMapsEnabled').get() &&
          !Bindings.blackboxManager.isBlackboxedUISourceCode(this._uiSourceCode)) {
        if (this._scriptFileForDebuggerModel.size) {
          const scriptFile = this._scriptFileForDebuggerModel.valuesArray()[0];
          const addSourceMapURLLabel = Common.UIString('Add source map\u2026');
          contextMenu.debugSection().appendItem(addSourceMapURLLabel, addSourceMapURL.bind(null, scriptFile));
        }
      }
    }

    return super.populateTextAreaContextMenu(contextMenu, editorLineNumber, editorColumnNumber)
        .then(populateSourceMapMembers.bind(this));
  }

  _workingCopyChanged() {
    if (this._scriptFileForDebuggerModel.size)
      return;

    if (this._uiSourceCode.isDirty())
      this._muteBreakpointsWhileEditing();
    else
      this._restoreBreakpointsAfterEditing();
  }

  /**
   * @param {!Common.Event} event
   */
  _workingCopyCommitted(event) {
    this._scriptsPanel.updateLastModificationTime();
    if (!this._scriptFileForDebuggerModel.size)
      this._restoreBreakpointsAfterEditing();
  }

  _didMergeToVM() {
    this._restoreBreakpointsIfConsistentScripts();
  }

  _didDivergeFromVM() {
    this._muteBreakpointsWhileEditing();
  }

  _muteBreakpointsWhileEditing() {
    if (this._muted)
      return;
    for (const decoration of this._breakpointDecorations)
      this._updateBreakpointDecoration(decoration);
    this._muted = true;
  }

  _restoreBreakpointsIfConsistentScripts() {
    const scriptFiles = this._scriptFileForDebuggerModel.valuesArray();
    for (let i = 0; i < scriptFiles.length; ++i) {
      if (scriptFiles[i].hasDivergedFromVM() || scriptFiles[i].isMergingToVM())
        return;
    }

    this._restoreBreakpointsAfterEditing();
  }

  _restoreBreakpointsAfterEditing() {
    this._muted = false;
    if (this._mutedFromStart) {
      this._mutedFromStart = false;
      this._initializeBreakpoints();
      return;
    }
    const decorations = Array.from(this._breakpointDecorations);
    this._breakpointDecorations.clear();
    this._textEditor.operation(() => decorations.map(decoration => decoration.hide()));
    for (const decoration of decorations) {
      if (!decoration.breakpoint)
        continue;
      const enabled = decoration.enabled;
      decoration.breakpoint.remove();
      const location = decoration.handle.resolve();
      if (location)
        this._setBreakpoint(location.lineNumber, location.columnNumber, decoration.condition, enabled);
    }
  }

  /**
   * @param {string}  tokenType
   * @return {boolean}
   */
  _isIdentifier(tokenType) {
    return tokenType.startsWith('js-variable') || tokenType.startsWith('js-property') || tokenType === 'js-def';
  }

  /**
   * @param {!MouseEvent} event
   * @return {?UI.PopoverRequest}
   */
  _getPopoverRequest(event) {
    if (UI.KeyboardShortcut.eventHasCtrlOrMeta(event))
      return null;
    const target = UI.context.flavor(SDK.Target);
    const debuggerModel = target ? target.model(SDK.DebuggerModel) : null;
    if (!debuggerModel || !debuggerModel.isPaused())
      return null;

    const textPosition = this._textEditor.coordinatesToCursorPosition(event.x, event.y);
    if (!textPosition)
      return null;

    const mouseLine = textPosition.startLine;
    const mouseColumn = textPosition.startColumn;
    const textSelection = this._textEditor.selection().normalize();
    let anchorBox;
    let editorLineNumber;
    let startHighlight;
    let endHighlight;

    if (textSelection && !textSelection.isEmpty()) {
      if (textSelection.startLine !== textSelection.endLine || textSelection.startLine !== mouseLine ||
          mouseColumn < textSelection.startColumn || mouseColumn > textSelection.endColumn)
        return null;

      const leftCorner =
          this._textEditor.cursorPositionToCoordinates(textSelection.startLine, textSelection.startColumn);
      const rightCorner = this._textEditor.cursorPositionToCoordinates(textSelection.endLine, textSelection.endColumn);
      anchorBox = new AnchorBox(leftCorner.x, leftCorner.y, rightCorner.x - leftCorner.x, leftCorner.height);
      editorLineNumber = textSelection.startLine;
      startHighlight = textSelection.startColumn;
      endHighlight = textSelection.endColumn - 1;
    } else {
      const token = this._textEditor.tokenAtTextPosition(textPosition.startLine, textPosition.startColumn);
      if (!token || !token.type)
        return null;
      editorLineNumber = textPosition.startLine;
      const line = this._textEditor.line(editorLineNumber);
      const tokenContent = line.substring(token.startColumn, token.endColumn);

      const isIdentifier = this._isIdentifier(token.type);
      if (!isIdentifier && (token.type !== 'js-keyword' || tokenContent !== 'this'))
        return null;

      const leftCorner = this._textEditor.cursorPositionToCoordinates(editorLineNumber, token.startColumn);
      const rightCorner = this._textEditor.cursorPositionToCoordinates(editorLineNumber, token.endColumn - 1);
      anchorBox = new AnchorBox(leftCorner.x, leftCorner.y, rightCorner.x - leftCorner.x, leftCorner.height);

      startHighlight = token.startColumn;
      endHighlight = token.endColumn - 1;
      while (startHighlight > 1 && line.charAt(startHighlight - 1) === '.') {
        const tokenBefore = this._textEditor.tokenAtTextPosition(editorLineNumber, startHighlight - 2);
        if (!tokenBefore || !tokenBefore.type)
          return null;
        if (tokenBefore.type === 'js-meta')
          break;
        startHighlight = tokenBefore.startColumn;
      }
    }

    let objectPopoverHelper;
    let highlightDescriptor;

    return {
      box: anchorBox,
      show: async popover => {
        const selectedCallFrame = UI.context.flavor(SDK.DebuggerModel.CallFrame);
        if (!selectedCallFrame)
          return false;
        const evaluationText = this._textEditor.line(editorLineNumber).substring(startHighlight, endHighlight + 1);
        const resolvedText = await Sources.SourceMapNamesResolver.resolveExpression(
            /** @type {!SDK.DebuggerModel.CallFrame} */ (selectedCallFrame), evaluationText, this._uiSourceCode,
            editorLineNumber, startHighlight, endHighlight);
        const result = await selectedCallFrame.evaluate({
          expression: resolvedText || evaluationText,
          objectGroup: 'popover',
          includeCommandLineAPI: false,
          silent: true,
          returnByValue: false,
          generatePreview: false
        });
        if (!result.object)
          return false;
        objectPopoverHelper = await ObjectUI.ObjectPopoverHelper.buildObjectPopover(result.object, popover);
        const potentiallyUpdatedCallFrame = UI.context.flavor(SDK.DebuggerModel.CallFrame);
        if (!objectPopoverHelper || selectedCallFrame !== potentiallyUpdatedCallFrame) {
          debuggerModel.runtimeModel().releaseObjectGroup('popover');
          if (objectPopoverHelper)
            objectPopoverHelper.dispose();
          return false;
        }
        const highlightRange =
            new TextUtils.TextRange(editorLineNumber, startHighlight, editorLineNumber, endHighlight);
        highlightDescriptor = this._textEditor.highlightRange(highlightRange, 'source-frame-eval-expression');
        return true;
      },
      hide: () => {
        objectPopoverHelper.dispose();
        debuggerModel.runtimeModel().releaseObjectGroup('popover');
        this._textEditor.removeHighlight(highlightDescriptor);
      }
    };
  }

  /**
   * @param {!KeyboardEvent} event
   */
  _onKeyDown(event) {
    this._clearControlDown();

    if (event.key === 'Escape') {
      if (this._popoverHelper.isPopoverVisible()) {
        this._popoverHelper.hidePopover();
        event.consume();
      }
      return;
    }

    if (UI.shortcutRegistry.eventMatchesAction(event, 'debugger.toggle-breakpoint')) {
      const selection = this._textEditor.selection();
      if (!selection)
        return;
      this._toggleBreakpoint(selection.startLine, false);
      event.consume(true);
      return;
    }
    if (UI.shortcutRegistry.eventMatchesAction(event, 'debugger.toggle-breakpoint-enabled')) {
      const selection = this._textEditor.selection();
      if (!selection)
        return;
      this._toggleBreakpoint(selection.startLine, true);
      event.consume(true);
      return;
    }

    if (UI.KeyboardShortcut.eventHasCtrlOrMeta(event) && this._executionLocation) {
      this._controlDown = true;
      if (event.key === (Host.isMac() ? 'Meta' : 'Control')) {
        this._controlTimeout = setTimeout(() => {
          if (this._executionLocation && this._controlDown)
            this._showContinueToLocations();
        }, 150);
      }
    }
  }

  /**
   * @param {!MouseEvent} event
   */
  _onMouseMove(event) {
    if (this._executionLocation && this._controlDown && UI.KeyboardShortcut.eventHasCtrlOrMeta(event)) {
      if (!this._continueToLocationDecorations)
        this._showContinueToLocations();
    }
    if (this._continueToLocationDecorations) {
      const textPosition = this._textEditor.coordinatesToCursorPosition(event.x, event.y);
      const hovering = !!event.target.enclosingNodeOrSelfWithClass('source-frame-async-step-in');
      this._setAsyncStepInHoveredLine(textPosition ? textPosition.startLine : null, hovering);
    }
  }

  /**
   * @param {?number} editorLineNumber
   * @param {boolean} hovered
   */
  _setAsyncStepInHoveredLine(editorLineNumber, hovered) {
    if (this._asyncStepInHoveredLine === editorLineNumber && this._asyncStepInHovered === hovered)
      return;
    if (this._asyncStepInHovered && this._asyncStepInHoveredLine)
      this._textEditor.toggleLineClass(this._asyncStepInHoveredLine, 'source-frame-async-step-in-hovered', false);
    this._asyncStepInHoveredLine = editorLineNumber;
    this._asyncStepInHovered = hovered;
    if (this._asyncStepInHovered && this._asyncStepInHoveredLine)
      this._textEditor.toggleLineClass(this._asyncStepInHoveredLine, 'source-frame-async-step-in-hovered', true);
  }

  /**
   * @param {!MouseEvent} event
   */
  _onMouseDown(event) {
    if (!this._executionLocation || !UI.KeyboardShortcut.eventHasCtrlOrMeta(event))
      return;
    if (!this._continueToLocationDecorations)
      return;
    event.consume();
    const textPosition = this._textEditor.coordinatesToCursorPosition(event.x, event.y);
    if (!textPosition)
      return;
    for (const decoration of this._continueToLocationDecorations.keys()) {
      const range = decoration.find();
      if (range.from.line !== textPosition.startLine || range.to.line !== textPosition.startLine)
        continue;
      if (range.from.ch <= textPosition.startColumn && textPosition.startColumn <= range.to.ch) {
        this._continueToLocationDecorations.get(decoration)();
        break;
      }
    }
  }

  /**
   * @param {!Event} event
   */
  _onBlur(event) {
    if (this._textEditor.element.isAncestor(/** @type {!Node} */ (event.target)))
      return;
    this._clearControlDown();
  }

  /**
   * @param {!KeyboardEvent} event
   */
  _onKeyUp(event) {
    this._clearControlDown();
  }

  _clearControlDown() {
    this._controlDown = false;
    this._clearContinueToLocations();
    clearTimeout(this._controlTimeout);
  }

  /**
   * @param {number} editorLineNumber
   * @param {?Bindings.BreakpointManager.Breakpoint} breakpoint
   * @param {?{lineNumber: number, columnNumber: number}} location
   */
  async _editBreakpointCondition(editorLineNumber, breakpoint, location) {
    const conditionElement = createElementWithClass('div', 'source-frame-breakpoint-condition');

    const labelElement = conditionElement.createChild('label', 'source-frame-breakpoint-message');
    labelElement.htmlFor = 'source-frame-breakpoint-condition';
    labelElement.createTextChild(
        Common.UIString('The breakpoint on line %d will stop only if this expression is true:', editorLineNumber + 1));


    this._textEditor.addDecoration(conditionElement, editorLineNumber);

    /** @type {!UI.TextEditorFactory} */
    const factory = await self.runtime.extension(UI.TextEditorFactory).instance();
    const editor =
        factory.createEditor({lineNumbers: false, lineWrapping: true, mimeType: 'javascript', autoHeight: true});
    editor.widget().show(conditionElement);
    if (breakpoint)
      editor.setText(breakpoint.condition());
    editor.setSelection(editor.fullRange());
    editor.configureAutocomplete(ObjectUI.JavaScriptAutocompleteConfig.createConfigForEditor(editor));
    editor.widget().element.addEventListener('keydown', async event => {
      if (isEnterKey(event) && !event.shiftKey) {
        event.consume(true);
        const expression = editor.text();
        if (event.ctrlKey || await ObjectUI.JavaScriptAutocomplete.isExpressionComplete(expression))
          finishEditing.call(this, true);
        else
          editor.newlineAndIndent();
      }
      if (isEscKey(event))
        finishEditing.call(this, false);
    }, true);
    editor.widget().focus();
    editor.widget().element.id = 'source-frame-breakpoint-condition';
    editor.widget().element.addEventListener('blur', event => {
      if (event.relatedTarget && !event.relatedTarget.isSelfOrDescendant(editor.widget().element))
        finishEditing.call(this, true);
    }, true);
    let finished = false;
    /**
     * @this {Sources.DebuggerPlugin}
     */
    function finishEditing(committed) {
      if (finished)
        return;
      finished = true;
      editor.widget().detach();
      this._textEditor.removeDecoration(/** @type {!Element} */ (conditionElement), editorLineNumber);
      if (!committed)
        return;

      if (breakpoint)
        breakpoint.setCondition(editor.text().trim());
      else if (location)
        this._setBreakpoint(location.lineNumber, location.columnNumber, editor.text().trim(), true);
      else
        this._createNewBreakpoint(editorLineNumber, editor.text().trim(), true);
    }
  }

  /**
   * @param {!Bindings.LiveLocation} liveLocation
   */
  _executionLineChanged(liveLocation) {
    this._clearExecutionLine();
    const uiLocation = liveLocation.uiLocation();
    if (!uiLocation || uiLocation.uiSourceCode !== this._uiSourceCode) {
      this._executionLocation = null;
      return;
    }

    this._executionLocation = uiLocation;
    const editorLocation = this._transformer.rawToEditorLocation(uiLocation.lineNumber, uiLocation.columnNumber);
    this._textEditor.setExecutionLocation(editorLocation[0], editorLocation[1]);
    if (this._textEditor.isShowing()) {
      // We need SourcesTextEditor to be initialized prior to this call. @see crbug.com/506566
      setImmediate(() => {
        if (this._controlDown)
          this._showContinueToLocations();
        else
          this._generateValuesInSource();
      });
    }
  }

  _generateValuesInSource() {
    if (!Common.moduleSetting('inlineVariableValues').get())
      return;
    const executionContext = UI.context.flavor(SDK.ExecutionContext);
    if (!executionContext)
      return;
    const callFrame = UI.context.flavor(SDK.DebuggerModel.CallFrame);
    if (!callFrame)
      return;

    const localScope = callFrame.localScope();
    const functionLocation = callFrame.functionLocation();
    if (localScope && functionLocation) {
      Sources.SourceMapNamesResolver.resolveScopeInObject(localScope)
          .getAllProperties(false, false)
          .then(this._prepareScopeVariables.bind(this, callFrame));
    }
  }

  _showContinueToLocations() {
    this._popoverHelper.hidePopover();
    const executionContext = UI.context.flavor(SDK.ExecutionContext);
    if (!executionContext)
      return;
    const callFrame = UI.context.flavor(SDK.DebuggerModel.CallFrame);
    if (!callFrame)
      return;
    const start = callFrame.functionLocation() || callFrame.location();
    const debuggerModel = callFrame.debuggerModel;
    debuggerModel.getPossibleBreakpoints(start, null, true)
        .then(locations => this._textEditor.operation(renderLocations.bind(this, locations)));

    /**
     * @param {!Array<!SDK.DebuggerModel.BreakLocation>} locations
     * @this {Sources.DebuggerPlugin}
     */
    function renderLocations(locations) {
      this._clearContinueToLocationsNoRestore();
      this._textEditor.hideExecutionLineBackground();
      this._clearValueWidgets();
      this._continueToLocationDecorations = new Map();
      locations = locations.reverse();
      let previousCallLine = -1;
      for (const location of locations) {
        const editorLocation = this._transformer.rawToEditorLocation(location.lineNumber, location.columnNumber);
        let token = this._textEditor.tokenAtTextPosition(editorLocation[0], editorLocation[1]);
        if (!token)
          continue;
        const line = this._textEditor.line(editorLocation[0]);
        let tokenContent = line.substring(token.startColumn, token.endColumn);
        if (!token.type && tokenContent === '.') {
          token = this._textEditor.tokenAtTextPosition(editorLocation[0], token.endColumn + 1);
          tokenContent = line.substring(token.startColumn, token.endColumn);
        }
        if (!token.type)
          continue;
        const validKeyword = token.type === 'js-keyword' &&
            (tokenContent === 'this' || tokenContent === 'return' || tokenContent === 'new' ||
             tokenContent === 'continue' || tokenContent === 'break');
        if (!validKeyword && !this._isIdentifier(token.type))
          continue;
        if (previousCallLine === editorLocation[0] && location.type !== Protocol.Debugger.BreakLocationType.Call)
          continue;

        let highlightRange =
            new TextUtils.TextRange(editorLocation[0], token.startColumn, editorLocation[0], token.endColumn - 1);
        let decoration = this._textEditor.highlightRange(highlightRange, 'source-frame-continue-to-location');
        this._continueToLocationDecorations.set(decoration, location.continueToLocation.bind(location));
        if (location.type === Protocol.Debugger.BreakLocationType.Call)
          previousCallLine = editorLocation[0];

        let isAsyncCall = (line[token.startColumn - 1] === '.' && tokenContent === 'then') ||
            tokenContent === 'setTimeout' || tokenContent === 'setInterval' || tokenContent === 'postMessage';
        if (tokenContent === 'new') {
          token = this._textEditor.tokenAtTextPosition(editorLocation[0], token.endColumn + 1);
          tokenContent = line.substring(token.startColumn, token.endColumn);
          isAsyncCall = tokenContent === 'Worker';
        }
        const isCurrentPosition = this._executionLocation &&
            location.lineNumber === this._executionLocation.lineNumber &&
            location.columnNumber === this._executionLocation.columnNumber;
        if (location.type === Protocol.Debugger.BreakLocationType.Call && isAsyncCall) {
          const asyncStepInRange =
              this._findAsyncStepInRange(this._textEditor, editorLocation[0], line, token.endColumn);
          if (asyncStepInRange) {
            highlightRange = new TextUtils.TextRange(
                editorLocation[0], asyncStepInRange.from, editorLocation[0], asyncStepInRange.to - 1);
            decoration = this._textEditor.highlightRange(highlightRange, 'source-frame-async-step-in');
            this._continueToLocationDecorations.set(
                decoration, this._asyncStepIn.bind(this, location, !!isCurrentPosition));
          }
        }
      }

      this._continueToLocationRenderedForTest();
    }
  }

  _continueToLocationRenderedForTest() {
  }

  /**
   * @param {!SourceFrame.SourcesTextEditor} textEditor
   * @param {number} editorLineNumber
   * @param {string} line
   * @param {number} column
   * @return {?{from: number, to: number}}
   */
  _findAsyncStepInRange(textEditor, editorLineNumber, line, column) {
    let token;
    let tokenText;
    let from = column;
    let to = line.length;

    let position = line.indexOf('(', column);
    const argumentsStart = position;
    if (position === -1)
      return null;
    position++;

    skipWhitespace();
    if (position >= line.length)
      return null;

    nextToken();
    if (!token)
      return null;
    from = token.startColumn;

    if (token.type === 'js-keyword' && tokenText === 'async') {
      skipWhitespace();
      if (position >= line.length)
        return {from: from, to: to};
      nextToken();
      if (!token)
        return {from: from, to: to};
    }

    if (token.type === 'js-keyword' && tokenText === 'function')
      return {from: from, to: to};

    if (token.type === 'js-string')
      return {from: argumentsStart, to: to};

    if (token.type && this._isIdentifier(token.type))
      return {from: from, to: to};

    if (tokenText !== '(')
      return null;
    const closeParen = line.indexOf(')', position);
    if (closeParen === -1 || line.substring(position, closeParen).indexOf('(') !== -1)
      return {from: from, to: to};
    return {from: from, to: closeParen + 1};

    function nextToken() {
      token = textEditor.tokenAtTextPosition(editorLineNumber, position);
      if (token) {
        position = token.endColumn;
        to = token.endColumn;
        tokenText = line.substring(token.startColumn, token.endColumn);
      }
    }

    function skipWhitespace() {
      while (position < line.length) {
        if (line[position] === ' ') {
          position++;
          continue;
        }
        const token = textEditor.tokenAtTextPosition(editorLineNumber, position);
        if (token.type === 'js-comment') {
          position = token.endColumn;
          continue;
        }
        break;
      }
    }
  }

  /**
   * @param {!SDK.DebuggerModel.BreakLocation} location
   * @param {boolean} isCurrentPosition
   */
  _asyncStepIn(location, isCurrentPosition) {
    if (!isCurrentPosition)
      location.continueToLocation(asyncStepIn);
    else
      asyncStepIn();

    function asyncStepIn() {
      location.debuggerModel.scheduleStepIntoAsync();
    }
  }

  /**
   * @param {!SDK.DebuggerModel.CallFrame} callFrame
   * @param {!SDK.GetPropertiesResult} allProperties
   */
  _prepareScopeVariables(callFrame, allProperties) {
    const properties = allProperties.properties;
    this._clearValueWidgets();
    if (!properties || !properties.length || properties.length > 500 || !this._textEditor.isShowing())
      return;

    const functionUILocation = Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(
        /** @type {!SDK.DebuggerModel.Location} */ (callFrame.functionLocation()));
    const executionUILocation = Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(callFrame.location());
    if (!functionUILocation || !executionUILocation || functionUILocation.uiSourceCode !== this._uiSourceCode ||
        executionUILocation.uiSourceCode !== this._uiSourceCode)
      return;

    const functionEditorLocation =
        this._transformer.rawToEditorLocation(functionUILocation.lineNumber, functionUILocation.columnNumber);
    const executionEditorLocation =
        this._transformer.rawToEditorLocation(executionUILocation.lineNumber, executionUILocation.columnNumber);
    const fromLine = functionEditorLocation[0];
    const fromColumn = functionEditorLocation[1];
    const toLine = executionEditorLocation[0];
    if (fromLine >= toLine || toLine - fromLine > 500 || fromLine < 0 || toLine >= this._textEditor.linesCount)
      return;

    const valuesMap = new Map();
    for (const property of properties)
      valuesMap.set(property.name, property.value);

    /** @type {!Map.<number, !Set<string>>} */
    const namesPerLine = new Map();
    let skipObjectProperty = false;
    const tokenizer = new TextEditor.CodeMirrorUtils.TokenizerFactory().createTokenizer('text/javascript');
    tokenizer(this._textEditor.line(fromLine).substring(fromColumn), processToken.bind(this, fromLine));
    for (let i = fromLine + 1; i < toLine; ++i)
      tokenizer(this._textEditor.line(i), processToken.bind(this, i));

    /**
     * @param {number} editorLineNumber
     * @param {string} tokenValue
     * @param {?string} tokenType
     * @param {number} column
     * @param {number} newColumn
     * @this {Sources.DebuggerPlugin}
     */
    function processToken(editorLineNumber, tokenValue, tokenType, column, newColumn) {
      if (!skipObjectProperty && tokenType && this._isIdentifier(tokenType) && valuesMap.get(tokenValue)) {
        let names = namesPerLine.get(editorLineNumber);
        if (!names) {
          names = new Set();
          namesPerLine.set(editorLineNumber, names);
        }
        names.add(tokenValue);
      }
      skipObjectProperty = tokenValue === '.';
    }
    this._textEditor.operation(this._renderDecorations.bind(this, valuesMap, namesPerLine, fromLine, toLine));
  }

  /**
   * @param {!Map.<string,!SDK.RemoteObject>} valuesMap
   * @param {!Map.<number, !Set<string>>} namesPerLine
   * @param {number} fromLine
   * @param {number} toLine
   */
  _renderDecorations(valuesMap, namesPerLine, fromLine, toLine) {
    const formatter = new ObjectUI.RemoteObjectPreviewFormatter();
    for (let i = fromLine; i < toLine; ++i) {
      const names = namesPerLine.get(i);
      const oldWidget = this._valueWidgets.get(i);
      if (!names) {
        if (oldWidget) {
          this._valueWidgets.delete(i);
          this._textEditor.removeDecoration(oldWidget, i);
        }
        continue;
      }

      const widget = createElementWithClass('div', 'text-editor-value-decoration');
      const base = this._textEditor.cursorPositionToCoordinates(i, 0);
      const offset = this._textEditor.cursorPositionToCoordinates(i, this._textEditor.line(i).length);
      const codeMirrorLinesLeftPadding = 4;
      const left = offset.x - base.x + codeMirrorLinesLeftPadding;
      widget.style.left = left + 'px';
      widget.__nameToToken = new Map();

      let renderedNameCount = 0;
      for (const name of names) {
        if (renderedNameCount > 10)
          break;
        if (namesPerLine.get(i - 1) && namesPerLine.get(i - 1).has(name))
          continue;  // Only render name once in the given continuous block.
        if (renderedNameCount)
          widget.createTextChild(', ');
        const nameValuePair = widget.createChild('span');
        widget.__nameToToken.set(name, nameValuePair);
        nameValuePair.createTextChild(name + ' = ');
        const value = valuesMap.get(name);
        const propertyCount = value.preview ? value.preview.properties.length : 0;
        const entryCount = value.preview && value.preview.entries ? value.preview.entries.length : 0;
        if (value.preview && propertyCount + entryCount < 10) {
          formatter.appendObjectPreview(nameValuePair, value.preview, false /* isEntry */);
        } else {
          nameValuePair.appendChild(ObjectUI.ObjectPropertiesSection.createValueElement(
              value, false /* wasThrown */, false /* showPreview */));
        }
        ++renderedNameCount;
      }

      let widgetChanged = true;
      if (oldWidget) {
        widgetChanged = false;
        for (const name of widget.__nameToToken.keys()) {
          const oldText = oldWidget.__nameToToken.get(name) ? oldWidget.__nameToToken.get(name).textContent : '';
          const newText = widget.__nameToToken.get(name) ? widget.__nameToToken.get(name).textContent : '';
          if (newText !== oldText) {
            widgetChanged = true;
            // value has changed, update it.
            UI.runCSSAnimationOnce(
                /** @type {!Element} */ (widget.__nameToToken.get(name)), 'source-frame-value-update-highlight');
          }
        }
        if (widgetChanged) {
          this._valueWidgets.delete(i);
          this._textEditor.removeDecoration(oldWidget, i);
        }
      }
      if (widgetChanged) {
        this._valueWidgets.set(i, widget);
        this._textEditor.addDecoration(widget, i);
      }
    }
  }

  _clearExecutionLine() {
    this._textEditor.operation(() => {
      if (this._executionLocation)
        this._textEditor.clearExecutionLine();
      this._executionLocation = null;
      if (this._clearValueWidgetsTimer) {
        clearTimeout(this._clearValueWidgetsTimer);
        this._clearValueWidgetsTimer = null;
      }
      this._clearValueWidgetsTimer = setTimeout(this._clearValueWidgets.bind(this), 1000);
      this._clearContinueToLocationsNoRestore();
    });
  }

  _clearValueWidgets() {
    clearTimeout(this._clearValueWidgetsTimer);
    this._clearValueWidgetsTimer = null;
    this._textEditor.operation(() => {
      for (const line of this._valueWidgets.keys())
        this._textEditor.removeDecoration(this._valueWidgets.get(line), line);
      this._valueWidgets.clear();
    });
  }

  _clearContinueToLocationsNoRestore() {
    if (!this._continueToLocationDecorations)
      return;
    this._textEditor.operation(() => {
      for (const decoration of this._continueToLocationDecorations.keys())
        this._textEditor.removeHighlight(decoration);
      this._continueToLocationDecorations = null;
      this._setAsyncStepInHoveredLine(null, false);
    });
  }

  _clearContinueToLocations() {
    if (!this._continueToLocationDecorations)
      return;
    this._textEditor.operation(() => {
      this._textEditor.showExecutionLineBackground();
      this._generateValuesInSource();
      this._clearContinueToLocationsNoRestore();
    });
  }

  /**
   * @param {number} lineNumber
   * @return {!Array<!Sources.DebuggerPlugin.BreakpointDecoration>}
   */
  _lineBreakpointDecorations(lineNumber) {
    return Array.from(this._breakpointDecorations)
        .filter(decoration => (decoration.handle.resolve() || {}).lineNumber === lineNumber);
  }

  /**
   * @param {number} editorLineNumber
   * @param {number} editorColumnNumber
   * @return {?Sources.DebuggerPlugin.BreakpointDecoration}
   */
  _breakpointDecoration(editorLineNumber, editorColumnNumber) {
    for (const decoration of this._breakpointDecorations) {
      const location = decoration.handle.resolve();
      if (!location)
        continue;
      if (location.lineNumber === editorLineNumber && location.columnNumber === editorColumnNumber)
        return decoration;
    }
    return null;
  }

  /**
   * @param {!Sources.DebuggerPlugin.BreakpointDecoration} decoration
   */
  _updateBreakpointDecoration(decoration) {
    if (!this._scheduledBreakpointDecorationUpdates) {
      /** @type {?Set<!Sources.DebuggerPlugin.BreakpointDecoration>} */
      this._scheduledBreakpointDecorationUpdates = new Set();
      setImmediate(() => this._textEditor.operation(update.bind(this)));
    }
    this._scheduledBreakpointDecorationUpdates.add(decoration);

    /**
     * @this {Sources.DebuggerPlugin}
     */
    function update() {
      if (!this._scheduledBreakpointDecorationUpdates)
        return;
      const editorLineNumbers = new Set();
      for (const decoration of this._scheduledBreakpointDecorationUpdates) {
        const location = decoration.handle.resolve();
        if (!location)
          continue;
        editorLineNumbers.add(location.lineNumber);
      }
      this._scheduledBreakpointDecorationUpdates = null;
      let waitingForInlineDecorations = false;
      for (const lineNumber of editorLineNumbers) {
        const decorations = this._lineBreakpointDecorations(lineNumber);
        updateGutter.call(this, lineNumber, decorations);
        if (this._possibleBreakpointsRequested.has(lineNumber)) {
          waitingForInlineDecorations = true;
          continue;
        }
        updateInlineDecorations.call(this, lineNumber, decorations);
      }
      if (!waitingForInlineDecorations)
        this._breakpointDecorationsUpdatedForTest();
    }

    /**
     * @param {number} editorLineNumber
     * @param {!Array<!Sources.DebuggerPlugin.BreakpointDecoration>} decorations
     * @this {Sources.DebuggerPlugin}
     */
    function updateGutter(editorLineNumber, decorations) {
      this._textEditor.toggleLineClass(editorLineNumber, 'cm-breakpoint', false);
      this._textEditor.toggleLineClass(editorLineNumber, 'cm-breakpoint-disabled', false);
      this._textEditor.toggleLineClass(editorLineNumber, 'cm-breakpoint-conditional', false);

      if (decorations.length) {
        decorations.sort(Sources.DebuggerPlugin.BreakpointDecoration.mostSpecificFirst);
        this._textEditor.toggleLineClass(editorLineNumber, 'cm-breakpoint', true);
        this._textEditor.toggleLineClass(
            editorLineNumber, 'cm-breakpoint-disabled', !decorations[0].enabled || this._muted);
        this._textEditor.toggleLineClass(editorLineNumber, 'cm-breakpoint-conditional', !!decorations[0].condition);
      }
    }

    /**
     * @param {number} editorLineNumber
     * @param {!Array<!Sources.DebuggerPlugin.BreakpointDecoration>} decorations
     * @this {Sources.DebuggerPlugin}
     */
    function updateInlineDecorations(editorLineNumber, decorations) {
      const actualBookmarks =
          new Set(decorations.map(decoration => decoration.bookmark).filter(bookmark => !!bookmark));
      const lineEnd = this._textEditor.line(editorLineNumber).length;
      const bookmarks = this._textEditor.bookmarks(
          new TextUtils.TextRange(editorLineNumber, 0, editorLineNumber, lineEnd),
          Sources.DebuggerPlugin.BreakpointDecoration.bookmarkSymbol);
      for (const bookmark of bookmarks) {
        if (!actualBookmarks.has(bookmark))
          bookmark.clear();
      }
      if (!decorations.length)
        return;
      if (decorations.length > 1) {
        for (const decoration of decorations) {
          decoration.update();
          if (!this._muted)
            decoration.show();
          else
            decoration.hide();
        }
      } else {
        decorations[0].update();
        decorations[0].hide();
      }
    }
  }

  _breakpointDecorationsUpdatedForTest() {
  }

  /**
   * @param {!Sources.DebuggerPlugin.BreakpointDecoration} decoration
   * @param {!Event} event
   */
  _inlineBreakpointClick(decoration, event) {
    event.consume(true);
    if (decoration.breakpoint) {
      if (event.shiftKey)
        decoration.breakpoint.setEnabled(!decoration.breakpoint.enabled());
      else
        decoration.breakpoint.remove();
    } else {
      const editorLocation = decoration.handle.resolve();
      if (!editorLocation)
        return;
      const location = this._transformer.editorToRawLocation(editorLocation.lineNumber, editorLocation.columnNumber);
      this._setBreakpoint(location[0], location[1], decoration.condition, true);
    }
  }

  /**
   * @param {!Sources.DebuggerPlugin.BreakpointDecoration} decoration
   * @param {!Event} event
   */
  _inlineBreakpointContextMenu(decoration, event) {
    event.consume(true);
    const editorLocation = decoration.handle.resolve();
    if (!editorLocation)
      return;
    const location = this._transformer.editorToRawLocation(editorLocation[0], editorLocation[1]);
    const contextMenu = new UI.ContextMenu(event);
    if (decoration.breakpoint) {
      contextMenu.debugSection().appendItem(
          Common.UIString('Edit breakpoint\u2026'),
          this._editBreakpointCondition.bind(this, editorLocation.lineNumber, decoration.breakpoint, null));
    } else {
      contextMenu.debugSection().appendItem(
          Common.UIString('Add conditional breakpoint\u2026'),
          this._editBreakpointCondition.bind(this, editorLocation.lineNumber, null, editorLocation));
      contextMenu.debugSection().appendItem(
          Common.UIString('Never pause here'), this._setBreakpoint.bind(this, location[0], location[1], 'false', true));
    }
    contextMenu.show();
  }

  /**
   * @param {!Common.Event} event
   * @return {boolean}
   */
  _shouldIgnoreExternalBreakpointEvents(event) {
    const uiLocation = /** @type {!Workspace.UILocation} */ (event.data.uiLocation);
    if (uiLocation.uiSourceCode !== this._uiSourceCode)
      return true;
    if (this._muted)
      return true;
    const scriptFiles = this._scriptFileForDebuggerModel.valuesArray();
    for (let i = 0; i < scriptFiles.length; ++i) {
      if (scriptFiles[i].isDivergingFromVM() || scriptFiles[i].isMergingToVM())
        return true;
    }
    return false;
  }

  /**
   * @param {!Common.Event} event
   */
  _breakpointAdded(event) {
    if (this._shouldIgnoreExternalBreakpointEvents(event))
      return;
    const uiLocation = /** @type {!Workspace.UILocation} */ (event.data.uiLocation);
    const breakpoint = /** @type {!Bindings.BreakpointManager.Breakpoint} */ (event.data.breakpoint);
    this._addBreakpoint(uiLocation, breakpoint);
  }

  /**
   * @param {!Workspace.UILocation} uiLocation
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   */
  _addBreakpoint(uiLocation, breakpoint) {
    const editorLocation = this._transformer.rawToEditorLocation(uiLocation.lineNumber, uiLocation.columnNumber);
    const lineDecorations = this._lineBreakpointDecorations(uiLocation.lineNumber);
    let decoration = this._breakpointDecoration(editorLocation[0], editorLocation[1]);
    if (decoration) {
      decoration.breakpoint = breakpoint;
      decoration.condition = breakpoint.condition();
      decoration.enabled = breakpoint.enabled();
    } else {
      const handle = this._textEditor.textEditorPositionHandle(editorLocation[0], editorLocation[1]);
      decoration = new Sources.DebuggerPlugin.BreakpointDecoration(
          this._textEditor, handle, breakpoint.condition(), breakpoint.enabled(), breakpoint);
      decoration.element.addEventListener('click', this._inlineBreakpointClick.bind(this, decoration), true);
      decoration.element.addEventListener(
          'contextmenu', this._inlineBreakpointContextMenu.bind(this, decoration), true);
      this._breakpointDecorations.add(decoration);
    }
    this._decorationByBreakpoint.set(breakpoint, decoration);
    this._updateBreakpointDecoration(decoration);
    if (breakpoint.enabled() && !lineDecorations.length) {
      this._possibleBreakpointsRequested.add(editorLocation[0]);
      const start = this._transformer.editorToRawLocation(editorLocation[0], 0);
      const end = this._transformer.editorToRawLocation(editorLocation[0] + 1, 0);
      this._breakpointManager
          .possibleBreakpoints(this._uiSourceCode, new TextUtils.TextRange(start[0], start[1], end[0], end[1]))
          .then(addInlineDecorations.bind(this, editorLocation[0]));
    }

    /**
     * @this {Sources.DebuggerPlugin}
     * @param {number} editorLineNumber
     * @param {!Array<!Workspace.UILocation>} possibleLocations
     */
    function addInlineDecorations(editorLineNumber, possibleLocations) {
      this._possibleBreakpointsRequested.delete(editorLineNumber);
      const decorations = this._lineBreakpointDecorations(editorLineNumber);
      for (const decoration of decorations)
        this._updateBreakpointDecoration(decoration);
      if (!decorations.some(decoration => !!decoration.breakpoint))
        return;
      /** @type {!Set<number>} */
      const columns = new Set();
      for (const decoration of decorations) {
        const editorLocation = decoration.handle.resolve();
        if (!editorLocation)
          continue;
        columns.add(editorLocation.columnNumber);
      }
      for (const location of possibleLocations) {
        const editorLocation = this._transformer.rawToEditorLocation(location.lineNumber, location.columnNumber);
        if (columns.has(editorLocation[1]))
          continue;
        const handle = this._textEditor.textEditorPositionHandle(editorLocation[0], editorLocation[1]);
        const decoration = new Sources.DebuggerPlugin.BreakpointDecoration(this._textEditor, handle, '', false, null);
        decoration.element.addEventListener('click', this._inlineBreakpointClick.bind(this, decoration), true);
        decoration.element.addEventListener(
            'contextmenu', this._inlineBreakpointContextMenu.bind(this, decoration), true);
        this._breakpointDecorations.add(decoration);
        this._updateBreakpointDecoration(decoration);
      }
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _breakpointRemoved(event) {
    if (this._shouldIgnoreExternalBreakpointEvents(event))
      return;
    const uiLocation = /** @type {!Workspace.UILocation} */ (event.data.uiLocation);
    const breakpoint = /** @type {!Bindings.BreakpointManager.Breakpoint} */ (event.data.breakpoint);
    const decoration = this._decorationByBreakpoint.get(breakpoint);
    if (!decoration)
      return;
    this._decorationByBreakpoint.delete(breakpoint);

    const editorLocation = this._transformer.rawToEditorLocation(uiLocation.lineNumber, uiLocation.columnNumber);
    decoration.breakpoint = null;
    decoration.enabled = false;

    const lineDecorations = this._lineBreakpointDecorations(editorLocation[0]);
    if (!lineDecorations.some(decoration => !!decoration.breakpoint)) {
      for (const lineDecoration of lineDecorations) {
        this._breakpointDecorations.delete(lineDecoration);
        this._updateBreakpointDecoration(lineDecoration);
      }
    } else {
      this._updateBreakpointDecoration(decoration);
    }
  }

  _initializeBreakpoints() {
    const breakpointLocations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
    for (const breakpointLocation of breakpointLocations)
      this._addBreakpoint(breakpointLocation.uiLocation, breakpointLocation.breakpoint);
  }

  _updateLinesWithoutMappingHighlight() {
    const isSourceMapSource = !!Bindings.CompilerScriptMapping.uiSourceCodeOrigin(this._uiSourceCode);
    if (!isSourceMapSource)
      return;
    const linesCount = this._textEditor.linesCount;
    for (let i = 0; i < linesCount; ++i) {
      const lineHasMapping = Bindings.CompilerScriptMapping.uiLineHasMapping(this._uiSourceCode, i);
      if (!lineHasMapping)
        this._hasLineWithoutMapping = true;
      if (this._hasLineWithoutMapping)
        this._textEditor.toggleLineClass(i, 'cm-line-without-source-mapping', !lineHasMapping);
    }
  }

  _updateScriptFiles() {
    for (const debuggerModel of SDK.targetManager.models(SDK.DebuggerModel)) {
      const scriptFile = Bindings.debuggerWorkspaceBinding.scriptFile(this._uiSourceCode, debuggerModel);
      if (scriptFile)
        this._updateScriptFile(debuggerModel);
    }
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  _updateScriptFile(debuggerModel) {
    const oldScriptFile = this._scriptFileForDebuggerModel.get(debuggerModel);
    const newScriptFile = Bindings.debuggerWorkspaceBinding.scriptFile(this._uiSourceCode, debuggerModel);
    this._scriptFileForDebuggerModel.delete(debuggerModel);
    if (oldScriptFile) {
      oldScriptFile.removeEventListener(Bindings.ResourceScriptFile.Events.DidMergeToVM, this._didMergeToVM, this);
      oldScriptFile.removeEventListener(
          Bindings.ResourceScriptFile.Events.DidDivergeFromVM, this._didDivergeFromVM, this);
      if (this._muted && !this._uiSourceCode.isDirty())
        this._restoreBreakpointsIfConsistentScripts();
    }
    if (!newScriptFile)
      return;
    this._scriptFileForDebuggerModel.set(debuggerModel, newScriptFile);
    newScriptFile.addEventListener(Bindings.ResourceScriptFile.Events.DidMergeToVM, this._didMergeToVM, this);
    newScriptFile.addEventListener(Bindings.ResourceScriptFile.Events.DidDivergeFromVM, this._didDivergeFromVM, this);
    newScriptFile.checkMapping();
    if (newScriptFile.hasSourceMapURL())
      this._showSourceMapInfobar();
  }

  _showSourceMapInfobar() {
    if (this._sourceMapInfobar)
      return;
    this._sourceMapInfobar = UI.Infobar.create(
        UI.Infobar.Type.Info, Common.UIString('Source Map detected.'),
        Common.settings.createSetting('sourceMapInfobarDisabled', false));
    if (!this._sourceMapInfobar)
      return;
    this._sourceMapInfobar.createDetailsRowMessage(Common.UIString(
        'Associated files should be added to the file tree. You can debug these resolved source files as regular JavaScript files.'));
    this._sourceMapInfobar.createDetailsRowMessage(Common.UIString(
        'Associated files are available via file tree or %s.',
        UI.shortcutRegistry.shortcutTitleForAction('quickOpen.show')));
    this._sourceMapInfobar.setCloseCallback(() => this._sourceMapInfobar = null);
    this._textEditor.attachInfobar(this._sourceMapInfobar);
  }

  _detectMinified() {
    const content = this._uiSourceCode.content();
    if (!content || !TextUtils.isMinified(content))
      return;

    this._prettyPrintInfobar = UI.Infobar.create(
        UI.Infobar.Type.Info, Common.UIString('Pretty-print this minified file?'),
        Common.settings.createSetting('prettyPrintInfobarDisabled', false));
    if (!this._prettyPrintInfobar)
      return;

    this._prettyPrintInfobar.setCloseCallback(() => this._prettyPrintInfobar = null);
    const toolbar = new UI.Toolbar('');
    const button = new UI.ToolbarButton('', 'largeicon-pretty-print');
    toolbar.appendToolbarItem(button);
    toolbar.element.style.display = 'inline-block';
    toolbar.element.style.verticalAlign = 'middle';
    toolbar.element.style.marginBottom = '3px';
    toolbar.element.style.pointerEvents = 'none';
    const element = this._prettyPrintInfobar.createDetailsRowMessage();
    element.appendChild(UI.formatLocalized(
        'You can click the %s button on the bottom status bar, and continue debugging with the new formatted source.',
        [toolbar.element]));
    this._textEditor.attachInfobar(this._prettyPrintInfobar);
  }

  /**
   * @param {!Common.Event} event
   */
  _handleGutterClick(event) {
    if (this._muted)
      return;

    const eventData = /** @type {!SourceFrame.SourcesTextEditor.GutterClickEventData} */ (event.data);
    const editorLineNumber = eventData.lineNumber;
    const eventObject = eventData.event;

    if (eventObject.button !== 0 || eventObject.altKey || eventObject.ctrlKey || eventObject.metaKey)
      return;

    this._toggleBreakpoint(editorLineNumber, eventObject.shiftKey);
    eventObject.consume(true);
  }

  /**
   * @param {number} editorLineNumber
   * @param {boolean} onlyDisable
   */
  _toggleBreakpoint(editorLineNumber, onlyDisable) {
    const decorations = this._lineBreakpointDecorations(editorLineNumber);
    if (!decorations.length) {
      this._createNewBreakpoint(editorLineNumber, '', true);
      return;
    }
    const hasDisabled = this._textEditor.hasLineClass(editorLineNumber, 'cm-breakpoint-disabled');
    const breakpoints = decorations.map(decoration => decoration.breakpoint).filter(breakpoint => !!breakpoint);
    for (const breakpoint of breakpoints) {
      if (onlyDisable)
        breakpoint.setEnabled(hasDisabled);
      else
        breakpoint.remove();
    }
  }

  /**
   * @param {number} editorLineNumber
   * @param {string} condition
   * @param {boolean} enabled
   */
  async _createNewBreakpoint(editorLineNumber, condition, enabled) {
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.ScriptsBreakpointSet);
    if (editorLineNumber < this._textEditor.linesCount) {
      const lineLength = Math.min(this._textEditor.line(editorLineNumber).length, 1024);
      const start = this._transformer.editorToRawLocation(editorLineNumber, 0);
      const end = this._transformer.editorToRawLocation(editorLineNumber, lineLength);
      const locations = await this._breakpointManager.possibleBreakpoints(
          this._uiSourceCode, new TextUtils.TextRange(start[0], start[1], end[0], end[1]));
      if (locations && locations.length) {
        this._setBreakpoint(locations[0].lineNumber, locations[0].columnNumber, condition, enabled);
        return;
      }
    }
    const origin = this._transformer.editorToRawLocation(editorLineNumber, 0);
    await this._setBreakpoint(origin[0], origin[1], condition, enabled);
  }

  /**
   * @param {boolean} onlyDisable
   */
  toggleBreakpointOnCurrentLine(onlyDisable) {
    if (this._muted)
      return;

    const selection = this._textEditor.selection();
    if (!selection)
      return;
    this._toggleBreakpoint(selection.startLine, onlyDisable);
  }

  /**
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {string} condition
   * @param {boolean} enabled
   */
  _setBreakpoint(lineNumber, columnNumber, condition, enabled) {
    if (!Bindings.CompilerScriptMapping.uiLineHasMapping(this._uiSourceCode, lineNumber))
      return;

    Common.moduleSetting('breakpointsActive').set(true);
    this._breakpointManager.setBreakpoint(this._uiSourceCode, lineNumber, columnNumber, condition, enabled);
    this._breakpointWasSetForTest(lineNumber, columnNumber, condition, enabled);
  }

  /**
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {string} condition
   * @param {boolean} enabled
   */
  _breakpointWasSetForTest(lineNumber, columnNumber, condition, enabled) {
  }


  _callFrameChanged() {
    this._liveLocationPool.disposeAll();
    const callFrame = UI.context.flavor(SDK.DebuggerModel.CallFrame);
    if (!callFrame) {
      this._clearExecutionLine();
      return;
    }
    Bindings.debuggerWorkspaceBinding.createCallFrameLiveLocation(
        callFrame.location(), this._executionLineChanged.bind(this), this._liveLocationPool);
  }

  /**
   * @override
   */
  dispose() {
    for (const decoration of this._breakpointDecorations)
      decoration.dispose();
    this._breakpointDecorations.clear();
    if (this._scheduledBreakpointDecorationUpdates) {
      for (const decoration of this._scheduledBreakpointDecorationUpdates)
        decoration.dispose();
      this._scheduledBreakpointDecorationUpdates.clear();
    }

    this._hideBlackboxInfobar();
    if (this._sourceMapInfobar)
      this._sourceMapInfobar.dispose();
    if (this._prettyPrintInfobar)
      this._prettyPrintInfobar.dispose();
    this._scriptsPanel.element.removeEventListener('scroll', this._boundPopoverHelperHide, true);
    for (const script of this._scriptFileForDebuggerModel.values()) {
      script.removeEventListener(Bindings.ResourceScriptFile.Events.DidMergeToVM, this._didMergeToVM, this);
      script.removeEventListener(Bindings.ResourceScriptFile.Events.DidDivergeFromVM, this._didDivergeFromVM, this);
    }
    this._scriptFileForDebuggerModel.clear();


    this._textEditor.element.removeEventListener('keydown', this._boundKeyDown, true);
    this._textEditor.element.removeEventListener('keyup', this._boundKeyUp, true);
    this._textEditor.element.removeEventListener('mousemove', this._boundMouseMove, false);
    this._textEditor.element.removeEventListener('mousedown', this._boundMouseDown, true);
    this._textEditor.element.removeEventListener('focusout', this._boundBlur, false);
    this._textEditor.element.removeEventListener('wheel', this._boundWheel, true);

    this._textEditor.removeEventListener(
        SourceFrame.SourcesTextEditor.Events.GutterClick, this._handleGutterClick, this);
    this._popoverHelper.hidePopover();
    this._popoverHelper.dispose();

    this._breakpointManager.removeEventListener(
        Bindings.BreakpointManager.Events.BreakpointAdded, this._breakpointAdded, this);
    this._breakpointManager.removeEventListener(
        Bindings.BreakpointManager.Events.BreakpointRemoved, this._breakpointRemoved, this);
    this._uiSourceCode.removeEventListener(
        Workspace.UISourceCode.Events.WorkingCopyChanged, this._workingCopyChanged, this);
    this._uiSourceCode.removeEventListener(
        Workspace.UISourceCode.Events.WorkingCopyCommitted, this._workingCopyCommitted, this);

    Common.moduleSetting('skipStackFramesPattern').removeChangeListener(this._showBlackboxInfobarIfNeeded, this);
    Common.moduleSetting('skipContentScripts').removeChangeListener(this._showBlackboxInfobarIfNeeded, this);
    super.dispose();

    this._clearExecutionLine();
    UI.context.removeFlavorChangeListener(SDK.DebuggerModel.CallFrame, this._callFrameChanged, this);
    this._liveLocationPool.disposeAll();
  }
};

/**
 * @unrestricted
 */
Sources.DebuggerPlugin.BreakpointDecoration = class {
  /**
   * @param {!TextEditor.CodeMirrorTextEditor} textEditor
   * @param {!TextEditor.TextEditorPositionHandle} handle
   * @param {string} condition
   * @param {boolean} enabled
   * @param {?Bindings.BreakpointManager.Breakpoint} breakpoint
   */
  constructor(textEditor, handle, condition, enabled, breakpoint) {
    this._textEditor = textEditor;
    this.handle = handle;
    this.condition = condition;
    this.enabled = enabled;
    this.breakpoint = breakpoint;
    this.element = UI.Icon.create('smallicon-inline-breakpoint');
    this.element.classList.toggle('cm-inline-breakpoint', true);

    /** @type {?TextEditor.TextEditorBookMark} */
    this.bookmark = null;
  }

  /**
   * @param {!Sources.DebuggerPlugin.BreakpointDecoration} decoration1
   * @param {!Sources.DebuggerPlugin.BreakpointDecoration} decoration2
   * @return {number}
   */
  static mostSpecificFirst(decoration1, decoration2) {
    if (decoration1.enabled !== decoration2.enabled)
      return decoration1.enabled ? -1 : 1;
    if (!!decoration1.condition !== !!decoration2.condition)
      return !!decoration1.condition ? -1 : 1;
    return 0;
  }

  update() {
    if (!this.condition)
      this.element.setIconType('smallicon-inline-breakpoint');
    else
      this.element.setIconType('smallicon-inline-breakpoint-conditional');
    this.element.classList.toggle('cm-inline-disabled', !this.enabled);
  }

  show() {
    if (this.bookmark)
      return;
    const editorLocation = this.handle.resolve();
    if (!editorLocation)
      return;
    this.bookmark = this._textEditor.addBookmark(
        editorLocation.lineNumber, editorLocation.columnNumber, this.element,
        Sources.DebuggerPlugin.BreakpointDecoration.bookmarkSymbol);
    this.bookmark[Sources.DebuggerPlugin.BreakpointDecoration._elementSymbolForTest] = this.element;
  }

  hide() {
    if (!this.bookmark)
      return;
    this.bookmark.clear();
    this.bookmark = null;
  }

  dispose() {
    const location = this.handle.resolve();
    if (location) {
      this._textEditor.toggleLineClass(location.lineNumber, 'cm-breakpoint', false);
      this._textEditor.toggleLineClass(location.lineNumber, 'cm-breakpoint-disabled', false);
      this._textEditor.toggleLineClass(location.lineNumber, 'cm-breakpoint-conditional', false);
    }
    this.hide();
  }
};

Sources.DebuggerPlugin.BreakpointDecoration.bookmarkSymbol = Symbol('bookmark');
Sources.DebuggerPlugin.BreakpointDecoration._elementSymbolForTest = Symbol('element');

Sources.DebuggerPlugin.continueToLocationDecorationSymbol = Symbol('bookmark');
