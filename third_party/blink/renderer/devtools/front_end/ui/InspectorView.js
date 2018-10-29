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
/**
 * @implements {UI.ViewLocationResolver}
 * @unrestricted
 */
UI.InspectorView = class extends UI.VBox {
  constructor() {
    super();
    UI.GlassPane.setContainer(this.element);
    this.setMinimumSize(240, 72);

    // DevTools sidebar is a vertical split of panels tabbed pane and a drawer.
    this._drawerSplitWidget = new UI.SplitWidget(false, true, 'Inspector.drawerSplitViewState', 200, 200);
    this._drawerSplitWidget.hideSidebar();
    this._drawerSplitWidget.hideDefaultResizer();
    this._drawerSplitWidget.enableShowModeSaving();
    this._drawerSplitWidget.show(this.element);

    if (Runtime.experiments.isEnabled('splitInDrawer')) {
      this._innerDrawerSplitWidget = new UI.SplitWidget(true, true, 'Inspector.drawerSidebarSplitViewState', 200, 200);
      this._drawerSplitWidget.setSidebarWidget(this._innerDrawerSplitWidget);
      this._drawerSidebarTabbedLocation =
          UI.viewManager.createTabbedLocation(this._showDrawer.bind(this, false), 'drawer-sidebar', true, true);
      this._drawerSidebarTabbedPane = this._drawerSidebarTabbedLocation.tabbedPane();
      this._drawerSidebarTabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected, this._drawerTabSelected, this);
      this._innerDrawerSplitWidget.setSidebarWidget(this._drawerSidebarTabbedPane);
    }

    // Create drawer tabbed pane.
    this._drawerTabbedLocation =
        UI.viewManager.createTabbedLocation(this._showDrawer.bind(this, false), 'drawer-view', true, true);
    this._drawerTabbedLocation.enableMoreTabsButton();
    this._drawerTabbedPane = this._drawerTabbedLocation.tabbedPane();
    this._drawerTabbedPane.setMinimumSize(0, 27);
    const closeDrawerButton = new UI.ToolbarButton(Common.UIString('Close drawer'), 'largeicon-delete');
    closeDrawerButton.addEventListener(UI.ToolbarButton.Events.Click, this._closeDrawer, this);
    this._drawerSplitWidget.installResizer(this._drawerTabbedPane.headerElement());
    this._drawerTabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected, this._drawerTabSelected, this);

    if (this._drawerSidebarTabbedPane) {
      this._innerDrawerSplitWidget.setMainWidget(this._drawerTabbedPane);
      this._drawerSidebarTabbedPane.rightToolbar().appendToolbarItem(closeDrawerButton);
      this._drawerSplitWidget.installResizer(this._drawerSidebarTabbedPane.headerElement());
    } else {
      this._drawerSplitWidget.setSidebarWidget(this._drawerTabbedPane);
      this._drawerTabbedPane.rightToolbar().appendToolbarItem(closeDrawerButton);
    }

    // Create main area tabbed pane.
    this._tabbedLocation = UI.viewManager.createTabbedLocation(
        InspectorFrontendHost.bringToFront.bind(InspectorFrontendHost), 'panel', true, true,
        Runtime.queryParam('panel'));

    this._tabbedPane = this._tabbedLocation.tabbedPane();
    this._tabbedPane.registerRequiredCSS('ui/inspectorViewTabbedPane.css');
    this._tabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected, this._tabSelected, this);
    this._tabbedPane.setAccessibleName(Common.UIString('Panels'));

    if (Host.isUnderTest())
      this._tabbedPane.setAutoSelectFirstItemOnShow(false);
    this._drawerSplitWidget.setMainWidget(this._tabbedPane);

    this._keyDownBound = this._keyDown.bind(this);
    InspectorFrontendHost.events.addEventListener(InspectorFrontendHostAPI.Events.ShowPanel, showPanel.bind(this));

    /**
     * @this {UI.InspectorView}
     * @param {!Common.Event} event
     */
    function showPanel(event) {
      const panelName = /** @type {string} */ (event.data);
      this.showPanel(panelName);
    }
  }

  /**
   * @return {!UI.InspectorView}
   */
  static instance() {
    return /** @type {!UI.InspectorView} */ (self.runtime.sharedInstance(UI.InspectorView));
  }

  /**
   * @override
   */
  wasShown() {
    this.element.ownerDocument.addEventListener('keydown', this._keyDownBound, false);
  }

  /**
   * @override
   */
  willHide() {
    this.element.ownerDocument.removeEventListener('keydown', this._keyDownBound, false);
  }

  /**
   * @override
   * @param {string} locationName
   * @return {?UI.ViewLocation}
   */
  resolveLocation(locationName) {
    if (locationName === 'drawer-view')
      return this._drawerTabbedLocation;
    if (locationName === 'panel')
      return this._tabbedLocation;
    if (locationName === 'drawer-sidebar')
      return this._drawerSidebarTabbedLocation;
    return null;
  }

  createToolbars() {
    this._tabbedPane.leftToolbar().appendLocationItems('main-toolbar-left');
    this._tabbedPane.rightToolbar().appendLocationItems('main-toolbar-right');
  }

  /**
   * @param {!UI.View} view
   */
  addPanel(view) {
    this._tabbedLocation.appendView(view);
  }

  /**
   * @param {string} panelName
   * @return {boolean}
   */
  hasPanel(panelName) {
    return this._tabbedPane.hasTab(panelName);
  }

  /**
   * @param {string} panelName
   * @return {!Promise.<!UI.Panel>}
   */
  panel(panelName) {
    return /** @type {!Promise.<!UI.Panel>} */ (UI.viewManager.view(panelName).widget());
  }

  /**
   * @param {boolean} allTargetsSuspended
   */
  onSuspendStateChanged(allTargetsSuspended) {
    this._currentPanelLocked = allTargetsSuspended;
    this._tabbedPane.setCurrentTabLocked(this._currentPanelLocked);
    this._tabbedPane.leftToolbar().setEnabled(!this._currentPanelLocked);
    this._tabbedPane.rightToolbar().setEnabled(!this._currentPanelLocked);
  }

  /**
   * @param {string} panelName
   * @return {boolean}
   */
  canSelectPanel(panelName) {
    return !this._currentPanelLocked || this._tabbedPane.selectedTabId === panelName;
  }

  /**
   * @param {string} panelName
   * @return {!Promise.<?UI.Panel>}
   */
  showPanel(panelName) {
    return UI.viewManager.showView(panelName);
  }

  /**
   * @param {string} panelName
   * @param {?UI.Icon} icon
   */
  setPanelIcon(panelName, icon) {
    this._tabbedPane.setTabIcon(panelName, icon);
  }

  /**
   * @return {!UI.Panel}
   */
  currentPanelDeprecated() {
    return /** @type {!UI.Panel} */ (UI.viewManager.materializedWidget(this._tabbedPane.selectedTabId || ''));
  }

  /**
   * @param {boolean} focus
   */
  _showDrawer(focus) {
    if (this._drawerTabbedPane.isShowing())
      return;
    this._drawerSplitWidget.showBoth();
    if (focus)
      this._focusRestorer = new UI.WidgetFocusRestorer(this._drawerTabbedPane);
    else
      this._focusRestorer = null;
  }

  /**
   * @return {boolean}
   */
  drawerVisible() {
    return this._drawerTabbedPane.isShowing();
  }

  _closeDrawer() {
    if (!this._drawerTabbedPane.isShowing())
      return;
    if (this._focusRestorer)
      this._focusRestorer.restore();
    this._drawerSplitWidget.hideSidebar(true);
  }

  /**
   * @param {boolean} minimized
   */
  setDrawerMinimized(minimized) {
    this._drawerSplitWidget.setSidebarMinimized(minimized);
    this._drawerSplitWidget.setResizable(!minimized);
  }

  /**
   * @return {boolean}
   */
  isDrawerMinimized() {
    return this._drawerSplitWidget.isSidebarMinimized();
  }

  /**
   * @param {string} id
   * @param {boolean=} userGesture
   */
  closeDrawerTab(id, userGesture) {
    this._drawerTabbedPane.closeTab(id, userGesture);
  }

  /**
   * @param {!Event} event
   */
  _keyDown(event) {
    const keyboardEvent = /** @type {!KeyboardEvent} */ (event);
    if (!UI.KeyboardShortcut.eventHasCtrlOrMeta(keyboardEvent) || event.altKey || event.shiftKey)
      return;

    // Ctrl/Cmd + 1-9 should show corresponding panel.
    const panelShortcutEnabled = Common.moduleSetting('shortcutPanelSwitch').get();
    if (panelShortcutEnabled) {
      let panelIndex = -1;
      if (event.keyCode > 0x30 && event.keyCode < 0x3A)
        panelIndex = event.keyCode - 0x31;
      else if (
          event.keyCode > 0x60 && event.keyCode < 0x6A &&
          keyboardEvent.location === KeyboardEvent.DOM_KEY_LOCATION_NUMPAD)
        panelIndex = event.keyCode - 0x61;
      if (panelIndex !== -1) {
        const panelName = this._tabbedPane.allTabs()[panelIndex];
        if (panelName) {
          if (!UI.Dialog.hasInstance() && !this._currentPanelLocked)
            this.showPanel(panelName);
          event.consume(true);
        }
      }
    }
  }

  /**
   * @override
   */
  onResize() {
    UI.GlassPane.containerMoved(this.element);
  }

  /**
   * @return {!Element}
   */
  topResizerElement() {
    return this._tabbedPane.headerElement();
  }

  toolbarItemResized() {
    this._tabbedPane.headerResized();
  }

  /**
   * @param {!Common.Event} event
   */
  _tabSelected(event) {
    const tabId = /** @type {string} */ (event.data['tabId']);
    Host.userMetrics.panelShown(tabId);
  }

  /**
   * @param {!Common.Event} event
   */
  _drawerTabSelected(event) {
    const tabId = /** @type {string} */ (event.data['tabId']);
    Host.userMetrics.drawerShown(tabId);
  }

  /**
   * @param {!UI.SplitWidget} splitWidget
   */
  setOwnerSplit(splitWidget) {
    this._ownerSplitWidget = splitWidget;
  }

  minimize() {
    if (this._ownerSplitWidget)
      this._ownerSplitWidget.setSidebarMinimized(true);
  }

  restore() {
    if (this._ownerSplitWidget)
      this._ownerSplitWidget.setSidebarMinimized(false);
  }
};


/**
 * @type {!UI.InspectorView}
 */
UI.inspectorView;

/**
 * @implements {UI.ActionDelegate}
 * @unrestricted
 */
UI.InspectorView.ActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    switch (actionId) {
      case 'main.toggle-drawer':
        if (UI.inspectorView.drawerVisible())
          UI.inspectorView._closeDrawer();
        else
          UI.inspectorView._showDrawer(true);
        return true;
      case 'main.next-tab':
        UI.inspectorView._tabbedPane.selectNextTab();
        UI.inspectorView._tabbedPane.focus();
        return true;
      case 'main.previous-tab':
        UI.inspectorView._tabbedPane.selectPrevTab();
        UI.inspectorView._tabbedPane.focus();
        return true;
    }
    return false;
  }
};
