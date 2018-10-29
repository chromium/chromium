/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
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
Main.Main = class {
  /**
   * @suppressGlobalPropertiesCheck
   */
  constructor() {
    Main.Main._instanceForTest = this;
    runOnWindowLoad(this._loaded.bind(this));
  }

  /**
   * @param {string} label
   */
  static time(label) {
    if (Host.isUnderTest())
      return;
    console.time(label);
  }

  /**
   * @param {string} label
   */
  static timeEnd(label) {
    if (Host.isUnderTest())
      return;
    console.timeEnd(label);
  }

  async _loaded() {
    console.timeStamp('Main._loaded');
    await Runtime.runtimeReady();
    Runtime.setPlatform(Host.platform());
    InspectorFrontendHost.getPreferences(this._gotPreferences.bind(this));
  }

  /**
   * @param {!Object<string, string>} prefs
   */
  _gotPreferences(prefs) {
    console.timeStamp('Main._gotPreferences');
    if (Host.isUnderTest(prefs))
      self.runtime.useTestBase();
    this._createSettings(prefs);
    this._createAppUI();
  }

  /**
   * @param {!Object<string, string>} prefs
   * Note: this function is called from testSettings in Tests.js.
   */
  _createSettings(prefs) {
    this._initializeExperiments();
    let storagePrefix = '';
    if (Host.isCustomDevtoolsFrontend())
      storagePrefix = '__custom__';
    else if (!Runtime.queryParam('can_dock') && !!Runtime.queryParam('debugFrontend') && !Host.isUnderTest())
      storagePrefix = '__bundled__';

    let localStorage;
    if (!Host.isUnderTest() && window.localStorage) {
      localStorage = new Common.SettingsStorage(
          window.localStorage, undefined, undefined, () => window.localStorage.clear(), storagePrefix);
    } else {
      localStorage = new Common.SettingsStorage({}, undefined, undefined, undefined, storagePrefix);
    }
    const globalStorage = new Common.SettingsStorage(
        prefs, InspectorFrontendHost.setPreference, InspectorFrontendHost.removePreference,
        InspectorFrontendHost.clearPreferences, storagePrefix);
    Common.settings = new Common.Settings(globalStorage, localStorage);
    if (!Host.isUnderTest())
      new Common.VersionController().updateVersion();
  }

  _initializeExperiments() {
    // Keep this sorted alphabetically: both keys and values.
    Runtime.experiments.register('applyCustomStylesheet', 'Allow custom UI themes');
    Runtime.experiments.register('blackboxJSFramesOnTimeline', 'Blackbox JavaScript frames on Timeline', true);
    Runtime.experiments.register('colorContrastRatio', 'Color contrast ratio line in color picker', true);
    Runtime.experiments.register('consoleBelowPrompt', 'Console eager evaluation');
    Runtime.experiments.register('consoleKeyboardNavigation', 'Console keyboard navigation', true);
    Runtime.experiments.register('emptySourceMapAutoStepping', 'Empty sourcemap auto-stepping');
    Runtime.experiments.register('inputEventsOnTimelineOverview', 'Input events on Timeline overview', true);
    Runtime.experiments.register('nativeHeapProfiler', 'Native memory sampling heap profiler', true);
    Runtime.experiments.register('networkSearch', 'Network search');
    Runtime.experiments.register('oopifInlineDOM', 'OOPIF: inline DOM ', true);
    Runtime.experiments.register('pinnedExpressions', 'Pinned expressions in Console', true);
    Runtime.experiments.register('protocolMonitor', 'Protocol Monitor');
    Runtime.experiments.register('samplingHeapProfilerTimeline', 'Sampling heap profiler timeline', true);
    Runtime.experiments.register('sourceDiff', 'Source diff');
    Runtime.experiments.register('sourcesPrettyPrint', 'Automatically pretty print in the Sources Panel');
    Runtime.experiments.register(
        'stepIntoAsync', 'Introduce separate step action, stepInto becomes powerful enough to go inside async call');
    Runtime.experiments.register('splitInDrawer', 'Split in drawer', true);
    Runtime.experiments.register('terminalInDrawer', 'Terminal in drawer', true);

    // Timeline
    Runtime.experiments.register('timelineEventInitiators', 'Timeline: event initiators');
    Runtime.experiments.register('timelineFlowEvents', 'Timeline: flow events', true);
    Runtime.experiments.register('timelineInvalidationTracking', 'Timeline: invalidation tracking', true);
    Runtime.experiments.register('timelinePaintTimingMarkers', 'Timeline: paint timing markers', true);
    Runtime.experiments.register('timelineShowAllEvents', 'Timeline: show all events', true);
    Runtime.experiments.register('timelineTracingJSProfile', 'Timeline: tracing based JS profiler', true);
    Runtime.experiments.register('timelineV8RuntimeCallStats', 'Timeline: V8 Runtime Call Stats on Timeline', true);
    Runtime.experiments.register('timelineWebGL', 'Timeline: WebGL-based flamechart');

    Runtime.experiments.cleanUpStaleExperiments();

    if (Host.isUnderTest()) {
      const testPath = Runtime.queryParam('test');
      // Enable experiments for testing.
      if (testPath.indexOf('oopif/') !== -1)
        Runtime.experiments.enableForTest('oopifInlineDOM');
      if (testPath.indexOf('network/') !== -1)
        Runtime.experiments.enableForTest('networkSearch');
      if (testPath.indexOf('console/viewport-testing/') !== -1)
        Runtime.experiments.enableForTest('consoleKeyboardNavigation');
      if (testPath.indexOf('console/') !== -1)
        Runtime.experiments.enableForTest('pinnedExpressions');
    }

    Runtime.experiments.setDefaultExperiments([
      'colorContrastRatio', 'stepIntoAsync', 'oopifInlineDOM', 'consoleBelowPrompt', 'timelineTracingJSProfile',
      'pinnedExpressions'
    ]);
  }

  /**
   * @suppressGlobalPropertiesCheck
   */
  async _createAppUI() {
    Main.Main.time('Main._createAppUI');

    UI.viewManager = new UI.ViewManager();

    // Request filesystems early, we won't create connections until callback is fired. Things will happen in parallel.
    Persistence.isolatedFileSystemManager = new Persistence.IsolatedFileSystemManager();

    const themeSetting = Common.settings.createSetting('uiTheme', 'default');
    UI.initializeUIUtils(document, themeSetting);
    themeSetting.addChangeListener(Components.reload.bind(Components));

    UI.installComponentRootStyles(/** @type {!Element} */ (document.body));

    this._addMainEventListeners(document);

    const canDock = !!Runtime.queryParam('can_dock');
    UI.zoomManager = new UI.ZoomManager(window, InspectorFrontendHost);
    UI.inspectorView = UI.InspectorView.instance();
    UI.ContextMenu.initialize();
    UI.ContextMenu.installHandler(document);
    UI.Tooltip.installHandler(document);
    Components.dockController = new Components.DockController(canDock);
    SDK.consoleModel = new SDK.ConsoleModel();
    SDK.multitargetNetworkManager = new SDK.MultitargetNetworkManager();
    SDK.domDebuggerManager = new SDK.DOMDebuggerManager();
    SDK.targetManager.addEventListener(
        SDK.TargetManager.Events.SuspendStateChanged, this._onSuspendStateChanged.bind(this));

    UI.shortcutsScreen = new UI.ShortcutsScreen();
    // set order of some sections explicitly
    UI.shortcutsScreen.section(Common.UIString('Elements Panel'));
    UI.shortcutsScreen.section(Common.UIString('Styles Pane'));
    UI.shortcutsScreen.section(Common.UIString('Debugger'));
    UI.shortcutsScreen.section(Common.UIString('Console'));

    Workspace.fileManager = new Workspace.FileManager();
    Workspace.workspace = new Workspace.Workspace();

    Bindings.networkProjectManager = new Bindings.NetworkProjectManager();
    Bindings.resourceMapping = new Bindings.ResourceMapping(SDK.targetManager, Workspace.workspace);
    new Bindings.PresentationConsoleMessageManager();
    Bindings.cssWorkspaceBinding = new Bindings.CSSWorkspaceBinding(SDK.targetManager, Workspace.workspace);
    Bindings.debuggerWorkspaceBinding = new Bindings.DebuggerWorkspaceBinding(SDK.targetManager, Workspace.workspace);
    Bindings.breakpointManager =
        new Bindings.BreakpointManager(Workspace.workspace, SDK.targetManager, Bindings.debuggerWorkspaceBinding);
    Extensions.extensionServer = new Extensions.ExtensionServer();

    new Persistence.FileSystemWorkspaceBinding(Persistence.isolatedFileSystemManager, Workspace.workspace);
    Persistence.persistence = new Persistence.Persistence(Workspace.workspace, Bindings.breakpointManager);
    Persistence.networkPersistenceManager = new Persistence.NetworkPersistenceManager(Workspace.workspace);

    new Main.ExecutionContextSelector(SDK.targetManager, UI.context);
    Bindings.blackboxManager = new Bindings.BlackboxManager(Bindings.debuggerWorkspaceBinding);

    new Main.Main.PauseListener();

    UI.actionRegistry = new UI.ActionRegistry();
    UI.shortcutRegistry = new UI.ShortcutRegistry(UI.actionRegistry, document);
    UI.ShortcutsScreen.registerShortcuts();
    this._registerForwardedShortcuts();
    this._registerMessageSinkListener();

    Main.Main.timeEnd('Main._createAppUI');
    this._showAppUI(await self.runtime.extension(Common.AppProvider).instance());
  }

  /**
   * @param {!Object} appProvider
   * @suppressGlobalPropertiesCheck
   */
  _showAppUI(appProvider) {
    Main.Main.time('Main._showAppUI');
    const app = /** @type {!Common.AppProvider} */ (appProvider).createApp();
    // It is important to kick controller lifetime after apps are instantiated.
    Components.dockController.initialize();
    app.presentUI(document);

    const toggleSearchNodeAction = UI.actionRegistry.action('elements.toggle-element-search');
    // TODO: we should not access actions from other modules.
    if (toggleSearchNodeAction) {
      InspectorFrontendHost.events.addEventListener(
          InspectorFrontendHostAPI.Events.EnterInspectElementMode,
          toggleSearchNodeAction.execute.bind(toggleSearchNodeAction), this);
    }
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.RevealSourceLine, this._revealSourceLine, this);

    UI.inspectorView.createToolbars();
    InspectorFrontendHost.loadCompleted();

    const extensions = self.runtime.extensions(Common.QueryParamHandler);
    for (const extension of extensions) {
      const value = Runtime.queryParam(extension.descriptor()['name']);
      if (value !== null)
        extension.instance().then(handleQueryParam.bind(null, value));
    }

    /**
     * @param {string} value
     * @param {!Common.QueryParamHandler} handler
     */
    function handleQueryParam(value, handler) {
      handler.handleQueryParam(value);
    }

    // Allow UI cycles to repaint prior to creating connection.
    setTimeout(this._initializeTarget.bind(this), 0);
    Main.Main.timeEnd('Main._showAppUI');
  }

  async _initializeTarget() {
    Main.Main.time('Main._initializeTarget');
    const instances =
        await Promise.all(self.runtime.extensions('early-initialization').map(extension => extension.instance()));
    for (const instance of instances)
      /** @type {!Common.Runnable} */ (instance).run();
    // Used for browser tests.
    InspectorFrontendHost.readyForTest();
    // Asynchronously run the extensions.
    setTimeout(this._lateInitialization.bind(this), 100);
    Main.Main.timeEnd('Main._initializeTarget');
  }

  _lateInitialization() {
    Main.Main.time('Main._lateInitialization');
    this._registerShortcuts();
    Extensions.extensionServer.initializeExtensions();
    if (!Host.isUnderTest()) {
      for (const extension of self.runtime.extensions('late-initialization'))
        extension.instance().then(instance => (/** @type {!Common.Runnable} */ (instance)).run());
    }
    Main.Main.timeEnd('Main._lateInitialization');
  }

  _registerForwardedShortcuts() {
    /** @const */ const forwardedActions = [
      'main.toggle-dock', 'debugger.toggle-breakpoints-active', 'debugger.toggle-pause', 'commandMenu.show',
      'console.show'
    ];
    const actionKeys =
        UI.shortcutRegistry.keysForActions(forwardedActions).map(UI.KeyboardShortcut.keyCodeAndModifiersFromKey);
    InspectorFrontendHost.setWhitelistedShortcuts(JSON.stringify(actionKeys));
  }

  _registerMessageSinkListener() {
    Common.console.addEventListener(Common.Console.Events.MessageAdded, messageAdded);

    /**
     * @param {!Common.Event} event
     */
    function messageAdded(event) {
      const message = /** @type {!Common.Console.Message} */ (event.data);
      if (message.show)
        Common.console.show();
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _revealSourceLine(event) {
    const url = /** @type {string} */ (event.data['url']);
    const lineNumber = /** @type {number} */ (event.data['lineNumber']);
    const columnNumber = /** @type {number} */ (event.data['columnNumber']);

    const uiSourceCode = Workspace.workspace.uiSourceCodeForURL(url);
    if (uiSourceCode) {
      Common.Revealer.reveal(uiSourceCode.uiLocation(lineNumber, columnNumber));
      return;
    }

    /**
     * @param {!Common.Event} event
     */
    function listener(event) {
      const uiSourceCode = /** @type {!Workspace.UISourceCode} */ (event.data);
      if (uiSourceCode.url() === url) {
        Common.Revealer.reveal(uiSourceCode.uiLocation(lineNumber, columnNumber));
        Workspace.workspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeAdded, listener);
      }
    }

    Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, listener);
  }

  _registerShortcuts() {
    const shortcut = UI.KeyboardShortcut;
    const section = UI.shortcutsScreen.section(Common.UIString('All Panels'));
    let keys = [
      shortcut.makeDescriptor('[', shortcut.Modifiers.CtrlOrMeta),
      shortcut.makeDescriptor(']', shortcut.Modifiers.CtrlOrMeta)
    ];
    section.addRelatedKeys(keys, Common.UIString('Go to the panel to the left/right'));

    const toggleConsoleLabel = Common.UIString('Show console');
    section.addKey(shortcut.makeDescriptor(shortcut.Keys.Tilde, shortcut.Modifiers.Ctrl), toggleConsoleLabel);
    section.addKey(shortcut.makeDescriptor(shortcut.Keys.Esc), Common.UIString('Toggle drawer'));
    if (Components.dockController.canDock()) {
      section.addKey(
          shortcut.makeDescriptor('M', shortcut.Modifiers.CtrlOrMeta | shortcut.Modifiers.Shift),
          Common.UIString('Toggle device mode'));
      section.addKey(
          shortcut.makeDescriptor('D', shortcut.Modifiers.CtrlOrMeta | shortcut.Modifiers.Shift),
          Common.UIString('Toggle dock side'));
    }
    section.addKey(shortcut.makeDescriptor('f', shortcut.Modifiers.CtrlOrMeta), Common.UIString('Search'));

    const advancedSearchShortcutModifier = Host.isMac() ?
        UI.KeyboardShortcut.Modifiers.Meta | UI.KeyboardShortcut.Modifiers.Alt :
        UI.KeyboardShortcut.Modifiers.Ctrl | UI.KeyboardShortcut.Modifiers.Shift;
    const advancedSearchShortcut = shortcut.makeDescriptor('f', advancedSearchShortcutModifier);
    section.addKey(advancedSearchShortcut, Common.UIString('Search across all sources'));

    const inspectElementModeShortcuts =
        UI.shortcutRegistry.shortcutDescriptorsForAction('elements.toggle-element-search');
    if (inspectElementModeShortcuts.length)
      section.addKey(inspectElementModeShortcuts[0], Common.UIString('Select node to inspect'));

    const openResourceShortcut = UI.KeyboardShortcut.makeDescriptor('p', UI.KeyboardShortcut.Modifiers.CtrlOrMeta);
    section.addKey(openResourceShortcut, Common.UIString('Go to source'));

    if (Host.isMac()) {
      keys = [
        shortcut.makeDescriptor('g', shortcut.Modifiers.Meta),
        shortcut.makeDescriptor('g', shortcut.Modifiers.Meta | shortcut.Modifiers.Shift)
      ];
      section.addRelatedKeys(keys, Common.UIString('Find next/previous'));
    }
  }

  _postDocumentKeyDown(event) {
    if (!event.handled)
      UI.shortcutRegistry.handleShortcut(event);
  }

  /**
   * @param {!Event} event
   */
  _redispatchClipboardEvent(event) {
    const eventCopy = new CustomEvent('clipboard-' + event.type, {bubbles: true});
    eventCopy['original'] = event;
    const document = event.target && event.target.ownerDocument;
    const target = document ? document.deepActiveElement() : null;
    if (target)
      target.dispatchEvent(eventCopy);
    if (eventCopy.handled)
      event.preventDefault();
  }

  _contextMenuEventFired(event) {
    if (event.handled || event.target.classList.contains('popup-glasspane'))
      event.preventDefault();
  }

  /**
   * @param {!Document} document
   */
  _addMainEventListeners(document) {
    document.addEventListener('keydown', this._postDocumentKeyDown.bind(this), false);
    document.addEventListener('beforecopy', this._redispatchClipboardEvent.bind(this), true);
    document.addEventListener('copy', this._redispatchClipboardEvent.bind(this), false);
    document.addEventListener('cut', this._redispatchClipboardEvent.bind(this), false);
    document.addEventListener('paste', this._redispatchClipboardEvent.bind(this), false);
    document.addEventListener('contextmenu', this._contextMenuEventFired.bind(this), true);
  }

  _onSuspendStateChanged() {
    const suspended = SDK.targetManager.allTargetsSuspended();
    UI.inspectorView.onSuspendStateChanged(suspended);
  }
};

