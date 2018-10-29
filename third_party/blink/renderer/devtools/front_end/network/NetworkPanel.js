/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * @implements {UI.ContextMenu.Provider}
 * @implements {UI.ViewLocationResolver}
 */
Network.NetworkPanel = class extends UI.Panel {
  constructor() {
    super('network');
    this.registerRequiredCSS('network/networkPanel.css');

    this._networkLogShowOverviewSetting = Common.settings.createSetting('networkLogShowOverview', true);
    this._networkLogLargeRowsSetting = Common.settings.createSetting('networkLogLargeRows', false);
    this._networkRecordFilmStripSetting = Common.settings.createSetting('networkRecordFilmStripSetting', false);
    this._toggleRecordAction = /** @type {!UI.Action }*/ (UI.actionRegistry.action('network.toggle-recording'));

    /** @type {number|undefined} */
    this._pendingStopTimer;
    /** @type {?Network.NetworkItemView} */
    this._networkItemView = null;
    /** @type {?PerfUI.FilmStripView} */
    this._filmStripView = null;
    /** @type {?Network.NetworkPanel.FilmStripRecorder} */
    this._filmStripRecorder = null;

    const panel = new UI.VBox();

    this._panelToolbar = new UI.Toolbar('', panel.contentElement);
    this._filterBar = new UI.FilterBar('networkPanel', true);
    this._filterBar.show(panel.contentElement);

    this._filmStripPlaceholderElement = panel.contentElement.createChild('div', 'network-film-strip-placeholder');

    // Create top overview component.
    this._overviewPane = new PerfUI.TimelineOverviewPane('network');
    this._overviewPane.addEventListener(
        PerfUI.TimelineOverviewPane.Events.WindowChanged, this._onWindowChanged.bind(this));
    this._overviewPane.element.id = 'network-overview-panel';
    this._networkOverview = new Network.NetworkOverview();
    this._overviewPane.setOverviewControls([this._networkOverview]);
    this._overviewPlaceholderElement = panel.contentElement.createChild('div');

    this._calculator = new Network.NetworkTransferTimeCalculator();

    this._splitWidget = new UI.SplitWidget(true, false, 'networkPanelSplitViewState');
    this._splitWidget.hideMain();
    this._splitWidget.show(panel.contentElement);

    panel.setDefaultFocusedChild(this._filterBar);

    const initialSidebarWidth = 225;
    const splitWidget = new UI.SplitWidget(true, false, 'networkPanelSidebarState', initialSidebarWidth);
    splitWidget.hideSidebar();
    splitWidget.enableShowModeSaving();
    splitWidget.element.tabIndex = 0;
    splitWidget.show(this.element);
    this._sidebarLocation = UI.viewManager.createTabbedLocation(async () => {
      UI.viewManager.showView('network');
      splitWidget.showBoth();
    }, 'network-sidebar', true);
    const tabbedPane = this._sidebarLocation.tabbedPane();
    tabbedPane.setMinimumSize(100, 25);
    tabbedPane.element.classList.add('network-tabbed-pane');
    tabbedPane.element.addEventListener('keydown', event => {
      if (event.key !== 'Escape')
        return;
      splitWidget.hideSidebar();
      event.consume();
    });
    const closeSidebar = new UI.ToolbarButton(Common.UIString('Close'), 'largeicon-delete');
    closeSidebar.addEventListener(UI.ToolbarButton.Events.Click, () => splitWidget.hideSidebar());
    tabbedPane.rightToolbar().appendToolbarItem(closeSidebar);
    splitWidget.setSidebarWidget(tabbedPane);
    splitWidget.setMainWidget(panel);
    splitWidget.setDefaultFocusedChild(panel);
    this.setDefaultFocusedChild(splitWidget);

    this._progressBarContainer = createElement('div');

    /** @type {!Network.NetworkLogView} */
    this._networkLogView =
        new Network.NetworkLogView(this._filterBar, this._progressBarContainer, this._networkLogLargeRowsSetting);
    this._splitWidget.setSidebarWidget(this._networkLogView);

    this._detailsWidget = new UI.VBox();
    this._detailsWidget.element.classList.add('network-details-view');
    this._splitWidget.setMainWidget(this._detailsWidget);

    this._closeButtonElement = createElement('div', 'dt-close-button');
    this._closeButtonElement.addEventListener('click', this._showRequest.bind(this, null), false);
    this._closeButtonElement.style.margin = '0 5px';

    this._networkLogShowOverviewSetting.addChangeListener(this._toggleShowOverview, this);
    this._networkLogLargeRowsSetting.addChangeListener(this._toggleLargerRequests, this);
    this._networkRecordFilmStripSetting.addChangeListener(this._toggleRecordFilmStrip, this);

    this._preserveLogSetting = Common.moduleSetting('network_log.preserve-log');

    this._offlineCheckbox = MobileThrottling.throttlingManager().createOfflineToolbarCheckbox();
    this._throttlingSelect = this._createThrottlingConditionsSelect();
    this._setupToolbarButtons(splitWidget);

    this._toggleRecord(true);
    this._toggleShowOverview();
    this._toggleLargerRequests();
    this._toggleRecordFilmStrip();
    this._updateUI();

    SDK.targetManager.addModelListener(
        SDK.ResourceTreeModel, SDK.ResourceTreeModel.Events.WillReloadPage, this._willReloadPage, this);
    SDK.targetManager.addModelListener(SDK.ResourceTreeModel, SDK.ResourceTreeModel.Events.Load, this._load, this);
    this._networkLogView.addEventListener(Network.NetworkLogView.Events.RequestSelected, this._onRequestSelected, this);
    SDK.networkLog.addEventListener(SDK.NetworkLog.Events.RequestAdded, this._onUpdateRequest, this);
    SDK.networkLog.addEventListener(SDK.NetworkLog.Events.RequestUpdated, this._onUpdateRequest, this);
    SDK.networkLog.addEventListener(SDK.NetworkLog.Events.Reset, this._onNetworkLogReset, this);
  }

  /**
   * @param {!Array<{filterType: !Network.NetworkLogView.FilterType, filterValue: string}>} filters
   */
  static revealAndFilter(filters) {
    const panel = Network.NetworkPanel._instance();
    let filterString = '';
    for (const filter of filters)
      filterString += `${filter.filterType}:${filter.filterValue} `;
    panel._networkLogView.setTextFilterValue(filterString);
    UI.viewManager.showView('network');
  }

  /**
   * @return {!Network.NetworkPanel}
   */
  static _instance() {
    return /** @type {!Network.NetworkPanel} */ (self.runtime.sharedInstance(Network.NetworkPanel));
  }

  /**
   * @return {!UI.ToolbarCheckbox}
   */
  offlineCheckboxForTest() {
    return this._offlineCheckbox;
  }

  /**
   * @return {!UI.ToolbarComboBox}
   */
  throttlingSelectForTest() {
    return this._throttlingSelect;
  }

  /**
   * @param {!Common.Event} event
   */
  _onWindowChanged(event) {
    const startTime = Math.max(this._calculator.minimumBoundary(), event.data.startTime / 1000);
    const endTime = Math.min(this._calculator.maximumBoundary(), event.data.endTime / 1000);
    this._networkLogView.setWindow(startTime, endTime);
  }

  _setupToolbarButtons(splitWidget) {
    const searchToggle = new UI.ToolbarToggle('Search', 'largeicon-search');
    function updateSidebarToggle() {
      searchToggle.setToggled(splitWidget.showMode() !== UI.SplitWidget.ShowMode.OnlyMain);
    }
    this._panelToolbar.appendToolbarItem(UI.Toolbar.createActionButton(this._toggleRecordAction));
    const clearButton = new UI.ToolbarButton(Common.UIString('Clear'), 'largeicon-clear');
    clearButton.addEventListener(UI.ToolbarButton.Events.Click, () => SDK.networkLog.reset(), this);
    this._panelToolbar.appendToolbarItem(clearButton);
    this._panelToolbar.appendSeparator();
    const recordFilmStripButton = new UI.ToolbarSettingToggle(
        this._networkRecordFilmStripSetting, 'largeicon-camera', Common.UIString('Capture screenshots'));
    this._panelToolbar.appendToolbarItem(recordFilmStripButton);

    this._panelToolbar.appendToolbarItem(this._filterBar.filterButton());
    updateSidebarToggle();
    splitWidget.addEventListener(UI.SplitWidget.Events.ShowModeChanged, updateSidebarToggle);
    searchToggle.addEventListener(UI.ToolbarButton.Events.Click, () => {
      if (splitWidget.showMode() === UI.SplitWidget.ShowMode.OnlyMain)
        splitWidget.showBoth();
      else
        splitWidget.hideSidebar();
    });
    this._panelToolbar.appendToolbarItem(searchToggle);
    this._panelToolbar.appendSeparator();

    this._panelToolbar.appendText(Common.UIString('View:'));

    const largerRequestsButton = new UI.ToolbarSettingToggle(
        this._networkLogLargeRowsSetting, 'largeicon-large-list', Common.UIString('Use large request rows'),
        Common.UIString('Use small request rows'));
    this._panelToolbar.appendToolbarItem(largerRequestsButton);

    const showOverviewButton = new UI.ToolbarSettingToggle(
        this._networkLogShowOverviewSetting, 'largeicon-waterfall', Common.UIString('Show overview'),
        Common.UIString('Hide overview'));
    this._panelToolbar.appendToolbarItem(showOverviewButton);

    this._panelToolbar.appendToolbarItem(new UI.ToolbarSettingCheckbox(
        Common.moduleSetting('network.group-by-frame'), '', Common.UIString('Group by frame')));

    this._panelToolbar.appendSeparator();
    this._panelToolbar.appendToolbarItem(new UI.ToolbarSettingCheckbox(
        this._preserveLogSetting, Common.UIString('Do not clear log on page reload / navigation'),
        Common.UIString('Preserve log')));

    const disableCacheCheckbox = new UI.ToolbarSettingCheckbox(
        Common.moduleSetting('cacheDisabled'), Common.UIString('Disable cache (while DevTools is open)'),
        Common.UIString('Disable cache'));
    this._panelToolbar.appendToolbarItem(disableCacheCheckbox);

    this._panelToolbar.appendSeparator();
    this._panelToolbar.appendToolbarItem(this._offlineCheckbox);
    this._panelToolbar.appendToolbarItem(this._throttlingSelect);

    this._panelToolbar.appendToolbarItem(new UI.ToolbarItem(this._progressBarContainer));
  }

  /**
   * @return {!UI.ToolbarComboBox}
   */
  _createThrottlingConditionsSelect() {
    const toolbarItem = new UI.ToolbarComboBox(null);
    toolbarItem.setTitle(ls`Throttling`);
    toolbarItem.setMaxWidth(160);
    MobileThrottling.throttlingManager().decorateSelectWithNetworkThrottling(toolbarItem.selectElement());
    return toolbarItem;
  }

  _toggleRecording() {
    if (!this._preserveLogSetting.get() && !this._toggleRecordAction.toggled())
      SDK.networkLog.reset();
    this._toggleRecord(!this._toggleRecordAction.toggled());
  }

  /**
   * @param {boolean} toggled
   */
  _toggleRecord(toggled) {
    this._toggleRecordAction.setToggled(toggled);
    this._networkLogView.setRecording(toggled);
    if (!toggled && this._filmStripRecorder)
      this._filmStripRecorder.stopRecording(this._filmStripAvailable.bind(this));
    // TODO(einbinder) This should be moved to a setting/action that NetworkLog owns but NetworkPanel controls, but
    // always be present in the command menu.
    SDK.networkLog.setIsRecording(toggled);
  }

  /**
   * @param {?SDK.FilmStripModel} filmStripModel
   */
  _filmStripAvailable(filmStripModel) {
    if (!filmStripModel)
      return;
    const calculator = this._networkLogView.timeCalculator();
    this._filmStripView.setModel(filmStripModel, calculator.minimumBoundary() * 1000, calculator.boundarySpan() * 1000);
    this._networkOverview.setFilmStripModel(filmStripModel);
    const timestamps = filmStripModel.frames().map(mapTimestamp);

    /**
     * @param {!SDK.FilmStripModel.Frame} frame
     * @return {number}
     */
    function mapTimestamp(frame) {
      return frame.timestamp / 1000;
    }

    this._networkLogView.addFilmStripFrames(timestamps);
  }

  _onNetworkLogReset() {
    Network.BlockedURLsPane.reset();
    if (!this._preserveLogSetting.get()) {
      this._calculator.reset();
      this._overviewPane.reset();
    }
    if (this._filmStripView)
      this._resetFilmStripView();
  }

  /**
   * @param {!Common.Event} event
   */
  _willReloadPage(event) {
    this._toggleRecord(true);
    if (this._pendingStopTimer) {
      clearTimeout(this._pendingStopTimer);
      delete this._pendingStopTimer;
    }
    if (this.isShowing() && this._filmStripRecorder)
      this._filmStripRecorder.startRecording();
  }

  /**
   * @param {!Common.Event} event
   */
  _load(event) {
    if (this._filmStripRecorder && this._filmStripRecorder.isRecording()) {
      this._pendingStopTimer =
          setTimeout(this._stopFilmStripRecording.bind(this), Network.NetworkPanel.displayScreenshotDelay);
    }
  }

  _stopFilmStripRecording() {
    this._filmStripRecorder.stopRecording(this._filmStripAvailable.bind(this));
    delete this._pendingStopTimer;
  }

  _toggleLargerRequests() {
    this._updateUI();
  }

  _toggleShowOverview() {
    const toggled = this._networkLogShowOverviewSetting.get();
    if (toggled)
      this._overviewPane.show(this._overviewPlaceholderElement);
    else
      this._overviewPane.detach();
    this.doResize();
  }

  _toggleRecordFilmStrip() {
    const toggled = this._networkRecordFilmStripSetting.get();
    if (toggled && !this._filmStripRecorder) {
      this._filmStripView = new PerfUI.FilmStripView();
      this._filmStripView.setMode(PerfUI.FilmStripView.Modes.FrameBased);
      this._filmStripView.element.classList.add('network-film-strip');
      this._filmStripRecorder =
          new Network.NetworkPanel.FilmStripRecorder(this._networkLogView.timeCalculator(), this._filmStripView);
      this._filmStripView.show(this._filmStripPlaceholderElement);
      this._filmStripView.addEventListener(PerfUI.FilmStripView.Events.FrameSelected, this._onFilmFrameSelected, this);
      this._filmStripView.addEventListener(PerfUI.FilmStripView.Events.FrameEnter, this._onFilmFrameEnter, this);
      this._filmStripView.addEventListener(PerfUI.FilmStripView.Events.FrameExit, this._onFilmFrameExit, this);
      this._resetFilmStripView();
    }

    if (!toggled && this._filmStripRecorder) {
      this._filmStripView.detach();
      this._filmStripView = null;
      this._filmStripRecorder = null;
    }
  }

  _resetFilmStripView() {
    this._filmStripView.reset();
    this._filmStripView.setStatusText(Common.UIString(
        'Hit %s to reload and capture filmstrip.',
        UI.shortcutRegistry.shortcutDescriptorsForAction('inspector_main.reload')[0].name));
  }

  /**
   * @override
   * @return {!Array.<!Element>}
   */
  elementsToRestoreScrollPositionsFor() {
    return this._networkLogView.elementsToRestoreScrollPositionsFor();
  }

  /**
   * @override
   */
  wasShown() {
    UI.context.setFlavor(Network.NetworkPanel, this);
  }

  /**
   * @override
   */
  willHide() {
    UI.context.setFlavor(Network.NetworkPanel, null);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  revealAndHighlightRequest(request) {
    this._showRequest(null);
    if (request)
      this._networkLogView.revealAndHighlightRequest(request);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {!Promise<?Network.NetworkItemView>}
   */
  async selectRequest(request) {
    await UI.viewManager.showView('network');
    this._networkLogView.selectRequest(request);
    return this._networkItemView;
  }

  /**
   * @param {!Common.Event} event
   */
  _onRowSizeChanged(event) {
    this._updateUI();
  }

  /**
   * @param {!Common.Event} event
   */
  _onRequestSelected(event) {
    const request = /** @type {?SDK.NetworkRequest} */ (event.data);
    this._showRequest(request);
  }

  /**
   * @param {?SDK.NetworkRequest} request
   */
  _showRequest(request) {
    if (this._networkItemView) {
      this._networkItemView.detach();
      this._networkItemView = null;
    }

    if (request) {
      this._networkItemView = new Network.NetworkItemView(request, this._networkLogView.timeCalculator());
      this._networkItemView.leftToolbar().appendToolbarItem(new UI.ToolbarItem(this._closeButtonElement));
      this._networkItemView.show(this._detailsWidget.element);
      this._splitWidget.showBoth();
    } else {
      this._splitWidget.hideMain();
      this._networkLogView.clearSelection();
    }
    this._updateUI();
  }

  _updateUI() {
    this._detailsWidget.element.classList.toggle(
        'network-details-view-tall-header', this._networkLogLargeRowsSetting.get());
    this._networkLogView.switchViewMode(!this._splitWidget.isResizable());
  }

  /**
   * @override
   * @param {!Event} event
   * @param {!UI.ContextMenu} contextMenu
   * @param {!Object} target
   * @this {Network.NetworkPanel}
   */
  appendApplicableItems(event, contextMenu, target) {
    /**
     * @this {Network.NetworkPanel}
     */
    function reveal(request) {
      UI.viewManager.showView('network').then(this.revealAndHighlightRequest.bind(this, request));
    }

    /**
     * @this {Network.NetworkPanel}
     */
    function appendRevealItem(request) {
      contextMenu.revealSection().appendItem(Common.UIString('Reveal in Network panel'), reveal.bind(this, request));
    }

    if (event.target.isSelfOrDescendant(this.element))
      return;

    if (target instanceof SDK.Resource) {
      const resource = /** @type {!SDK.Resource} */ (target);
      if (resource.request)
        appendRevealItem.call(this, resource.request);
      return;
    }
    if (target instanceof Workspace.UISourceCode) {
      const uiSourceCode = /** @type {!Workspace.UISourceCode} */ (target);
      const resource = Bindings.resourceForURL(uiSourceCode.url());
      if (resource && resource.request)
        appendRevealItem.call(this, resource.request);
      return;
    }

    if (!(target instanceof SDK.NetworkRequest))
      return;
    const request = /** @type {!SDK.NetworkRequest} */ (target);
    if (this._networkItemView && this._networkItemView.isShowing() && this._networkItemView.request() === request)
      return;

    appendRevealItem.call(this, request);
  }

  /**
   * @param {!Common.Event} event
   */
  _onFilmFrameSelected(event) {
    const timestamp = /** @type {number} */ (event.data);
    this._overviewPane.setWindowTimes(0, timestamp);
  }

  /**
   * @param {!Common.Event} event
   */
  _onFilmFrameEnter(event) {
    const timestamp = /** @type {number} */ (event.data);
    this._networkOverview.selectFilmStripFrame(timestamp);
    this._networkLogView.selectFilmStripFrame(timestamp / 1000);
  }

  /**
   * @param {!Common.Event} event
   */
  _onFilmFrameExit(event) {
    this._networkOverview.clearFilmStripFrame();
    this._networkLogView.clearFilmStripFrame();
  }

  /**
   * @param {!Common.Event} event
   */
  _onUpdateRequest(event) {
    const request = /** @type {!SDK.NetworkRequest} */ (event.data);
    this._calculator.updateBoundaries(request);
    // FIXME: Unify all time units across the frontend!
    this._overviewPane.setBounds(this._calculator.minimumBoundary() * 1000, this._calculator.maximumBoundary() * 1000);
    this._networkOverview.updateRequest(request);
    this._overviewPane.scheduleUpdate();
  }

  /**
   * @override
   * @param {string} locationName
   * @return {?UI.ViewLocation}
   */
  resolveLocation(locationName) {
    if (locationName === 'network-sidebar')
      return this._sidebarLocation;
    return null;
  }
};

Network.NetworkPanel.displayScreenshotDelay = 1000;

/**
 * @implements {UI.ContextMenu.Provider}
 * @unrestricted
 */
Network.NetworkPanel.ContextMenuProvider = class {
  /**
   * @override
   * @param {!Event} event
   * @param {!UI.ContextMenu} contextMenu
   * @param {!Object} target
   */
  appendApplicableItems(event, contextMenu, target) {
    Network.NetworkPanel._instance().appendApplicableItems(event, contextMenu, target);
  }
};

/**
 * @implements {Common.Revealer}
 * @unrestricted
 */
Network.NetworkPanel.RequestRevealer = class {
  /**
   * @override
   * @param {!Object} request
   * @return {!Promise}
   */
  reveal(request) {
    if (!(request instanceof SDK.NetworkRequest))
      return Promise.reject(new Error('Internal error: not a network request'));
    const panel = Network.NetworkPanel._instance();
    return UI.viewManager.showView('network').then(panel.revealAndHighlightRequest.bind(panel, request));
  }
};


/**
 * @implements {SDK.TracingManagerClient}
 */
Network.NetworkPanel.FilmStripRecorder = class {
  /**
   * @param {!Network.NetworkTimeCalculator} timeCalculator
   * @param {!PerfUI.FilmStripView} filmStripView
   */
  constructor(timeCalculator, filmStripView) {
    /** @type {?SDK.TracingManager} */
    this._tracingManager = null;
    /** @type {?SDK.ResourceTreeModel} */
    this._resourceTreeModel = null;
    this._timeCalculator = timeCalculator;
    this._filmStripView = filmStripView;
    /** @type {?SDK.TracingModel} */
    this._tracingModel = null;
    /** @type {?function(?SDK.FilmStripModel)} */
    this._callback = null;
  }

  /**
   * @override
   * @param {!Array.<!SDK.TracingManager.EventPayload>} events
   */
  traceEventsCollected(events) {
    if (this._tracingModel)
      this._tracingModel.addEvents(events);
  }

  /**
   * @override
   */
  tracingComplete() {
    if (!this._tracingModel || !this._tracingManager)
      return;
    this._tracingModel.tracingComplete();
    this._tracingManager = null;
    this._callback(new SDK.FilmStripModel(this._tracingModel, this._timeCalculator.minimumBoundary() * 1000));
    this._callback = null;
    if (this._resourceTreeModel)
      this._resourceTreeModel.resumeReload();
    this._resourceTreeModel = null;
  }

  /**
   * @override
   */
  tracingBufferUsage() {
  }

  /**
   * @override
   * @param {number} progress
   */
  eventsRetrievalProgress(progress) {
  }

  startRecording() {
    this._filmStripView.reset();
    this._filmStripView.setStatusText(Common.UIString('Recording frames...'));
    const tracingManagers = SDK.targetManager.models(SDK.TracingManager);
    if (this._tracingManager || !tracingManagers.length)
      return;

    this._tracingManager = tracingManagers[0];
    this._resourceTreeModel = this._tracingManager.target().model(SDK.ResourceTreeModel);
    if (this._tracingModel)
      this._tracingModel.dispose();
    this._tracingModel = new SDK.TracingModel(new Bindings.TempFileBackingStorage());
    this._tracingManager.start(this, '-*,disabled-by-default-devtools.screenshot', '');

    Host.userMetrics.actionTaken(Host.UserMetrics.Action.FilmStripStartedRecording);
  }

  /**
   * @return {boolean}
   */
  isRecording() {
    return !!this._tracingManager;
  }

  /**
   * @param {function(?SDK.FilmStripModel)} callback
   */
  stopRecording(callback) {
    if (!this._tracingManager)
      return;

    this._tracingManager.stop();
    if (this._resourceTreeModel)
      this._resourceTreeModel.suspendReload();
    this._callback = callback;
    this._filmStripView.setStatusText(Common.UIString('Fetching frames...'));
  }
};

/**
 * @implements {UI.ActionDelegate}
 */
Network.NetworkPanel.ActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    const panel = UI.context.flavor(Network.NetworkPanel);
    console.assert(panel && panel instanceof Network.NetworkPanel);
    switch (actionId) {
      case 'network.toggle-recording':
        panel._toggleRecording();
        return true;
      case 'network.hide-request-details':
        if (!panel._networkItemView)
          return false;
        panel._showRequest(null);
        return true;
      case 'network.search':
        const selection = UI.inspectorView.element.window().getSelection();
        let queryCandidate = '';
        if (selection.rangeCount)
          queryCandidate = selection.toString().replace(/\r?\n.*/, '');
        Network.SearchNetworkView.openSearch(queryCandidate);
        return true;
    }
    return false;
  }
};

