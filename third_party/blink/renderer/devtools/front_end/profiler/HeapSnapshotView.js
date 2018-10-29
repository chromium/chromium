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
 * @implements {Profiler.ProfileType.DataDisplayDelegate}
 * @implements {UI.Searchable}
 * @unrestricted
 */
Profiler.HeapSnapshotView = class extends UI.SimpleView {
  /**
   * @param {!Profiler.ProfileType.DataDisplayDelegate} dataDisplayDelegate
   * @param {!Profiler.HeapProfileHeader} profile
   */
  constructor(dataDisplayDelegate, profile) {
    super(Common.UIString('Heap Snapshot'));

    this.element.classList.add('heap-snapshot-view');
    this._profile = profile;
    this._linkifier = new Components.Linkifier();
    const profileType = profile.profileType();

    profileType.addEventListener(Profiler.HeapSnapshotProfileType.SnapshotReceived, this._onReceiveSnapshot, this);
    profileType.addEventListener(Profiler.ProfileType.Events.RemoveProfileHeader, this._onProfileHeaderRemoved, this);

    const isHeapTimeline = profileType.id === Profiler.TrackingHeapSnapshotProfileType.TypeId;
    if (isHeapTimeline)
      this._createOverview();

    this._parentDataDisplayDelegate = dataDisplayDelegate;

    this._searchableView = new UI.SearchableView(this);
    this._searchableView.show(this.element);

    this._splitWidget = new UI.SplitWidget(false, true, 'heapSnapshotSplitViewState', 200, 200);
    this._splitWidget.show(this._searchableView.element);

    this._containmentDataGrid = new Profiler.HeapSnapshotContainmentDataGrid(this);
    this._containmentDataGrid.addEventListener(DataGrid.DataGrid.Events.SelectedNode, this._selectionChanged, this);
    this._containmentWidget = this._containmentDataGrid.asWidget();
    this._containmentWidget.setMinimumSize(50, 25);

    this._statisticsView = new Profiler.HeapSnapshotStatisticsView();

    this._constructorsDataGrid = new Profiler.HeapSnapshotConstructorsDataGrid(this);
    this._constructorsDataGrid.addEventListener(DataGrid.DataGrid.Events.SelectedNode, this._selectionChanged, this);
    this._constructorsWidget = this._constructorsDataGrid.asWidget();
    this._constructorsWidget.setMinimumSize(50, 25);

    this._diffDataGrid = new Profiler.HeapSnapshotDiffDataGrid(this);
    this._diffDataGrid.addEventListener(DataGrid.DataGrid.Events.SelectedNode, this._selectionChanged, this);
    this._diffWidget = this._diffDataGrid.asWidget();
    this._diffWidget.setMinimumSize(50, 25);

    if (isHeapTimeline) {
      this._allocationDataGrid = new Profiler.AllocationDataGrid(profile.heapProfilerModel(), this);
      this._allocationDataGrid.addEventListener(
          DataGrid.DataGrid.Events.SelectedNode, this._onSelectAllocationNode, this);
      this._allocationWidget = this._allocationDataGrid.asWidget();
      this._allocationWidget.setMinimumSize(50, 25);

      this._allocationStackView = new Profiler.HeapAllocationStackView(profile.heapProfilerModel());
      this._allocationStackView.setMinimumSize(50, 25);

      this._tabbedPane = new UI.TabbedPane();
    }

    this._retainmentDataGrid = new Profiler.HeapSnapshotRetainmentDataGrid(this);
    this._retainmentWidget = this._retainmentDataGrid.asWidget();
    this._retainmentWidget.setMinimumSize(50, 21);
    this._retainmentWidget.element.classList.add('retaining-paths-view');

    let splitWidgetResizer;
    if (this._allocationStackView) {
      this._tabbedPane = new UI.TabbedPane();

      this._tabbedPane.appendTab('retainers', Common.UIString('Retainers'), this._retainmentWidget);
      this._tabbedPane.appendTab('allocation-stack', Common.UIString('Allocation stack'), this._allocationStackView);

      splitWidgetResizer = this._tabbedPane.headerElement();
      this._objectDetailsView = this._tabbedPane;
    } else {
      const retainmentViewHeader = createElementWithClass('div', 'heap-snapshot-view-resizer');
      const retainingPathsTitleDiv = retainmentViewHeader.createChild('div', 'title');
      const retainingPathsTitle = retainingPathsTitleDiv.createChild('span');
      retainingPathsTitle.textContent = Common.UIString('Retainers');

      splitWidgetResizer = retainmentViewHeader;
      this._objectDetailsView = new UI.VBox();
      this._objectDetailsView.element.appendChild(retainmentViewHeader);
      this._retainmentWidget.show(this._objectDetailsView.element);
    }
    this._splitWidget.hideDefaultResizer();
    this._splitWidget.installResizer(splitWidgetResizer);

    this._retainmentDataGrid.addEventListener(
        DataGrid.DataGrid.Events.SelectedNode, this._inspectedObjectChanged, this);
    this._retainmentDataGrid.reset();

    this._perspectives = [];
    this._comparisonPerspective = new Profiler.HeapSnapshotView.ComparisonPerspective();
    this._perspectives.push(new Profiler.HeapSnapshotView.SummaryPerspective());
    if (profile.profileType() !== Profiler.ProfileTypeRegistry.instance.trackingHeapSnapshotProfileType)
      this._perspectives.push(this._comparisonPerspective);
    this._perspectives.push(new Profiler.HeapSnapshotView.ContainmentPerspective());
    if (this._allocationWidget)
      this._perspectives.push(new Profiler.HeapSnapshotView.AllocationPerspective());
    this._perspectives.push(new Profiler.HeapSnapshotView.StatisticsPerspective());

    this._perspectiveSelect = new UI.ToolbarComboBox(this._onSelectedPerspectiveChanged.bind(this));
    this._updatePerspectiveOptions();

    this._baseSelect = new UI.ToolbarComboBox(this._changeBase.bind(this));
    this._baseSelect.setVisible(false);
    this._updateBaseOptions();

    this._filterSelect = new UI.ToolbarComboBox(this._changeFilter.bind(this));
    this._filterSelect.setVisible(false);
    this._updateFilterOptions();

    this._classNameFilter = new UI.ToolbarInput('Class filter');
    this._classNameFilter.setVisible(false);
    this._constructorsDataGrid.setNameFilter(this._classNameFilter);
    this._diffDataGrid.setNameFilter(this._classNameFilter);

    this._selectedSizeText = new UI.ToolbarText();

    this._popoverHelper = new UI.PopoverHelper(this.element, this._getPopoverRequest.bind(this));
    this._popoverHelper.setDisableOnClick(true);
    this._popoverHelper.setHasPadding(true);
    this.element.addEventListener('scroll', this._popoverHelper.hidePopover.bind(this._popoverHelper), true);

    this._currentPerspectiveIndex = 0;
    this._currentPerspective = this._perspectives[0];
    this._currentPerspective.activate(this);
    this._dataGrid = this._currentPerspective.masterGrid(this);

    this._populate();
    this._searchThrottler = new Common.Throttler(0);

    this.element.addEventListener('contextmenu', this._handleContextMenuEvent.bind(this), true);

    for (const existingProfile of this._profiles())
      existingProfile.addEventListener(Profiler.ProfileHeader.Events.ProfileTitleChanged, this._updateControls, this);
  }

  _createOverview() {
    const profileType = this._profile.profileType();
    this._trackingOverviewGrid = new Profiler.HeapTimelineOverview();
    this._trackingOverviewGrid.addEventListener(
        Profiler.HeapTimelineOverview.IdsRangeChanged, this._onIdsRangeChanged.bind(this));
    if (!this._profile.fromFile() && profileType.profileBeingRecorded() === this._profile) {
      profileType.addEventListener(
          Profiler.TrackingHeapSnapshotProfileType.HeapStatsUpdate, this._onHeapStatsUpdate, this);
      profileType.addEventListener(
          Profiler.TrackingHeapSnapshotProfileType.TrackingStopped, this._onStopTracking, this);
    }
  }

  _onStopTracking() {
    this._profile.profileType().removeEventListener(
        Profiler.TrackingHeapSnapshotProfileType.HeapStatsUpdate, this._onHeapStatsUpdate, this);
    this._profile.profileType().removeEventListener(
        Profiler.TrackingHeapSnapshotProfileType.TrackingStopped, this._onStopTracking, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _onHeapStatsUpdate(event) {
    const samples = event.data;
    if (samples)
      this._trackingOverviewGrid.setSamples(event.data);
  }

  /**
   * @return {!UI.SearchableView}
   */
  searchableView() {
    return this._searchableView;
  }

  /**
   * @override
   * @param {?Profiler.ProfileHeader} profile
   * @return {?UI.Widget}
   */
  showProfile(profile) {
    return this._parentDataDisplayDelegate.showProfile(profile);
  }

  /**
   * @override
   * @param {!Protocol.HeapProfiler.HeapSnapshotObjectId} snapshotObjectId
   * @param {string} perspectiveName
   */
  showObject(snapshotObjectId, perspectiveName) {
    if (snapshotObjectId <= this._profile.maxJSObjectId)
      this.selectLiveObject(perspectiveName, snapshotObjectId);
    else
      this._parentDataDisplayDelegate.showObject(snapshotObjectId, perspectiveName);
  }

  /**
   * @override
   * @param {number} nodeIndex
   * @return {!Promise<?Element>}
   */
  async linkifyObject(nodeIndex) {
    const heapProfilerModel = this._profile.heapProfilerModel();
    // heapProfilerModel is null if snapshot was loaded from file
    if (!heapProfilerModel)
      return null;
    const location = await this._profile.getLocation(nodeIndex);
    if (!location)
      return null;
    const debuggerModel = heapProfilerModel.runtimeModel().debuggerModel();
    const rawLocation = debuggerModel.createRawLocationByScriptId(
        String(location.scriptId), location.lineNumber, location.columnNumber);
    if (!rawLocation)
      return null;
    const sourceURL = rawLocation.script() && rawLocation.script().sourceURL;
    return sourceURL && this._linkifier ? this._linkifier.linkifyRawLocation(rawLocation, sourceURL) : null;
  }

  async _populate() {
    const heapSnapshotProxy = await this._profile._loadPromise;

    this._retrieveStatistics(heapSnapshotProxy);
    this._dataGrid.setDataSource(heapSnapshotProxy);

    if (this._profile.profileType().id === Profiler.TrackingHeapSnapshotProfileType.TypeId &&
        this._profile.fromFile()) {
      const samples = await heapSnapshotProxy.getSamples();
      if (samples) {
        console.assert(samples.timestamps.length);
        const profileSamples = new Profiler.HeapTimelineOverview.Samples();
        profileSamples.sizes = samples.sizes;
        profileSamples.ids = samples.lastAssignedIds;
        profileSamples.timestamps = samples.timestamps;
        profileSamples.max = samples.sizes;
        profileSamples.totalTime = Math.max(samples.timestamps.peekLast(), 10000);
        this._trackingOverviewGrid.setSamples(profileSamples);
      }
    }

    const list = this._profiles();
    const profileIndex = list.indexOf(this._profile);
    this._baseSelect.setSelectedIndex(Math.max(0, profileIndex - 1));
    if (this._trackingOverviewGrid)
      this._trackingOverviewGrid.updateGrid();
  }

  /**
   * @param {!Profiler.HeapSnapshotProxy} heapSnapshotProxy
   * @return {!Promise<!HeapSnapshotModel.Statistics>}
   */
  async _retrieveStatistics(heapSnapshotProxy) {
    const statistics = await heapSnapshotProxy.getStatistics();
    this._statisticsView.setTotal(statistics.total);
    this._statisticsView.addRecord(statistics.code, Common.UIString('Code'), '#f77');
    this._statisticsView.addRecord(statistics.strings, Common.UIString('Strings'), '#5e5');
    this._statisticsView.addRecord(statistics.jsArrays, Common.UIString('JS Arrays'), '#7af');
    this._statisticsView.addRecord(statistics.native, Common.UIString('Typed Arrays'), '#fc5');
    this._statisticsView.addRecord(statistics.system, Common.UIString('System Objects'), '#98f');
    this._statisticsView.addRecord(statistics.total, Common.UIString('Total'));
    return statistics;
  }

  /**
   * @param {!Common.Event} event
   */
  _onIdsRangeChanged(event) {
    const minId = event.data.minId;
    const maxId = event.data.maxId;
    this._selectedSizeText.setText(Common.UIString('Selected size: %s', Number.bytesToString(event.data.size)));
    if (this._constructorsDataGrid.snapshot)
      this._constructorsDataGrid.setSelectionRange(minId, maxId);
  }

  /**
   * @override
   * @return {!Array<!UI.ToolbarItem>}
   */
  syncToolbarItems() {
    const result = [this._perspectiveSelect, this._classNameFilter];
    if (this._profile.profileType() !== Profiler.ProfileTypeRegistry.instance.trackingHeapSnapshotProfileType)
      result.push(this._baseSelect, this._filterSelect);
    result.push(this._selectedSizeText);
    return result;
  }

  /**
   * @override
   */
  willHide() {
    this._currentSearchResultIndex = -1;
    this._popoverHelper.hidePopover();
  }

  /**
   * @override
   * @return {boolean}
   */
  supportsCaseSensitiveSearch() {
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  supportsRegexSearch() {
    return false;
  }

  /**
   * @override
   */
  searchCanceled() {
    this._currentSearchResultIndex = -1;
    this._searchResults = [];
  }

  /**
   * @param {?Profiler.HeapSnapshotGridNode} node
   */
  _selectRevealedNode(node) {
    if (node)
      node.select();
  }

  /**
   * @param {!Event} event
   */
  _handleContextMenuEvent(event) {
    const contextMenu = new UI.ContextMenu(event);
    if (this._dataGrid)
      this._dataGrid.populateContextMenu(contextMenu, event);
    contextMenu.show();
  }

  /**
   * @override
   * @param {!UI.SearchableView.SearchConfig} searchConfig
   * @param {boolean} shouldJump
   * @param {boolean=} jumpBackwards
   */
  performSearch(searchConfig, shouldJump, jumpBackwards) {
    const nextQuery = new HeapSnapshotModel.SearchConfig(
        searchConfig.query.trim(), searchConfig.caseSensitive, searchConfig.isRegex, shouldJump,
        jumpBackwards || false);

    this._searchThrottler.schedule(this._performSearch.bind(this, nextQuery));
  }

  /**
   * @param {!HeapSnapshotModel.SearchConfig} nextQuery
   * @return {!Promise}
   */
  async _performSearch(nextQuery) {
    // Call searchCanceled since it will reset everything we need before doing a new search.
    this.searchCanceled();

    if (!this._currentPerspective.supportsSearch())
      return;

    this.currentQuery = nextQuery;
    const query = nextQuery.query.trim();

    if (!query)
      return;

    if (query.charAt(0) === '@') {
      const snapshotNodeId = parseInt(query.substring(1), 10);
      if (isNaN(snapshotNodeId))
        return;
      const node = await this._dataGrid.revealObjectByHeapSnapshotId(String(snapshotNodeId));
      this._selectRevealedNode(node);
      return;
    }

    this._searchResults = await this._profile._snapshotProxy.search(this.currentQuery, this._dataGrid.nodeFilter());

    this._searchableView.updateSearchMatchesCount(this._searchResults.length);
    if (this._searchResults.length)
      this._currentSearchResultIndex = nextQuery.jumpBackwards ? this._searchResults.length - 1 : 0;
    await this._jumpToSearchResult(this._currentSearchResultIndex);
  }

  /**
   * @override
   */
  jumpToNextSearchResult() {
    if (!this._searchResults.length)
      return;
    this._currentSearchResultIndex = (this._currentSearchResultIndex + 1) % this._searchResults.length;
    this._searchThrottler.schedule(this._jumpToSearchResult.bind(this, this._currentSearchResultIndex));
  }

  /**
   * @override
   */
  jumpToPreviousSearchResult() {
    if (!this._searchResults.length)
      return;
    this._currentSearchResultIndex =
        (this._currentSearchResultIndex + this._searchResults.length - 1) % this._searchResults.length;
    this._searchThrottler.schedule(this._jumpToSearchResult.bind(this, this._currentSearchResultIndex));
  }

  /**
   * @param {number} searchResultIndex
   * @return {!Promise}
   */
  async _jumpToSearchResult(searchResultIndex) {
    this._searchableView.updateCurrentMatchIndex(searchResultIndex);
    if (searchResultIndex === -1)
      return;
    const node = await this._dataGrid.revealObjectByHeapSnapshotId(String(this._searchResults[searchResultIndex]));
    this._selectRevealedNode(node);
  }

  refreshVisibleData() {
    if (!this._dataGrid)
      return;
    let child = this._dataGrid.rootNode().children[0];
    while (child) {
      child.refresh();
      child = child.traverseNextNode(false, null, true);
    }
  }

  _changeBase() {
    if (this._baseProfile === this._profiles()[this._baseSelect.selectedIndex()])
      return;

    this._baseProfile = this._profiles()[this._baseSelect.selectedIndex()];
    const dataGrid = /** @type {!Profiler.HeapSnapshotDiffDataGrid} */ (this._dataGrid);
    // Change set base data source only if main data source is already set.
    if (dataGrid.snapshot)
      this._baseProfile._loadPromise.then(dataGrid.setBaseDataSource.bind(dataGrid));

    if (!this.currentQuery || !this._searchResults)
      return;

    // The current search needs to be performed again. First negate out previous match
    // count by calling the search finished callback with a negative number of matches.
    // Then perform the search again with the same query and callback.
    this.performSearch(this.currentQuery, false);
  }

  _changeFilter() {
    const profileIndex = this._filterSelect.selectedIndex() - 1;
    this._dataGrid.filterSelectIndexChanged(this._profiles(), profileIndex);

    if (!this.currentQuery || !this._searchResults)
      return;

    // The current search needs to be performed again. First negate out previous match
    // count by calling the search finished callback with a negative number of matches.
    // Then perform the search again with the same query and callback.
    this.performSearch(this.currentQuery, false);
  }

  /**
   * @return {!Array.<!Profiler.ProfileHeader>}
   */
  _profiles() {
    return this._profile.profileType().getProfiles();
  }

  /**
   * @param {!Common.Event} event
   */
  _selectionChanged(event) {
    const selectedNode = /** @type {!Profiler.HeapSnapshotGridNode} */ (event.data);
    this._setSelectedNodeForDetailsView(selectedNode);
    this._inspectedObjectChanged(event);
  }

  /**
   * @param {!Common.Event} event
   */
  _onSelectAllocationNode(event) {
    const selectedNode = /** @type {!DataGrid.DataGridNode} */ (event.data);
    this._constructorsDataGrid.setAllocationNodeId(selectedNode.allocationNodeId());
    this._setSelectedNodeForDetailsView(null);
  }

  /**
   * @param {!Common.Event} event
   */
  _inspectedObjectChanged(event) {
    const selectedNode = /** @type {!DataGrid.DataGridNode} */ (event.data);
    const heapProfilerModel = this._profile.heapProfilerModel();
    if (heapProfilerModel && selectedNode instanceof Profiler.HeapSnapshotGenericObjectNode)
      heapProfilerModel.addInspectedHeapObject(String(selectedNode.snapshotNodeId));
  }

  /**
   * @param {?Profiler.HeapSnapshotGridNode} nodeItem
   */
  _setSelectedNodeForDetailsView(nodeItem) {
    const dataSource = nodeItem && nodeItem.retainersDataSource();
    if (dataSource) {
      this._retainmentDataGrid.setDataSource(dataSource.snapshot, dataSource.snapshotNodeIndex);
      if (this._allocationStackView)
        this._allocationStackView.setAllocatedObject(dataSource.snapshot, dataSource.snapshotNodeIndex);
    } else {
      if (this._allocationStackView)
        this._allocationStackView.clear();
      this._retainmentDataGrid.reset();
    }
  }

  /**
   * @param {string} perspectiveTitle
   * @return {!Promise}
   */
  _changePerspectiveAndWait(perspectiveTitle) {
    const perspectiveIndex = this._perspectives.findIndex(perspective => perspective.title() === perspectiveTitle);
    if (perspectiveIndex === -1 || this._currentPerspectiveIndex === perspectiveIndex)
      return Promise.resolve();

    const promise = this._perspectives[perspectiveIndex].masterGrid(this).once(
        Profiler.HeapSnapshotSortableDataGrid.Events.ContentShown);

    const option = this._perspectiveSelect.options().find(option => option.value === perspectiveIndex);
    this._perspectiveSelect.select(/** @type {!Element} */ (option));
    this._changePerspective(perspectiveIndex);
    return promise;
  }

  async _updateDataSourceAndView() {
    const dataGrid = this._dataGrid;
    if (!dataGrid || dataGrid.snapshot)
      return;

    const snapshotProxy = await this._profile._loadPromise;

    if (this._dataGrid !== dataGrid)
      return;
    if (dataGrid.snapshot !== snapshotProxy)
      dataGrid.setDataSource(snapshotProxy);
    if (dataGrid !== this._diffDataGrid)
      return;
    if (!this._baseProfile)
      this._baseProfile = this._profiles()[this._baseSelect.selectedIndex()];

    const baseSnapshotProxy = await this._baseProfile._loadPromise;

    if (this._diffDataGrid.baseSnapshot !== baseSnapshotProxy)
      this._diffDataGrid.setBaseDataSource(baseSnapshotProxy);
  }

  /**
   * @param {!Event} event
   */
  _onSelectedPerspectiveChanged(event) {
    this._changePerspective(event.target.selectedOptions[0].value);
  }

  /**
   * @param {number} selectedIndex
   */
  _changePerspective(selectedIndex) {
    if (selectedIndex === this._currentPerspectiveIndex)
      return;

    this._currentPerspectiveIndex = selectedIndex;

    this._currentPerspective.deactivate(this);
    const perspective = this._perspectives[selectedIndex];
    this._currentPerspective = perspective;
    this._dataGrid = perspective.masterGrid(this);
    perspective.activate(this);

    this.refreshVisibleData();
    if (this._dataGrid)
      this._dataGrid.updateWidths();

    this._updateDataSourceAndView();

    if (!this.currentQuery || !this._searchResults)
      return;

    // The current search needs to be performed again. First negate out previous match
    // count by calling the search finished callback with a negative number of matches.
    // Then perform the search again the with same query and callback.
    this.performSearch(this.currentQuery, false);
  }

  /**
   * @param {string} perspectiveName
   * @param {!Protocol.HeapProfiler.HeapSnapshotObjectId} snapshotObjectId
   */
  async selectLiveObject(perspectiveName, snapshotObjectId) {
    await this._changePerspectiveAndWait(perspectiveName);
    const node = await this._dataGrid.revealObjectByHeapSnapshotId(snapshotObjectId);
    if (node)
      node.select();
    else
      Common.console.error('Cannot find corresponding heap snapshot node');
  }

  /**
   * @param {!Event} event
   * @return {?UI.PopoverRequest}
   */
  _getPopoverRequest(event) {
    const span = event.target.enclosingNodeOrSelfWithNodeName('span');
    const row = event.target.enclosingNodeOrSelfWithNodeName('tr');
    const heapProfilerModel = this._profile.heapProfilerModel();
    if (!row || !span || !heapProfilerModel)
      return null;
    const node = row._dataGridNode;
    let objectPopoverHelper;
    return {
      box: span.boxInWindow(),
      show: async popover => {
        const remoteObject = await node.queryObjectContent(heapProfilerModel, 'popover');
        if (!remoteObject)
          return false;
        objectPopoverHelper = await ObjectUI.ObjectPopoverHelper.buildObjectPopover(remoteObject, popover);
        if (!objectPopoverHelper) {
          heapProfilerModel.runtimeModel().releaseObjectGroup('popover');
          return false;
        }
        return true;
      },
      hide: () => {
        heapProfilerModel.runtimeModel().releaseObjectGroup('popover');
        objectPopoverHelper.dispose();
      }
    };
  }

  _updatePerspectiveOptions() {
    const multipleSnapshots = this._profiles().length > 1;
    this._perspectiveSelect.removeOptions();
    this._perspectives.forEach((perspective, index) => {
      if (multipleSnapshots || perspective !== this._comparisonPerspective)
        this._perspectiveSelect.createOption(perspective.title(), '', String(index));
    });
  }

  _updateBaseOptions() {
    const list = this._profiles();
    const selectedIndex = this._baseSelect.selectedIndex();

    this._baseSelect.removeOptions();
    for (const item of list)
      this._baseSelect.createOption(item.title);

    if (selectedIndex > -1)
      this._baseSelect.setSelectedIndex(selectedIndex);
  }

  _updateFilterOptions() {
    const list = this._profiles();
    const selectedIndex = this._filterSelect.selectedIndex();

    this._filterSelect.removeOptions();
    this._filterSelect.createOption(Common.UIString('All objects'));
    for (let i = 0; i < list.length; ++i) {
      let title;
      if (!i)
        title = Common.UIString('Objects allocated before %s', list[i].title);
      else
        title = Common.UIString('Objects allocated between %s and %s', list[i - 1].title, list[i].title);
      this._filterSelect.createOption(title);
    }

    if (selectedIndex > -1)
      this._filterSelect.setSelectedIndex(selectedIndex);
  }

  _updateControls() {
    this._updatePerspectiveOptions();
    this._updateBaseOptions();
    this._updateFilterOptions();
  }

  /**
   * @param {!Common.Event} event
   */
  _onReceiveSnapshot(event) {
    this._updateControls();
    const profile = event.data;
    profile.addEventListener(Profiler.ProfileHeader.Events.ProfileTitleChanged, this._updateControls, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _onProfileHeaderRemoved(event) {
    const profile = event.data;
    profile.removeEventListener(Profiler.ProfileHeader.Events.ProfileTitleChanged, this._updateControls, this);

    if (this._profile === profile) {
      this.detach();
      this._profile.profileType().removeEventListener(
          Profiler.HeapSnapshotProfileType.SnapshotReceived, this._onReceiveSnapshot, this);
      this._profile.profileType().removeEventListener(
          Profiler.ProfileType.Events.RemoveProfileHeader, this._onProfileHeaderRemoved, this);
      this.dispose();
    } else {
      this._updateControls();
    }
  }

  dispose() {
    this._linkifier.dispose();
    this._popoverHelper.dispose();
    if (this._allocationStackView) {
      this._allocationStackView.clear();
      this._allocationDataGrid.dispose();
    }
    this._onStopTracking();
    if (this._trackingOverviewGrid) {
      this._trackingOverviewGrid.removeEventListener(
          Profiler.HeapTimelineOverview.IdsRangeChanged, this._onIdsRangeChanged.bind(this));
    }
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotView.Perspective = class {
  /**
   * @param {string} title
   */
  constructor(title) {
    this._title = title;
  }

  /**
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  activate(heapSnapshotView) {
  }

  /**
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  deactivate(heapSnapshotView) {
    heapSnapshotView._baseSelect.setVisible(false);
    heapSnapshotView._filterSelect.setVisible(false);
    heapSnapshotView._classNameFilter.setVisible(false);
    if (heapSnapshotView._trackingOverviewGrid)
      heapSnapshotView._trackingOverviewGrid.detach();
    if (heapSnapshotView._allocationWidget)
      heapSnapshotView._allocationWidget.detach();
    if (heapSnapshotView._statisticsView)
      heapSnapshotView._statisticsView.detach();

    heapSnapshotView._splitWidget.detach();
    heapSnapshotView._splitWidget.detachChildWidgets();
  }

  /**
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   * @return {?DataGrid.DataGrid}
   */
  masterGrid(heapSnapshotView) {
    return null;
  }

  /**
   * @return {string}
   */
  title() {
    return this._title;
  }

  /**
   * @return {boolean}
   */
  supportsSearch() {
    return false;
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotView.SummaryPerspective = class extends Profiler.HeapSnapshotView.Perspective {
  constructor() {
    super(Common.UIString('Summary'));
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  activate(heapSnapshotView) {
    heapSnapshotView._splitWidget.setMainWidget(heapSnapshotView._constructorsWidget);
    heapSnapshotView._splitWidget.setSidebarWidget(heapSnapshotView._objectDetailsView);
    heapSnapshotView._splitWidget.show(heapSnapshotView._searchableView.element);
    heapSnapshotView._filterSelect.setVisible(true);
    heapSnapshotView._classNameFilter.setVisible(true);
    if (!heapSnapshotView._trackingOverviewGrid)
      return;
    heapSnapshotView._trackingOverviewGrid.show(
        heapSnapshotView._searchableView.element, heapSnapshotView._splitWidget.element);
    heapSnapshotView._trackingOverviewGrid.update();
    heapSnapshotView._trackingOverviewGrid.updateGrid();
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   * @return {?DataGrid.DataGrid}
   */
  masterGrid(heapSnapshotView) {
    return heapSnapshotView._constructorsDataGrid;
  }

  /**
   * @override
   * @return {boolean}
   */
  supportsSearch() {
    return true;
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotView.ComparisonPerspective = class extends Profiler.HeapSnapshotView.Perspective {
  constructor() {
    super(Common.UIString('Comparison'));
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  activate(heapSnapshotView) {
    heapSnapshotView._splitWidget.setMainWidget(heapSnapshotView._diffWidget);
    heapSnapshotView._splitWidget.setSidebarWidget(heapSnapshotView._objectDetailsView);
    heapSnapshotView._splitWidget.show(heapSnapshotView._searchableView.element);
    heapSnapshotView._baseSelect.setVisible(true);
    heapSnapshotView._classNameFilter.setVisible(true);
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   * @return {?DataGrid.DataGrid}
   */
  masterGrid(heapSnapshotView) {
    return heapSnapshotView._diffDataGrid;
  }

  /**
   * @override
   * @return {boolean}
   */
  supportsSearch() {
    return true;
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotView.ContainmentPerspective = class extends Profiler.HeapSnapshotView.Perspective {
  constructor() {
    super(Common.UIString('Containment'));
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  activate(heapSnapshotView) {
    heapSnapshotView._splitWidget.setMainWidget(heapSnapshotView._containmentWidget);
    heapSnapshotView._splitWidget.setSidebarWidget(heapSnapshotView._objectDetailsView);
    heapSnapshotView._splitWidget.show(heapSnapshotView._searchableView.element);
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   * @return {?DataGrid.DataGrid}
   */
  masterGrid(heapSnapshotView) {
    return heapSnapshotView._containmentDataGrid;
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotView.AllocationPerspective = class extends Profiler.HeapSnapshotView.Perspective {
  constructor() {
    super(Common.UIString('Allocation'));
    this._allocationSplitWidget = new UI.SplitWidget(false, true, 'heapSnapshotAllocationSplitViewState', 200, 200);
    this._allocationSplitWidget.setSidebarWidget(new UI.VBox());
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  activate(heapSnapshotView) {
    this._allocationSplitWidget.setMainWidget(heapSnapshotView._allocationWidget);
    heapSnapshotView._splitWidget.setMainWidget(heapSnapshotView._constructorsWidget);
    heapSnapshotView._splitWidget.setSidebarWidget(heapSnapshotView._objectDetailsView);

    const allocatedObjectsView = new UI.VBox();
    const resizer = createElementWithClass('div', 'heap-snapshot-view-resizer');
    const title = resizer.createChild('div', 'title').createChild('span');
    title.textContent = Common.UIString('Live objects');
    this._allocationSplitWidget.hideDefaultResizer();
    this._allocationSplitWidget.installResizer(resizer);
    allocatedObjectsView.element.appendChild(resizer);
    heapSnapshotView._splitWidget.show(allocatedObjectsView.element);
    this._allocationSplitWidget.setSidebarWidget(allocatedObjectsView);

    this._allocationSplitWidget.show(heapSnapshotView._searchableView.element);

    heapSnapshotView._constructorsDataGrid.clear();
    const selectedNode = heapSnapshotView._allocationDataGrid.selectedNode;
    if (selectedNode)
      heapSnapshotView._constructorsDataGrid.setAllocationNodeId(selectedNode.allocationNodeId());
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  deactivate(heapSnapshotView) {
    this._allocationSplitWidget.detach();
    super.deactivate(heapSnapshotView);
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   * @return {?DataGrid.DataGrid}
   */
  masterGrid(heapSnapshotView) {
    return heapSnapshotView._allocationDataGrid;
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotView.StatisticsPerspective = class extends Profiler.HeapSnapshotView.Perspective {
  constructor() {
    super(Common.UIString('Statistics'));
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   */
  activate(heapSnapshotView) {
    heapSnapshotView._statisticsView.show(heapSnapshotView._searchableView.element);
  }

  /**
   * @override
   * @param {!Profiler.HeapSnapshotView} heapSnapshotView
   * @return {?DataGrid.DataGrid}
   */
  masterGrid(heapSnapshotView) {
    return null;
  }
};

/**
 * @implements {SDK.SDKModelObserver<!SDK.HeapProfilerModel>}
 * @unrestricted
 */
Profiler.HeapSnapshotProfileType = class extends Profiler.ProfileType {
  /**
   * @param {string=} id
   * @param {string=} title
   */
  constructor(id, title) {
    super(id || Profiler.HeapSnapshotProfileType.TypeId, title || ls`Heap snapshot`);
    SDK.targetManager.observeModels(SDK.HeapProfilerModel, this);
    SDK.targetManager.addModelListener(
        SDK.HeapProfilerModel, SDK.HeapProfilerModel.Events.ResetProfiles, this._resetProfiles, this);
    SDK.targetManager.addModelListener(
        SDK.HeapProfilerModel, SDK.HeapProfilerModel.Events.AddHeapSnapshotChunk, this._addHeapSnapshotChunk, this);
    SDK.targetManager.addModelListener(
        SDK.HeapProfilerModel, SDK.HeapProfilerModel.Events.ReportHeapSnapshotProgress,
        this._reportHeapSnapshotProgress, this);
  }

  /**
   * @override
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   */
  modelAdded(heapProfilerModel) {
    heapProfilerModel.enable();
  }

  /**
   * @override
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   */
  modelRemoved(heapProfilerModel) {
  }

  /**
   * @override
   * @return {!Array<!Profiler.HeapProfileHeader>}
   */
  getProfiles() {
    return /** @type {!Array<!Profiler.HeapProfileHeader>} */ (super.getProfiles());
  }

  /**
   * @override
   * @return {string}
   */
  fileExtension() {
    return '.heapsnapshot';
  }

  get buttonTooltip() {
    return Common.UIString('Take heap snapshot');
  }

  /**
   * @override
   * @return {boolean}
   */
  isInstantProfile() {
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  buttonClicked() {
    this._takeHeapSnapshot();
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.ProfilesHeapProfileTaken);
    return false;
  }

  get treeItemTitle() {
    return Common.UIString('HEAP SNAPSHOTS');
  }

  get description() {
    return Common.UIString(
        'Heap snapshot profiles show memory distribution among your page\'s JavaScript objects and related DOM nodes.');
  }

  /**
   * @override
   * @param {string} title
   * @return {!Profiler.ProfileHeader}
   */
  createProfileLoadedFromFile(title) {
    return new Profiler.HeapProfileHeader(null, this, title);
  }

  async _takeHeapSnapshot() {
    if (this.profileBeingRecorded())
      return;
    const heapProfilerModel = UI.context.flavor(SDK.HeapProfilerModel);
    if (!heapProfilerModel)
      return;

    let profile = new Profiler.HeapProfileHeader(heapProfilerModel, this);
    this.setProfileBeingRecorded(profile);
    this.addProfile(profile);
    profile.updateStatus(Common.UIString('Snapshotting\u2026'));

    await heapProfilerModel.takeHeapSnapshot(true);
    // ------------ ASYNC ------------
    profile = this.profileBeingRecorded();
    profile.title = Common.UIString('Snapshot %d', profile.uid);
    profile._finishLoad();
    this.setProfileBeingRecorded(null);
    this.dispatchEventToListeners(Profiler.ProfileType.Events.ProfileComplete, profile);
  }

  /**
   * @param {!Common.Event} event
   */
  _addHeapSnapshotChunk(event) {
    if (!this.profileBeingRecorded())
      return;
    const chunk = /** @type {string} */ (event.data);
    this.profileBeingRecorded().transferChunk(chunk);
  }

  /**
   * @param {!Common.Event} event
   */
  _reportHeapSnapshotProgress(event) {
    const profile = this.profileBeingRecorded();
    if (!profile)
      return;
    const data = /** @type {{done: number, total: number, finished: boolean}} */ (event.data);
    profile.updateStatus(Common.UIString('%.0f%%', (data.done / data.total) * 100), true);
    if (data.finished)
      profile._prepareToLoad();
  }

  /**
   * @param {!Common.Event} event
   */
  _resetProfiles(event) {
    const heapProfilerModel = /** @type {!SDK.HeapProfilerModel} */ (event.data);
    for (const profile of this.getProfiles()) {
      if (profile.heapProfilerModel() === heapProfilerModel)
        this.removeProfile(profile);
    }
  }

  _snapshotReceived(profile) {
    if (this.profileBeingRecorded() === profile)
      this.setProfileBeingRecorded(null);
    this.dispatchEventToListeners(Profiler.HeapSnapshotProfileType.SnapshotReceived, profile);
  }
};

Profiler.HeapSnapshotProfileType.TypeId = 'HEAP';
Profiler.HeapSnapshotProfileType.SnapshotReceived = 'SnapshotReceived';

/**
 * @unrestricted
 */
Profiler.TrackingHeapSnapshotProfileType = class extends Profiler.HeapSnapshotProfileType {
  constructor() {
    super(Profiler.TrackingHeapSnapshotProfileType.TypeId, ls`Allocation instrumentation on timeline`);
    this._recordAllocationStacksSetting = Common.settings.createSetting('recordAllocationStacks', false);
  }

  /**
   * @override
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   */
  modelAdded(heapProfilerModel) {
    super.modelAdded(heapProfilerModel);
    heapProfilerModel.addEventListener(SDK.HeapProfilerModel.Events.HeapStatsUpdate, this._heapStatsUpdate, this);
    heapProfilerModel.addEventListener(SDK.HeapProfilerModel.Events.LastSeenObjectId, this._lastSeenObjectId, this);
  }

  /**
   * @override
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   */
  modelRemoved(heapProfilerModel) {
    super.modelRemoved(heapProfilerModel);
    heapProfilerModel.removeEventListener(SDK.HeapProfilerModel.Events.HeapStatsUpdate, this._heapStatsUpdate, this);
    heapProfilerModel.removeEventListener(SDK.HeapProfilerModel.Events.LastSeenObjectId, this._lastSeenObjectId, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _heapStatsUpdate(event) {
    if (!this._profileSamples)
      return;
    const samples = /** @type {!Array.<number>} */ (event.data);
    let index;
    for (let i = 0; i < samples.length; i += 3) {
      index = samples[i];
      const size = samples[i + 2];
      this._profileSamples.sizes[index] = size;
      if (!this._profileSamples.max[index])
        this._profileSamples.max[index] = size;
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _lastSeenObjectId(event) {
    const profileSamples = this._profileSamples;
    if (!profileSamples)
      return;
    const data = /** @type {{lastSeenObjectId: number, timestamp: number}} */ (event.data);
    const currentIndex = Math.max(profileSamples.ids.length, profileSamples.max.length - 1);
    profileSamples.ids[currentIndex] = data.lastSeenObjectId;
    if (!profileSamples.max[currentIndex]) {
      profileSamples.max[currentIndex] = 0;
      profileSamples.sizes[currentIndex] = 0;
    }
    profileSamples.timestamps[currentIndex] = data.timestamp;
    if (profileSamples.totalTime < data.timestamp - profileSamples.timestamps[0])
      profileSamples.totalTime *= 2;
    this.dispatchEventToListeners(Profiler.TrackingHeapSnapshotProfileType.HeapStatsUpdate, this._profileSamples);
    this.profileBeingRecorded().updateStatus(null, true);
  }

  /**
   * @override
   * @return {boolean}
   */
  hasTemporaryView() {
    return true;
  }

  get buttonTooltip() {
    return this._recording ? ls`Stop recording heap profile` : ls`Start recording heap profile`;
  }

  /**
   * @override
   * @return {boolean}
   */
  isInstantProfile() {
    return false;
  }

  /**
   * @override
   * @return {boolean}
   */
  buttonClicked() {
    return this._toggleRecording();
  }

  _startRecordingProfile() {
    if (this.profileBeingRecorded())
      return;
    const heapProfilerModel = this._addNewProfile();
    if (!heapProfilerModel)
      return;
    heapProfilerModel.startTrackingHeapObjects(this._recordAllocationStacksSetting.get());
  }

  /**
   * @override
   * @return {?Element}
   */
  customContent() {
    return UI.SettingsUI.createSettingCheckbox(
        ls`Record allocation stacks (extra performance overhead)`, this._recordAllocationStacksSetting, true);
  }

  /**
   * @return {?SDK.HeapProfilerModel}
   */
  _addNewProfile() {
    const heapProfilerModel = UI.context.flavor(SDK.HeapProfilerModel);
    if (!heapProfilerModel)
      return null;
    this.setProfileBeingRecorded(new Profiler.HeapProfileHeader(heapProfilerModel, this, undefined));
    this._profileSamples = new Profiler.HeapTimelineOverview.Samples();
    this.profileBeingRecorded()._profileSamples = this._profileSamples;
    this._recording = true;
    this.addProfile(/** @type {!Profiler.ProfileHeader} */ (this.profileBeingRecorded()));
    this.profileBeingRecorded().updateStatus(Common.UIString('Recording\u2026'));
    this.dispatchEventToListeners(Profiler.TrackingHeapSnapshotProfileType.TrackingStarted);
    return heapProfilerModel;
  }

  async _stopRecordingProfile() {
    this.profileBeingRecorded().updateStatus(Common.UIString('Snapshotting\u2026'));
    const stopPromise = this.profileBeingRecorded().heapProfilerModel().stopTrackingHeapObjects(true);
    this._recording = false;
    this.dispatchEventToListeners(Profiler.TrackingHeapSnapshotProfileType.TrackingStopped);
    await stopPromise;
    // ------------ ASYNC ------------
    const profile = this.profileBeingRecorded();
    if (!profile)
      return;
    profile._finishLoad();
    this._profileSamples = null;
    this.setProfileBeingRecorded(null);
    this.dispatchEventToListeners(Profiler.ProfileType.Events.ProfileComplete, profile);
  }

  _toggleRecording() {
    if (this._recording)
      this._stopRecordingProfile();
    else
      this._startRecordingProfile();
    return this._recording;
  }

  /**
   * @override
   * @return {string}
   */
  fileExtension() {
    return '.heaptimeline';
  }

  get treeItemTitle() {
    return ls`ALLOCATION TIMELINES`;
  }

  get description() {
    return ls`
        Allocation timelines show instrumented JavaScript memory allocations over time.
        Once profile is recorded you can select a time interval to see objects that
        were allocated within it and still alive by the end of recording.
        Use this profile type to isolate memory leaks.`;
  }

  /**
   * @override
   * @param {!Common.Event} event
   */
  _resetProfiles(event) {
    const wasRecording = this._recording;
    // Clear current profile to avoid stopping backend.
    this.setProfileBeingRecorded(null);
    super._resetProfiles(event);
    this._profileSamples = null;
    if (wasRecording)
      this._addNewProfile();
  }

  /**
   * @override
   */
  profileBeingRecordedRemoved() {
    this._stopRecordingProfile();
    this._profileSamples = null;
  }
};

Profiler.TrackingHeapSnapshotProfileType.TypeId = 'HEAP-RECORD';

Profiler.TrackingHeapSnapshotProfileType.HeapStatsUpdate = 'HeapStatsUpdate';
Profiler.TrackingHeapSnapshotProfileType.TrackingStarted = 'TrackingStarted';
Profiler.TrackingHeapSnapshotProfileType.TrackingStopped = 'TrackingStopped';

/**
 * @unrestricted
 */
Profiler.HeapProfileHeader = class extends Profiler.ProfileHeader {
  /**
   * @param {?SDK.HeapProfilerModel} heapProfilerModel
   * @param {!Profiler.HeapSnapshotProfileType} type
   * @param {string=} title
   */
  constructor(heapProfilerModel, type, title) {
    super(type, title || Common.UIString('Snapshot %d', type.nextProfileUid()));
    this._heapProfilerModel = heapProfilerModel;
    this.maxJSObjectId = -1;
    /** @type {?Profiler.HeapSnapshotWorkerProxy} */
    this._workerProxy = null;
    /** @type {?Common.OutputStream} */
    this._receiver = null;
    /** @type {?Profiler.HeapSnapshotProxy} */
    this._snapshotProxy = null;
    /** @type {!Promise<!Profiler.HeapSnapshotProxy>} */
    this._loadPromise = new Promise(resolve => this._fulfillLoad = resolve);
    this._totalNumberOfChunks = 0;
    this._bufferedWriter = null;
    /** @type {?Bindings.TempFile} */
    this._tempFile = null;
  }

  /**
   * @return {?SDK.HeapProfilerModel}
   */
  heapProfilerModel() {
    return this._heapProfilerModel;
  }

  /**
   * @param {number} nodeIndex
   * @return {!Promise<?HeapSnapshotModel.Location>}
   */
  getLocation(nodeIndex) {
    return this._snapshotProxy.getLocation(nodeIndex);
  }

  /**
   * @override
   * @param {!Profiler.ProfileType.DataDisplayDelegate} dataDisplayDelegate
   * @return {!Profiler.ProfileSidebarTreeElement}
   */
  createSidebarTreeElement(dataDisplayDelegate) {
    return new Profiler.ProfileSidebarTreeElement(dataDisplayDelegate, this, 'heap-snapshot-sidebar-tree-item');
  }

  /**
   * @override
   * @param {!Profiler.ProfileType.DataDisplayDelegate} dataDisplayDelegate
   * @return {!Profiler.HeapSnapshotView}
   */
  createView(dataDisplayDelegate) {
    return new Profiler.HeapSnapshotView(dataDisplayDelegate, this);
  }

  _prepareToLoad() {
    console.assert(!this._receiver, 'Already loading');
    this._setupWorker();
    this.updateStatus(Common.UIString('Loading\u2026'), true);
  }

  _finishLoad() {
    if (!this._wasDisposed)
      this._receiver.close();
    if (!this._bufferedWriter)
      return;
    this._didWriteToTempFile(this._bufferedWriter);
  }

  /**
   * @param {!Bindings.TempFile} tempFile
   */
  _didWriteToTempFile(tempFile) {
    if (this._wasDisposed) {
      if (tempFile)
        tempFile.remove();
      return;
    }
    this._tempFile = tempFile;
    if (!tempFile)
      this._failedToCreateTempFile = true;
    if (this._onTempFileReady) {
      this._onTempFileReady();
      this._onTempFileReady = null;
    }
  }

  _setupWorker() {
    /**
     * @this {Profiler.HeapProfileHeader}
     */
    function setProfileWait(event) {
      this.updateStatus(null, event.data);
    }
    console.assert(!this._workerProxy, 'HeapSnapshotWorkerProxy already exists');
    this._workerProxy = new Profiler.HeapSnapshotWorkerProxy(this._handleWorkerEvent.bind(this));
    this._workerProxy.addEventListener(Profiler.HeapSnapshotWorkerProxy.Events.Wait, setProfileWait, this);
    this._receiver = this._workerProxy.createLoader(this.uid, this._snapshotReceived.bind(this));
  }

  /**
   * @param {string} eventName
   * @param {*} data
   */
  _handleWorkerEvent(eventName, data) {
    if (HeapSnapshotModel.HeapSnapshotProgressEvent.BrokenSnapshot === eventName) {
      const error = /** @type {string} */ (data);
      Common.console.error(error);
      return;
    }

    if (HeapSnapshotModel.HeapSnapshotProgressEvent.Update !== eventName)
      return;
    const subtitle = /** @type {string} */ (data);
    this.updateStatus(subtitle);
  }

  /**
   * @override
   */
  dispose() {
    if (this._workerProxy)
      this._workerProxy.dispose();
    this.removeTempFile();
    this._wasDisposed = true;
  }

  _didCompleteSnapshotTransfer() {
    if (!this._snapshotProxy)
      return;
    this.updateStatus(Number.bytesToString(this._snapshotProxy.totalSize), false);
  }

  /**
   * @param {string} chunk
   */
  transferChunk(chunk) {
    if (!this._bufferedWriter)
      this._bufferedWriter = new Bindings.TempFile();
    this._bufferedWriter.write([chunk]);

    ++this._totalNumberOfChunks;
    this._receiver.write(chunk);
  }

  _snapshotReceived(snapshotProxy) {
    if (this._wasDisposed)
      return;
    this._receiver = null;
    this._snapshotProxy = snapshotProxy;
    this.maxJSObjectId = snapshotProxy.maxJSObjectId();
    this._didCompleteSnapshotTransfer();
    this._workerProxy.startCheckingForLongRunningCalls();
    this.notifySnapshotReceived();
  }

  notifySnapshotReceived() {
    this._fulfillLoad(this._snapshotProxy);
    this.profileType()._snapshotReceived(this);
    if (this.canSaveToFile())
      this.dispatchEventToListeners(Profiler.ProfileHeader.Events.ProfileReceived);
  }

  /**
   * @override
   * @return {boolean}
   */
  canSaveToFile() {
    return !this.fromFile() && !!this._snapshotProxy;
  }

  /**
   * @override
   */
  saveToFile() {
    const fileOutputStream = new Bindings.FileOutputStream();
    this._fileName = this._fileName || 'Heap-' + new Date().toISO8601Compact() + this.profileType().fileExtension();
    fileOutputStream.open(this._fileName).then(onOpen.bind(this));

    /**
     * @param {boolean} accepted
     * @this {Profiler.HeapProfileHeader}
     */
    async function onOpen(accepted) {
      if (!accepted)
        return;
      if (this._failedToCreateTempFile) {
        Common.console.error('Failed to open temp file with heap snapshot');
        fileOutputStream.close();
        return;
      }
      if (this._tempFile) {
        const error = await this._tempFile.copyToOutputStream(fileOutputStream, this._onChunkTransferred.bind(this));
        if (error)
          Common.console.error('Failed to read heap snapshot from temp file: ' + error.message);
        this._didCompleteSnapshotTransfer();
        return;
      }
      this._onTempFileReady = onOpen.bind(this, accepted);
      this._updateSaveProgress(0, 1);
    }
  }

  /**
   * @param {!Bindings.ChunkedReader} reader
   */
  _onChunkTransferred(reader) {
    this._updateSaveProgress(reader.loadedSize(), reader.fileSize());
  }

  /**
   * @param {number} value
   * @param {number} total
   */
  _updateSaveProgress(value, total) {
    const percentValue = ((total && value / total) * 100).toFixed(0);
    this.updateStatus(Common.UIString('Saving\u2026 %d%%', percentValue));
  }

  /**
   * @override
   * @param {!File} file
   * @return {!Promise<?Error>}
   */
  async loadFromFile(file) {
    this.updateStatus(Common.UIString('Loading\u2026'), true);
    this._setupWorker();
    const reader = new Bindings.ChunkedFileReader(file, 10000000);
    const success = await reader.read(/** @type {!Common.OutputStream} */ (this._receiver));
    if (!success)
      this.updateStatus(reader.error().message);
    return success ? null : reader.error();
  }
};

/**
 * @unrestricted
 */
Profiler.HeapSnapshotStatisticsView = class extends UI.VBox {
  constructor() {
    super();
    this.element.classList.add('heap-snapshot-statistics-view');
    this._pieChart = new PerfUI.PieChart(150, Profiler.HeapSnapshotStatisticsView._valueFormatter, true);
    this._pieChart.element.classList.add('heap-snapshot-stats-pie-chart');
    this.element.appendChild(this._pieChart.element);
    this._labels = this.element.createChild('div', 'heap-snapshot-stats-legend');
  }

  /**
   * @param {number} value
   * @return {string}
   */
  static _valueFormatter(value) {
    return Common.UIString('%s KB', Number.withThousandsSeparator(Math.round(value / 1024)));
  }

  /**
   * @param {number} value
   */
  setTotal(value) {
    this._pieChart.setTotal(value);
  }

  /**
   * @param {number} value
   * @param {string} name
   * @param {string=} color
   */
  addRecord(value, name, color) {
    if (color)
      this._pieChart.addSlice(value, color);

    const node = this._labels.createChild('div');
    const swatchDiv = node.createChild('div', 'heap-snapshot-stats-swatch');
    const nameDiv = node.createChild('div', 'heap-snapshot-stats-name');
    const sizeDiv = node.createChild('div', 'heap-snapshot-stats-size');
    if (color)
      swatchDiv.style.backgroundColor = color;
    else
      swatchDiv.classList.add('heap-snapshot-stats-empty-swatch');
    nameDiv.textContent = name;
    sizeDiv.textContent = Profiler.HeapSnapshotStatisticsView._valueFormatter(value);
  }
};

Profiler.HeapAllocationStackView = class extends UI.Widget {
  /**
   * @param {?SDK.HeapProfilerModel} heapProfilerModel
   */
  constructor(heapProfilerModel) {
    super();
    this._heapProfilerModel = heapProfilerModel;
    this._linkifier = new Components.Linkifier();
  }

  /**
   * @param {!Profiler.HeapSnapshotProxy} snapshot
   * @param {number} snapshotNodeIndex
   */
  async setAllocatedObject(snapshot, snapshotNodeIndex) {
    this.clear();
    const frames = await snapshot.allocationStack(snapshotNodeIndex);

    if (!frames) {
      const stackDiv = this.element.createChild('div', 'no-heap-allocation-stack');
      stackDiv.createTextChild(Common.UIString(
          'Stack was not recorded for this object because it had been allocated before this profile recording started.'));
      return;
    }

    const stackDiv = this.element.createChild('div', 'heap-allocation-stack');
    for (const frame of frames) {
      const frameDiv = stackDiv.createChild('div', 'stack-frame');
      const name = frameDiv.createChild('div');
      name.textContent = UI.beautifyFunctionName(frame.functionName);
      if (!frame.scriptId)
        continue;
      const urlElement = this._linkifier.linkifyScriptLocation(
          this._heapProfilerModel ? this._heapProfilerModel.target() : null, String(frame.scriptId), frame.scriptName,
          frame.line - 1, frame.column - 1);
      frameDiv.appendChild(urlElement);
    }
  }

  clear() {
    this.element.removeChildren();
    this._linkifier.reset();
  }
};