/**
 * @implements {UI.ActionDelegate}
 * @unrestricted
 */
Main.Main.ZoomActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    if (InspectorFrontendHost.isHostedMode())
      return false;

    switch (actionId) {
      case 'main.zoom-in':
        InspectorFrontendHost.zoomIn();
        return true;
      case 'main.zoom-out':
        InspectorFrontendHost.zoomOut();
        return true;
      case 'main.zoom-reset':
        InspectorFrontendHost.resetZoom();
        return true;
    }
    return false;
  }
};

/**
 * @implements {UI.ActionDelegate}
 * @unrestricted
 */
Main.Main.SearchActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   * @suppressGlobalPropertiesCheck
   */
  handleAction(context, actionId) {
    const searchableView = UI.SearchableView.fromElement(document.deepActiveElement()) ||
        UI.inspectorView.currentPanelDeprecated().searchableView();
    if (!searchableView)
      return false;
    switch (actionId) {
      case 'main.search-in-panel.find':
        return searchableView.handleFindShortcut();
      case 'main.search-in-panel.cancel':
        return searchableView.handleCancelSearchShortcut();
      case 'main.search-in-panel.find-next':
        return searchableView.handleFindNextShortcut();
      case 'main.search-in-panel.find-previous':
        return searchableView.handleFindPreviousShortcut();
    }
    return false;
  }
};