/**
 * @implements {Common.Revealer}
 */
Network.NetworkPanel.RequestLocationRevealer = class {
  /**
   * @override
   * @param {!Object} match
   * @return {!Promise}
   */
  async reveal(match) {
    const location = /** @type {!Network.UIRequestLocation} */ (match);
    const view = await Network.NetworkPanel._instance().selectRequest(location.request);
    if (!view)
      return;
    if (location.searchMatch)
      await view.revealResponseBody(location.searchMatch.lineNumber);
    if (location.requestHeader)
      view.revealRequestHeader(location.requestHeader.name);
    if (location.responseHeader)
      view.revealResponseHeader(location.responseHeader.name);
  }
};

Network.SearchNetworkView = class extends Search.SearchView {
  constructor() {
    super('network');
  }

  /**
   * @param {string} query
   * @param {boolean=} searchImmediately
   * @return {!Promise<!Search.SearchView>}
   */
  static async openSearch(query, searchImmediately) {
    await UI.viewManager.showView('network.search-network-tab');
    const searchView =
        /** @type {!Network.SearchNetworkView} */ (self.runtime.sharedInstance(Network.SearchNetworkView));
    searchView.toggle(query, !!searchImmediately);
    return searchView;
  }

  /**
   * @override
   * @return {!Search.SearchScope}
   */
  createScope() {
    return new Network.NetworkSearchScope();
  }
};