/**
 * @implements {UI.ToolbarItem.Provider}
 */
Main.Main.MainMenuItem = class {
  constructor() {
    this._item = new UI.ToolbarMenuButton(this._handleContextMenu.bind(this), true);
    this._item.setTitle(Common.UIString('Customize and control DevTools'));
  }

  /**
   * @override
   * @return {?UI.ToolbarItem}
   */
  item() {
    return this._item;
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   */
  _handleContextMenu(contextMenu) {
    if (Components.dockController.canDock()) {
      const dockItemElement = createElementWithClass('div', 'flex-centered flex-auto');
      const titleElement = dockItemElement.createChild('span', 'flex-auto');
      titleElement.textContent = Common.UIString('Dock side');
      const toggleDockSideShorcuts = UI.shortcutRegistry.shortcutDescriptorsForAction('main.toggle-dock');
      titleElement.title = Common.UIString(
          'Placement of DevTools relative to the page. (%s to restore last position)', toggleDockSideShorcuts[0].name);
      dockItemElement.appendChild(titleElement);
      const dockItemToolbar = new UI.Toolbar('', dockItemElement);
      if (Host.isMac() && !UI.themeSupport.hasTheme())
        dockItemToolbar.makeBlueOnHover();
      const undock = new UI.ToolbarToggle(Common.UIString('Undock into separate window'), 'largeicon-undock');
      const bottom = new UI.ToolbarToggle(Common.UIString('Dock to bottom'), 'largeicon-dock-to-bottom');
      const right = new UI.ToolbarToggle(Common.UIString('Dock to right'), 'largeicon-dock-to-right');
      const left = new UI.ToolbarToggle(Common.UIString('Dock to left'), 'largeicon-dock-to-left');
      undock.addEventListener(UI.ToolbarButton.Events.MouseDown, event => event.data.consume());
      bottom.addEventListener(UI.ToolbarButton.Events.MouseDown, event => event.data.consume());
      right.addEventListener(UI.ToolbarButton.Events.MouseDown, event => event.data.consume());
      left.addEventListener(UI.ToolbarButton.Events.MouseDown, event => event.data.consume());
      undock.addEventListener(
          UI.ToolbarButton.Events.MouseUp, setDockSide.bind(null, Components.DockController.State.Undocked));
      bottom.addEventListener(
          UI.ToolbarButton.Events.MouseUp, setDockSide.bind(null, Components.DockController.State.DockedToBottom));
      right.addEventListener(
          UI.ToolbarButton.Events.MouseUp, setDockSide.bind(null, Components.DockController.State.DockedToRight));
      left.addEventListener(
          UI.ToolbarButton.Events.MouseUp, setDockSide.bind(null, Components.DockController.State.DockedToLeft));
      undock.setToggled(Components.dockController.dockSide() === Components.DockController.State.Undocked);
      bottom.setToggled(Components.dockController.dockSide() === Components.DockController.State.DockedToBottom);
      right.setToggled(Components.dockController.dockSide() === Components.DockController.State.DockedToRight);
      left.setToggled(Components.dockController.dockSide() === Components.DockController.State.DockedToLeft);
      dockItemToolbar.appendToolbarItem(undock);
      dockItemToolbar.appendToolbarItem(left);
      dockItemToolbar.appendToolbarItem(bottom);
      dockItemToolbar.appendToolbarItem(right);
      contextMenu.headerSection().appendCustomItem(dockItemElement);
    }

    /**
     * @param {string} side
     */
    function setDockSide(side) {
      Components.dockController.setDockSide(side);
      contextMenu.discard();
    }

    if (Components.dockController.dockSide() === Components.DockController.State.Undocked &&
        SDK.targetManager.mainTarget() && SDK.targetManager.mainTarget().hasBrowserCapability())
      contextMenu.defaultSection().appendAction('inspector_main.focus-debuggee', Common.UIString('Focus debuggee'));

    contextMenu.defaultSection().appendAction(
        'main.toggle-drawer',
        UI.inspectorView.drawerVisible() ? Common.UIString('Hide console drawer') :
                                           Common.UIString('Show console drawer'));
    contextMenu.appendItemsAtLocation('mainMenu');
    const moreTools = contextMenu.defaultSection().appendSubMenuItem(Common.UIString('More tools'));
    const extensions = self.runtime.extensions('view', undefined, true);
    for (const extension of extensions) {
      const descriptor = extension.descriptor();
      if (descriptor['persistence'] !== 'closeable')
        continue;
      if (descriptor['location'] !== 'drawer-view' && descriptor['location'] !== 'panel')
        continue;
      moreTools.defaultSection().appendItem(
          extension.title(), UI.viewManager.showView.bind(UI.viewManager, descriptor['id']));
    }

    const helpSubMenu = contextMenu.footerSection().appendSubMenuItem(Common.UIString('Help'));
    helpSubMenu.appendItemsAtLocation('mainMenuHelp');
  }
};

/**
 * @unrestricted
 */
Main.Main.PauseListener = class {
  constructor() {
    SDK.targetManager.addModelListener(
        SDK.DebuggerModel, SDK.DebuggerModel.Events.DebuggerPaused, this._debuggerPaused, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _debuggerPaused(event) {
    SDK.targetManager.removeModelListener(
        SDK.DebuggerModel, SDK.DebuggerModel.Events.DebuggerPaused, this._debuggerPaused, this);
    const debuggerModel = /** @type {!SDK.DebuggerModel} */ (event.data);
    const debuggerPausedDetails = debuggerModel.debuggerPausedDetails();
    UI.context.setFlavor(SDK.Target, debuggerModel.target());
    Common.Revealer.reveal(debuggerPausedDetails);
  }
};

/**
 * @param {string} method
 * @param {?Object} params
 * @return {!Promise}
 */
Main.sendOverProtocol = function(method, params) {
  return new Promise((resolve, reject) => {
    Protocol.InspectorBackend.sendRawMessageForTesting(method, params, (err, ...results) => {
      if (err)
        return reject(err);
      return resolve(results);
    });
  });
};

/**
 * @implements {UI.ActionDelegate}
 * @unrestricted
 */
Main.ReloadActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    switch (actionId) {
      case 'main.debug-reload':
        Components.reload();
        return true;
    }
    return false;
  }
};

new Main.Main();
